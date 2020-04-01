package ydb

/*
#cgo LDFLAGS: -lydb -lyaml
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ylog.h>
#include <ydb.h>

extern void handle(void *p, char op, ynode *old, ynode *cur);
static void ydb_hook(ydb *datablock, char op, ynode *_base, ynode *_cur, ynode *_new, void *U1)
{
	handle(U1, op, _cur, _new);
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
	"reflect"

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
)

type (
	Datablock map[string]interface{}
	Map map[string]interface{}
	IntegerMap map[int]string
	Seq []interface{}
	Set map[string]string
	// Str string
	// Int int64
	// Bool bool
)

type Handler interface {
	// Handler(op int, keys []string, tag string, value string)
	Create(keys []string, key string, tag string, value string)
	Replace(keys []string, key string, cTag string, cVal string, nTag string, nVal string)
	Delete(keys []string, key string)
}



// // Create - Interface to create an entity to the Datablock
// func (data *Datablock) Create(keys []string, tag string, value string) {
// 	log.Println("Create", keys, tag, value)
// 	datablock := data
// 	if len(keys) >= 1 {
// 		if (*datablock)[keys[0]] != nil {
// 			// if (*datablock)[keys[0]].(type) != *Datablock {
// 			// }
// 			// switch (*datablock)[keys[0]].(type) {
// 			// case string:
// 			// case *Datablock:

// 			// }
// 			datablock = (*datablock)[keys[0]].(*Datablock)
// 			datablock.Create(keys[1:], tag, value)
// 			log.Println(reflect.ValueOf(datablock), (*datablock)[keys[0]])
// 		} else {
// 			switch tag {
// 			case "!!map":
// 				subblock := make(Datablock)
// 				(*datablock)[keys[0]] = &subblock
// 			default:
// 				(*datablock)[keys[0]] = value
// 			}
// 		}
// 	}
// }

// // Replace - Interface to replace the entity in the Datablock
// func (data *Datablock) Replace(keys []string, cTag string, cVal string, nTag string, nVal string) {
// 	log.Println("Replace", keys, cTag, cVal, nTag, nVal)
// 	datablock := data
// 	if len(keys) > 1 {
// 		if (*datablock)[keys[0]] != nil {
// 			datablock = (*datablock)[keys[0]].(*Datablock)
// 			datablock.Delete(keys[1:])
// 			// log.Println(reflect.ValueOf(datablock), (*datablock)[keys[0]])
// 		}
// 	} else if len(keys) == 1 {
// 		delete(*datablock, keys[0])
// 	}
// }

// // Delete - Interface to delete the entity from the Datablock
// func (data *Datablock) Delete(keys []string) {
// 	log.Println("Delete", keys)
// 	datablock := data
// 	if len(keys) > 1 {
// 		if (*datablock)[keys[0]] != nil {
// 			datablock = (*datablock)[keys[0]].(*Datablock)
// 			datablock.Delete(keys[1:])
// 			log.Println(reflect.ValueOf(datablock), (*datablock)[keys[0]])
// 		}
// 	} else if len(keys) == 1 {
// 		delete(*datablock, keys[0])
// 	}
// }

func newNode(tag string, value string) interface{} {
	log.Printf("new(%s %s)\n", tag, value)
	switch tag {
	case "!!map":
		node := make(Map)
		return node
	// case "!!seq":
	// 	return &make(Seq)
	default:
		return value
	}
}

// Create - Interface to create an entity to the Datablock
func (m *Map) Create(keys []string, key string, tag string, value string) {
	log.Println("Create", keys, key, tag, value)
	if keys != nil && len(keys) > 0 {
		if (*m)[keys[0]] == nil {
			m.Create([]string{}, keys[0], "!!map", "")
		}
		switch (*m)[keys[0]].(type) {
		case Map:
			n := (*m)[keys[0]].(Map)
			n.Create(keys[1:], key, tag, value)
		default:
			panic("Not supported type")
		// case *Seq:
		// 	n := (*m)[keys[0]].(*Seq)
		// 	n.Create(keys[1:], key, tag, value)
		}
	} else {
		(*m)[key] = newNode(tag, value)
	}
}

// Replace - Interface to replace the entity in the Datablock
func (m *Map) Replace(keys []string, key string, cTag string, cVal string, nTag string, nVal string) {
	log.Println("Replace", keys, key, cTag, cVal, nTag, nVal)
	if keys != nil && len(keys) > 0 {
		if (*m)[keys[0]] == nil {
			m.Create([]string{}, keys[0], "!!map", "")
		}
		switch (*m)[keys[0]].(type) {
		case Map:
			n := (*m)[keys[0]].(Map)
			n.Replace(keys[1:], key, cTag, cVal, nTag, nVal)
		}
	} else {
		(*m)[key] = newNode(nTag, nVal)
		log.Println("Replaced", reflect.ValueOf(m), (*m)[key])
	}
}

// Delete - Interface to delete the entity from the Datablock
func (m *Map) Delete(keys []string, key string) {
	log.Println("Delete", keys, key)
	if keys != nil && len(keys) > 0 {
		if (*m)[keys[0]] != nil {
			switch (*m)[keys[0]].(type) {
			case Map:
				n := (*m)[keys[0]].(Map)
				n.Delete(keys[1:], key)
			default:
				panic("Not supported type")
			}
		}
	} else {
		delete(*m, key)
	}
}

// Search the data in the keys
// func Search(data *Datablock, keys []string) interface{} {
// 	var n interface{} = data
// 	log.Println(data)
// 	// log.Println(len(keys))
// 	for _, key := range keys {
// 		log.Println(key, reflect.ValueOf(n))
// 		// v := reflect.ValueOf(n).Elem()
// 		// typ := v.Type()
// 		// switch typ.Kind() {
// 		// case reflect.Map:

// 		// 	// log.Printf("Map Type:\t%v %s\n", reflect.MapOf(typ.Key(), typ.Elem()), reflect.ValueOf(n).Elem().MapIndex(key))
// 		// case reflect.Invalid:
// 		// 	log.Println("invalid")
// 		// case reflect.Int, reflect.Int8, reflect.Int16,
// 		// 	reflect.Int32, reflect.Int64:
// 		// 	log.Println(strconv.FormatInt(v.Int(), 10))
// 		// case reflect.Uint, reflect.Uint8, reflect.Uint16,
// 		// 	reflect.Uint32, reflect.Uint64, reflect.Uintptr:
// 		// 	log.Println(strconv.FormatUint(v.Uint(), 10))
// 		// // ...floating-point and complex cases omitted for brevity...
// 		// case reflect.Bool:
// 		// 	log.Println(strconv.FormatBool(v.Bool()))
// 		// case reflect.String:
// 		// 	log.Println(strconv.Quote(v.String()))
// 		// case reflect.Chan, reflect.Func, reflect.Ptr, reflect.Slice:
// 		// 	log.Println(v.Type().String() + " 0x" + strconv.FormatUint(uint64(v.Pointer()), 16))
// 		// default: // reflect.Array, reflect.Struct, reflect.Interface
// 		// 	log.Println(v.Type().String() + " value")
// 		// }
// 	}
// 	return nil
// }

//export handle
// handle -- Update YAML Data Block instance
func handle(ygo unsafe.Pointer, op C.char, cur *C.ynode, new *C.ynode) {
	var y *YDB = (*YDB)(ygo)
	var node *C.ynode
	var keys = make([]string, 0, int(C.YDB_LEVEL_MAX))
	if new != nil {
		node = new
	} else {
		node = cur
	}
	for n := C.ydb_up(node); n != nil; n = C.ydb_up(n) {
		key := C.GoString(C.ydb_key(n))
		if key != "" {
			keys = append([]string{key}, keys...)
		}
	}
	switch int(op) {
	case 'c':
		// log.Println("len", len(keys))
		y.Top.Create(keys, C.GoString(C.ydb_key(node)), C.GoString(C.ydb_tag(node)), C.GoString(C.ydb_value(node)))
		// for index, key range keys {
		// }
	case 'r':
		y.Top.Replace(keys, C.GoString(C.ydb_key(node)),
			C.GoString(C.ydb_tag(cur)), C.GoString(C.ydb_value(cur)),
			C.GoString(C.ydb_tag(new)), C.GoString(C.ydb_value(new)))
	case 'd':
		y.Top.Delete(keys, C.GoString(C.ydb_key(node)))
	}
}

// SetLog configures the log level of YDB
func SetLog(loglevel uint) {
	C.ylog_level = C.uint(loglevel)
}

// YDB (YAML Datablock type) to indicate an YDB instance
type YDB struct {
	block *C.ydb
	Name  string
	Top   Handler
	mutex sync.Mutex
	fd    int
}

// Close the YDB instance
func (y *YDB) Close() {
	y.mutex.Lock()
	log.Println("Top:", y.Top)
	if y.block != nil {
		// C.ydb_unregister(y.block)
		C.ydb_close(y.block)
		y.block = nil
	}
	log.Println("Top:", y.Top)
	y.mutex.Unlock()
}

// Open an YDB instance with a name
// name: The name of the creating YDB instance
// top: The go structure instance synced with the YDB instance
func Open(name string, top Handler) (*YDB, func()) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	y := YDB{Name: name, block: C.ydb_open(cname)}
	if top == nil {
		y.Top = &	Map{}
		y.Top.Create([]string{}, name, "!!map", "")
	} else {
		y.Top = top
	}
	C.ydb_register(y.block, unsafe.Pointer(&y.block))
	return &y, func() {
		y.Close()
	}
}

// Connect to YDB IPC (Inter Process Communication) channel
func (y *YDB) Connect(addr string, flags ...string) error {
	var gflags string
	if y.block == nil {
		return fmt.Errorf("no instance exists")
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
	return fmt.Errorf("%s", C.GoString(C.ydb_res_str(res)))
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
	return fmt.Errorf("%s", C.GoString(C.ydb_res_str(res)))
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

// Scanf -- Read the date from ydb such as the scanf() (YAML format)
// ydb_read() only fills the scalar value of the YAML mapping or list nodes.
// And it returns the number of found values.
//  - ydb_read(datablock, "key: %s\n"); // ok.
//  - ydb_read(datablock, "%s: value\n"); // not allowed.
func (y *YDB) Scanf(format string, a ...interface{}) (n int, err error) {
	// os.Stdout
	// C.ydb_fprintf
	return 0, nil
}
