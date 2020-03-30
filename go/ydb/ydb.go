package ydb

/*
#cgo LDFLAGS: -lydb -lyaml
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ylog.h>
#include <ydb.h>

extern void updateYdb(void *p, char op, ynode *node);
static void ydb_hook(ydb *datablock, char op, ynode *_base, ynode *_cur, ynode *_new, void *U1)
{
	switch (op)
	{
	case 'c':
		updateYdb(U1, op, _new);
		break;
	case 'r':
		updateYdb(U1, op, _new);
		break;
	case 'd':
		updateYdb(U1, op, _cur);
		break;
	}
}

static void ydb_register(ydb *datablock, void *U1)
{
	ydb_res res;
	res = ydb_write_hook_add(datablock, "/", 0, ydb_hook, 1, U1);
}

static void ydb_unregister(ydb *datablock)
{
	ydb_write_hook_delete(datablock, "/");
}

*/
import "C"
import (
	"errors"
	"fmt"
	"log"

	"sync"
	"unsafe"

	"golang.org/x/sys/unix"
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

	// OpCreate for data creation
	OpCreate = 'c'
	// OpReplace for data update
	OpReplace = 'r'
	// OpDelete for data deletion
	OpDelete = 'd'
)

type (
	Datablock map[string]interface{}
)

type Update interface {
	Update(op int, keys []string, tag string, value string)
}

func (d *Datablock) Update(op int, keys []string, tag string, value string) {
	switch op {
	case OpCreate:
		fmt.Println("create", keys, tag, value)
		// for index, key range keys {

		// }
	case OpReplace:
		fmt.Println("replace", keys, tag, value)
	case OpDelete:
		fmt.Println("Delete", keys, tag, value)
	}

}

//export updateYdb
// updateYdb -- Update YAML Data Block instance
func updateYdb(ygo unsafe.Pointer, op C.char, node *C.ynode) {
	var y *YDB = (*YDB)(ygo)
	var keys = make([]string, int(C.YDB_LEVEL_MAX))
	for n := node; n != nil; n = C.ydb_up(n) {
		key := C.GoString(C.ydb_key(n))
		keys = append([]string{key}, keys...)
	}
	y.top.Update(int(op), keys, C.GoString(C.ydb_tag(node)), C.GoString(C.ydb_value(node)))
}

// SetLog configures the log level of YDB
func SetLog(loglevel uint) {
	C.ylog_level = C.uint(loglevel)
}

// YDB (YAML Datablock type) to indicate an YDB instance
type YDB struct {
	block *C.ydb
	Name  string
	top   *Datablock
	mutex sync.Mutex
	fd    int
}

// Close the YDB instance
func (y *YDB) Close() {
	y.mutex.Lock()
	if y.block != nil {
		// C.ydb_unregister(y.block)
		C.ydb_close(y.block)
		y.block = nil
	}
	y.mutex.Unlock()
}

// Open an YDB instance with a name
// name: The name of the creating YDB instance
// top: The go structure instance synced with the YDB instance
// recursive: The
func Open(name string, top *Datablock) (*YDB, func()) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	y := YDB{Name: name, block: C.ydb_open(cname), top: top}
	C.ydb_register(y.block, unsafe.Pointer(&y.block))
	return &y, func() {
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
	y.mutex.Lock()
	res := C.ydb_connect(y.block, caddr, cflags)
	y.mutex.Unlock()
	if res == C.YDB_OK {
		go y.serve()
		return nil
	}
	return errors.New(C.GoString(C.ydb_res_str(res)))
}

// Disconnect to YDB IPC channel
func (y *YDB) Disconnect(addr string) error {
	caddr := C.CString(addr)
	defer C.free(unsafe.Pointer(caddr))
	y.mutex.Lock()
	res := C.ydb_disconnect(y.block, caddr)
	y.mutex.Unlock()
	if res == C.YDB_OK {
		return nil
	}
	return errors.New(C.GoString(C.ydb_res_str(res)))
}

// Serve --
// Run Serve() in the main loop if YDB IPC channel is used.
// Serve() updates the local YDB instance using the received YAML data from remotes.
func (y *YDB) Serve() {
	go y.serve()
}

// serve --
// Receive the YAML data from remotes.
func (y *YDB) serve() error {
	var rfds unix.FdSet
	if y.fd > 0 {
		// log.Println("serve() already running")
		return nil
	}
	for {
		y.fd = int(C.ydb_fd(y.block))
		if y.fd <= 0 {
			err := errors.New(C.GoString(C.ydb_res_str(C.YDB_E_CONN_FAILED)))
			log.Printf("serve:%v", err)
			return err
		}
		rfds.Set(y.fd)
		n, err := unix.Select(y.fd+1, &rfds, nil, nil, nil)
		if err != nil {
			log.Printf("serve:%v", err)
			y.fd = 0
			return err
		}
		if n == 1 && rfds.IsSet(y.fd) {
			rfds.Clear(y.fd)
			y.mutex.Lock()
			res := C.ydb_serve(y.block, C.int(0))
			y.mutex.Unlock()
			if res >= C.YDB_ERROR {
				err = errors.New(C.GoString(C.ydb_res_str(res)))
				log.Printf("serve:%v", err)
				y.fd = 0
				return err
			}
		}
	}
}

// ydb_read --
// Read the date from ydb such as the scanf() (YAML format)
// ydb_read() only fills the scalar value of the YAML mapping or list nodes.
// And it returns the number of found values.
//  - ydb_read(datablock, "key: %s\n"); // ok.
//  - ydb_read(datablock, "%s: value\n"); // not allowed.
func (y *YDB) Scanf(format string, a ...interface{}) (n int, err error) {
	// os.Stdout
	// C.ydb_fprintf
	return 0, nil
}
