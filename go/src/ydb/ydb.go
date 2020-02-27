package ydb

/*
#cgo LDFLAGS: -lydb -lyaml
#include <stdio.h>
#include <stdlib.h>
#include <ylog.h>
#include <ydb.h>
extern int sum(int a, int b);

static inline void CExample() {
	int r = sum(1, 2);
	printf("%d\n", r);
}

extern void callbackInGo(void *p, int n);
static inline void callbackExample(void *p) {
	callbackInGo(p, 100);
}

*/
import "C"
import (
	"errors"
	"unsafe"
)

const (
	// LogDebug YDB log level
	LogDebug = C.YLOG_DEBUG
	// LogInout YDB log level
	LogInout = C.YLOG_INOUT
	// LogInfo YDB log level
	LogInfo = C.YLOG_INFO
	// LogWarn YDB log level
	LogWarn = C.YLOG_WARN
	// LogError YDB log level
	LogError = C.YLOG_ERROR
	// LogCritical YDB log level
	LogCritical = C.YLOG_CRITICAL
)

// SetLog configures the log level of YDB
func SetLog(loglevel uint) {
	C.ylog_level = C.uint(loglevel)
}

// YDB (YAML Datablock type) to indicate an YDB instance
type YDB struct {
	Name  string
	block *C.ydb
}

// Close the YDB instance
func (y *YDB) Close() {
	C.ydb_close(y.block)
	y.block = nil
}

// Open an YDB instance with a name
func Open(name string) (YDB, func()) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	y := YDB{Name: name, block: C.ydb_open(cname)}
	return y, func() {
		y.Close()
	}
}

// Connect to YDB IPC (Inter Process Communication) channel
func (y *YDB) Connect(addr string, flags ...string) error {
	var gflags string
	if y.block == nil {
		return errors.New("no instance exists")
	}
	for _, flag := range flags {
		gflags = gflags + ":" + flag
	}

	caddr := C.CString(addr)
	cflags := C.CString(gflags)
	defer C.free(unsafe.Pointer(caddr))
	defer C.free(unsafe.Pointer(cflags))
	res := C.ydb_connect(y.block, caddr, cflags)
	if res == C.YDB_OK {
		return nil
	}
	return errors.New(C.GoString(C.ydb_res_str(res)))
}

// Disconnect to YDB IPC channel
func (y *YDB) Disconnect(addr string) error {
	caddr := C.CString(addr)
	defer C.free(unsafe.Pointer(caddr))
	res := C.ydb_disconnect(y.block, caddr)
	if res == C.YDB_OK {
		return nil
	}
	return errors.New(C.GoString(C.ydb_res_str(res)))
}

// Serve --
// Run Serve() in the main loop if YDB IPC channel is used.
// Serve() updates the local YDB instance using the received YAML data from remotes.
func (y *YDB) Serve(msec int) error {
	res := C.ydb_serve(y.block, C.int(msec))
	if res == C.YDB_OK {
		return nil
	} else if res >= C.YDB_WARNING_MIN && res <= C.YDB_WARNING_MAX {
		return nil
	}
	return errors.New(C.GoString(C.ydb_res_str(res)))
}

// Service --
// Run Service() in the main loop if YDB IPC channel is used.
// Service() updates the local YDB instance using the received YAML data from remotes.
func (y *YDB) Service(msec int) error {

	res := C.ydb_serve(y.block, C.int(msec))
	if res == C.YDB_OK {
		return nil
	} else if res >= C.YDB_WARNING_MIN && res <= C.YDB_WARNING_MAX {
		return nil
	}
	return errors.New(C.GoString(C.ydb_res_str(res)))
}
