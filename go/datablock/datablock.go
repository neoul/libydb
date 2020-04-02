package datablock

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

func (node *C.ynode) Empty() bool {
	empty := int(C.ydb_empty(node))
	if empty == 0 {
		return false
	}
	return true
}

func (node *C.ynode) Size() int {
	return int(C.ydb_size(node))
}

func (node *C.ynode) Find(key string) *C.ynode {
	k := C.CString(key)
	defer C.free(unsafe.Pointer(k))
	return C.ydb_find_child(node, k)
}

func (node *C.ynode) Up() *C.ynode {
	return C.ydb_up(node)
}

func (node *C.ynode) Down() *C.ynode {
	return C.ydb_down(node)
}

func (node *C.ynode) Next() *C.ynode {
	return C.ydb_next(node)
}

func (node *C.ynode) Prev() *C.ynode {
	return C.ydb_prev(node)
}

func (node *C.ynode) First() *C.ynode {
	return C.ydb_first(node)
}

func (node *C.ynode) Last() *C.ynode {
	return C.ydb_last(node)
}

func (node *C.ynode) Tag() string {
	return C.GoString(C.ydb_tag(node))
}

func (node *C.ynode) Key() string {
	return C.GoString(C.ydb_key(node))
}

func (node *C.ynode) Value() string {
	return C.GoString(C.ydb_value(node))
}

type Datablock struct {
	Tag string
	Key string
	Value string
	// Children map[string]Datablock
	Children []Datablock
}

type retrieveOption struct {
	keys []string
	all bool
	depth int
}

type RetrieveOption func(*retrieveOption)
 
// func ordering(o func(cell, cell) bool) RetrieveOption {
//     return func(s *retrieveOption) { s.ordering = o }
// }
 
func RetrieveDepth(d int) RetrieveOption {
    return func(s *retrieveOption) { s.depth = d }
}

func RetrieveAll() RetrieveOption {
    return func(s *retrieveOption) { s.all = true }
}
 
func RetrieveKeys(k []string) RetrieveOption {
    return func(s *retrieveOption) { s.keys = k }
}

func (node *C.ynode) Retrieve(options ...RetrieveOption) Datablock {
	var opt retrieveOption
    for _, o := range options {
        o(&opt)
    }
	nodeinfo := Datablock{
		Tag: C.GoString(C.ydb_tag(node)),
		Key: C.GoString(C.ydb_key(node)),
		Value: C.GoString(C.ydb_value(node))}
	if opt.all || opt.depth > 0 {
		num := int(C.ydb_size(node))
		if num > 0 {
			children := make([]*C.ynode, 0, num)
			Children := make([]Datablock, 0, num)
			for n := C.ydb_down(node); n != nil; n = C.ydb_next(n) {
				children = append(children, n)
			}
			for _, child := range children {
				datablock := child.Retrieve(RetrieveAll(), RetrieveDepth(opt.depth-1))
				Children = append(Children, datablock)
			}
			nodeinfo.Children = Children
		}
	}
	return nodeinfo
}


// type Handler interface {
// 	// Handler(op int, keys []string, tag string, value string)
// 	Create(keys []string, key string, tag string, value string)
// 	Replace(keys []string, key string, tag string, value string)
// 	Delete(keys []string, key string)
// }

// // Create - Interface to create an entity on !!map object
// func (node *C.ynode) Create(keys []string, key string, tag string, value string) {
// 	log.Println("Node.Create", keys, key, tag, value)
// }

// // Replace - Interface to replace the entity on !!map object
// func (node *C.ynode) Replace(keys []string, key string, tag string, value string) {
// 	log.Println("Node.Replace", keys, key, tag, value)
// }

// // Delete - Interface to delete the entity from !!map object
// func (node *C.ynode) Delete(keys []string, key string) {
// 	log.Println("Node.Delete", keys, key)
// }

//export handle
// handle -- Update YAML Data Block instance
func handle(ygo unsafe.Pointer, op C.char, cur *C.ynode, new *C.ynode) {
	// var y *YDB = (*YDB)(ygo)
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
	// switch int(op) {
	// case 'c':
	// 	node.Create(keys, C.GoString(C.ydb_key(node)),
	// 		C.GoString(C.ydb_tag(node)), C.GoString(C.ydb_value(node)))
	// case 'r':
	// 	node.Replace(keys, C.GoString(C.ydb_key(node)),
	// 		C.GoString(C.ydb_tag(new)), C.GoString(C.ydb_value(new)))
	// case 'd':
	// 	node.Delete(keys, C.GoString(C.ydb_key(node)))
	// }
}

// SetLog configures the log level of YDB
func SetLog(loglevel uint) {
	C.ylog_level = C.uint(loglevel)
}

// YDB (YAML Datablock type) to indicate an YDB instance
type YDB struct {
	block *C.ydb
	Name  string
	mutex sync.Mutex
	fd    int
}

func (y *YDB) Top() *C.ynode {
	return C.ydb_top(y.block)
}

func (y *YDB) Retrieve(options ...RetrieveOption) Datablock {
	var opt retrieveOption
    for _, o := range options {
        o(&opt)
	}
	node := C.ydb_top(y.block)
	if len(opt.keys) > 0 {
		for _, key := range opt.keys {
			node = node.Find(key)
			if node == nil {
				return Datablock{}
			}
		}
	}
	nodeinfo := Datablock{
		Tag: C.GoString(C.ydb_tag(node)),
		Key: C.GoString(C.ydb_key(node)),
		Value: C.GoString(C.ydb_value(node))}
	if opt.all || opt.depth > 0 {
		num := int(C.ydb_size(node))
		if num > 0 {
			children := make([]*C.ynode, 0, num)
			Children := make([]Datablock, 0, num)
			for n := C.ydb_down(node); n != nil; n = C.ydb_next(n) {
				children = append(children, n)
			}
			for _, child := range children {
				datablock := child.Retrieve(RetrieveAll(), RetrieveDepth(opt.depth-1))
				Children = append(Children, datablock)
			}
			nodeinfo.Children = Children
		}
	}
	return nodeinfo
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
func Open(name string) (*YDB, func()) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	y := YDB{Name: name, block: C.ydb_open(cname)}
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
	go y.Receive()
}

// Receive --
// Receive the YAML data from remotes.
func (y *YDB) Receive() error {
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
