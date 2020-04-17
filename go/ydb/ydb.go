package ydb

/*
#cgo LDFLAGS: -lydb -lyaml
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ylog.h>
#include <ydb.h>

typedef struct {
	int buflen;
	void *buf;
} bufinfo;

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

static ydb_res ydb_parses_wrapper(ydb *datablock, void *d, size_t dlen)
{
	return ydb_parses(datablock, (char *)d, dlen);
}

// The return must be free.
static bufinfo ydb_path_fprintf_wrapper(ydb *datablock, void *path)
{
    FILE *fp;
    char *buf = NULL;
	size_t buflen = 0;
	fp = open_memstream(&buf, &buflen);
	if (fp)
	{
		int n;
		n = ydb_path_fprintf(fp, datablock, "%s", path);
		fclose(fp);
	}
	bufinfo bi;
	bi.buflen = buflen;
	bi.buf = (void *) buf;
	return bi;
}

*/
import "C"
import (
	"errors"
	"fmt"
	"io"
	"io/ioutil"
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

// Size - Get the number of children
func (node *YNode) Size() int {
	switch node.Value.(type) {
	case map[string]interface{}:
		return len(node.Value.(map[string]interface{}))
	case []interface{}:
		return len(node.Value.([]interface{}))
	default:
		return 0
	}
}

// Find - Find a child from the current YNode
func (node *YNode) Find(key string) *YNode {
	switch node.Value.(type) {
	case map[string]interface{}:
		children := node.Value.(map[string]interface{})
		child, ok := children[key]
		if ok {
			return child.(*YNode)
		}
		return nil
	case []interface{}: // unable to find a key due to no key.
		return nil
	default:
		return nil
	}
}

// GetParent - Get the parent YNode
func (node *YNode) GetParent() *YNode {
	return node.Parent
}

// GetChildKeys - Get all child keys
func (node *YNode) GetChildKeys() []string {
	switch node.Value.(type) {
	case map[string]interface{}:
		len := len(node.Value.(map[string]interface{}))
		children := make([]string, 0, len)
		for childkey := range node.Value.(map[string]interface{}) {
			children = append(children, childkey)
		}
		return children
	// case []interface{}: // not support key lists in YAML sequence (!!seq)
	default:
		return make([]string, 0, 0)
	}
}

// GetChildren - Get all child YNodes
func (node *YNode) GetChildren() []*YNode {
	switch node.Value.(type) {
	case map[string]interface{}:
		len := len(node.Value.(map[string]interface{}))
		children := make([]*YNode, 0, len)
		for _, child := range node.Value.(map[string]interface{}) {
			children = append(children, child.(*YNode))
		}
		return children
	case []interface{}:
		len := len(node.Value.([]interface{}))
		children := make([]*YNode, 0, len)
		for _, child := range node.Value.([]interface{}) {
			children = append(children, child.(*YNode))
		}
		return children
	default:
		return make([]*YNode, 0, 0)
	}
}

// GetMap - Get all child YNodes
func (node *YNode) GetMap() map[string]interface{} {
	switch node.Value.(type) {
	case map[string]interface{}:
		return node.Value.(map[string]interface{})
	default:
		return make(map[string]interface{}, 0)
	}
}

// Lookup - Get the value of the key named node.
func (node *YNode) Lookup(key string) string {
	child := node.Find(key)
	if child != nil {
		return child.GetValue()
	}
	return ""
}

// GetTag - Get the current YNode YAML Tag
func (node *YNode) GetTag() string {
	return node.Tag
}

// GetKey - Get the current YNode YAML Key
func (node *YNode) GetKey() string {
	return node.Key
}

// GetValue - Get the current YNode YAML value
func (node *YNode) GetValue() string {
	switch node.Value.(type) {
	case string:
		return node.Value.(string)
	default:
		return ""
	}
}

// YNode YAML Data Block node
type YNode struct {
	Parent *YNode
	Tag    string
	Key    string
	Value  interface{}
}

type retrieveOption struct {
	keys  []string
	all   bool
	depth int
	user interface{}
}

// RetrieveOption - The option to retrieve YNodes from an YDB instance.
type RetrieveOption func(*retrieveOption)

// func ordering(o func(cell, cell) bool) RetrieveOption {
//     return func(s *retrieveOption) { s.ordering = o }
// }

// RetrieveStruct - The option to set the depth of the YDB retrieval
func RetrieveStruct(user interface{}) RetrieveOption {
	return func(s *retrieveOption) { s.user = user}
}

// RetrieveDepth - The option to set the depth of the YDB retrieval
func RetrieveDepth(d int) RetrieveOption {
	return func(s *retrieveOption) { s.depth = d }
}

// RetrieveAll - The option to retrieve all descendants
func RetrieveAll() RetrieveOption {
	return func(s *retrieveOption) { s.all = true }
}

// RetrieveKeys - The option to set the start point of the retrieval
func RetrieveKeys(k ...string) RetrieveOption {
	return func(s *retrieveOption) { s.keys = k }
}

// Updater interface to update user structure
type Updater interface {
	Create(keys []string, key string, tag string, value string)
	Replace(keys []string, key string, tag string, value string)
	Delete(keys []string, key string)
}

// EmptyUpdater - Empty Updater interface
type EmptyUpdater struct{}

// Create - Interface to create an entity on !!map object
func (updater *EmptyUpdater) Create(keys []string, key string, tag string, value string) {
	log.Println("Node.Create", keys, key, tag, value)
}

// Replace - Interface to replace the entity on !!map object
func (updater *EmptyUpdater) Replace(keys []string, key string, tag string, value string) {
	log.Println("Node.Replace", keys, key, tag, value)
}

// Delete - Interface to delete the entity from !!map object
func (updater *EmptyUpdater) Delete(keys []string, key string) {
	log.Println("Node.Delete", keys, key)
}

//export handle
// handle -- Update YAML Data Block instance
func handle(ygo unsafe.Pointer, op C.char, cur *C.ynode, new *C.ynode) {
	var db *YDB = (*YDB)(ygo)
	var n *C.ynode
	var keys = make([]string, 0, 0)
	if new != nil {
		n = new
	} else {
		n = cur
	}
	for n := n.up(); n != nil; n = n.up() {
		key := n.key()
		if key != "" {
			keys = append([]string{key}, keys...)
		}
	}
	// log.Println(keys)
	switch int(op) {
	case 'c':
		db.Updater.Create(keys, n.key(), n.tag(), n.value())
	case 'r':
		db.Updater.Replace(keys, n.key(), n.tag(), n.value())
	case 'd':
		db.Updater.Delete(keys, n.key())
	}
}

// SetLog configures the log level of YDB
func SetLog(loglevel uint) {
	C.ylog_level = C.uint(loglevel)
}

// YDB (YAML YNode type) to indicate an YDB instance
type YDB struct {
	block   *C.ydb
	mutex   sync.Mutex
	fd      int
	Name    string
	Updater Updater
}

// Retrieve - Retrieve the data that consists of YNodes.
func (db *YDB) Retrieve(options ...RetrieveOption) *YNode {
	var node, parent *YNode
	var opt retrieveOption
	for _, o := range options {
		o(&opt)
	}
	db.mutex.Lock()
	defer db.mutex.Unlock()
	n := db.top()
	node = n.createYNode(nil)
	// log.Println(opt.keys)
	if len(opt.keys) > 0 {
		for _, key := range opt.keys {
			parent = node
			n = n.find(key)
			if n == nil {
				return nil
			}
			node = n.createYNode(parent)
		}
	}
	if opt.all || opt.depth > 0 {
		parent = node
		for n := n.down(); n != nil; n = n.next() {
			n.addYnode(parent, opt.depth-1, opt.all)
		}
	}
	return node
}

// Convert - Convert the YDB data to the target struct.
func (db *YDB) Convert(options ...RetrieveOption) interface{} {
	var user interface{}
	var opt retrieveOption
	for _, o := range options {
		o(&opt)
	}
	user = opt.user
	
	db.mutex.Lock()
	defer db.mutex.Unlock()
	// n := db.top()
	// v := reflect.ValueOf(user)
	// t := reflect.TypeOf(user)
	// if len(opt.keys) > 0 {
	// 	for _, key := range opt.keys {
	// 		parent = node
	// 		n = n.find(key)
	// 		if n == nil {
	// 			return nil
	// 		}
	// 		node = n.createYNode(parent)
	// 	}
	// }

	return user
}

// Close the YDB instance
func (db *YDB) Close() {
	db.mutex.Lock()
	defer db.mutex.Unlock()
	if db.block != nil {
		// C.ydb_unregister(db.block)
		C.ydb_close(db.block)
		db.block = nil
	}
}

// Open an YDB instance with a name
// name: The name of the creating YDB instance
// top: The go structure instance synced with the YDB instance
func Open(name string) (*YDB, func()) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	db := YDB{Name: name, block: C.ydb_open(cname), Updater: &EmptyUpdater{}}
	C.ydb_register(db.block, unsafe.Pointer(&db.block))
	return &db, func() {
		db.Close()
	}
}

// Connect to YDB IPC (Inter Process Communication) channel
func (db *YDB) Connect(addr string, flags ...string) error {
	var gflags string
	if db.block == nil {
		return fmt.Errorf("no instance exists")
	}
	for _, flag := range flags {
		gflags = gflags + ":" + flag
	}

	caddr := C.CString(addr)
	cflags := C.CString(gflags)
	defer C.free(unsafe.Pointer(caddr))
	defer C.free(unsafe.Pointer(cflags))
	db.mutex.Lock()
	defer db.mutex.Unlock()
	res := C.ydb_connect(db.block, caddr, cflags)
	if res == C.YDB_OK {
		return nil
	}
	return fmt.Errorf("%s", C.GoString(C.ydb_res_str(res)))
}

// Disconnect to YDB IPC channel
func (db *YDB) Disconnect(addr string) error {
	caddr := C.CString(addr)
	defer C.free(unsafe.Pointer(caddr))
	db.mutex.Lock()
	defer db.mutex.Unlock()
	res := C.ydb_disconnect(db.block, caddr)
	if res == C.YDB_OK {
		return nil
	}
	return fmt.Errorf("%s", C.GoString(C.ydb_res_str(res)))
}

// Serve --
// Run Serve() in the main loop if YDB IPC channel is used.
// Serve() updates the local YDB instance using the received YAML data from remotes.
func (db *YDB) Serve() {
	go db.Receive()
}

// Receive --
// Receive the YAML data from remotes.
func (db *YDB) Receive() error {
	var rfds unix.FdSet
	if db.fd > 0 {
		// log.Println("serve() already running")
		return nil
	}
	for {
		db.fd = int(C.ydb_fd(db.block))
		if db.fd <= 0 {
			err := errors.New(C.GoString(C.ydb_res_str(C.YDB_E_CONN_FAILED)))
			log.Printf("serve:%v", err)
			return err
		}
		rfds.Set(db.fd)
		n, err := unix.Select(db.fd+1, &rfds, nil, nil, nil)
		if err != nil {
			log.Printf("serve:%v", err)
			db.fd = 0
			return err
		}
		if n == 1 && rfds.IsSet(db.fd) {
			rfds.Clear(db.fd)
			db.mutex.Lock()
			defer db.mutex.Unlock()
			res := C.ydb_serve(db.block, C.int(0))
			if res >= C.YDB_ERROR {
				err = fmt.Errorf("%s", C.GoString(C.ydb_res_str(res)))
				log.Printf("serve:%v", err)
				db.fd = 0
				return err
			}
		}
	}
}

// A Decoder reads and decodes YAML values from an input stream to an YDB instance.
type Decoder struct {
	db  *YDB
	r   io.Reader
	err error
}

// NewDecoder returns a new decoder that reads from r.
//
// The decoder introduces its own buffering and may
// read data from r beyond the YAML values requested.
func (db *YDB) NewDecoder(r io.Reader) *Decoder {
	return &Decoder{r: r, db: db}
}

// Decode reads the YAML value from its
// input and stores it into an YDB instance.
func (dec *Decoder) Decode() error {
	if dec.err != nil {
		return dec.err
	}
	byt, err := ioutil.ReadAll(dec.r)
	if err == nil {
		res := C.ydb_parses_wrapper(dec.db.block, unsafe.Pointer(&byt[0]), C.ulong(len(byt)))
		if res >= C.YDB_ERROR {
			dec.err = err
			return fmt.Errorf("%s", C.GoString(C.ydb_res_str(res)))
		}
	} else {
		dec.err = err
	}
	return err
}

// An Encoder writes JSON values to an output stream.
type Encoder struct {
	db  *YDB
	w   io.Writer
	err error
}

// NewEncoder returns a new encoder that writes to w.
func (db *YDB) NewEncoder(w io.Writer) *Encoder {
	return &Encoder{w: w, db: db}
}

// Encode writes the YAML data of an YDB instance to the output stream.
func (enc *Encoder) Encode() error {
	if enc.err != nil {
		return enc.err
	}
	path := byte(0)
	cptr := C.ydb_path_fprintf_wrapper(enc.db.block, unsafe.Pointer(&path))
	log.Printf("%T %d", cptr, cptr.buflen)
	if cptr.buf != nil {
		defer C.free(unsafe.Pointer(cptr.buf))
		byt := C.GoBytes(unsafe.Pointer(cptr.buf), cptr.buflen)
		_, err := enc.w.Write(byt)
		return err
	}
	return fmt.Errorf("empty path printf in ydb")
}

func (db *YDB) top() *C.ynode {
	return C.ydb_top(db.block)
}

// new - create new YNode based on *C.ynode
func (n *C.ynode) createYNode(parent *YNode) *YNode {
	node := new(YNode)
	node.Tag = n.tag()
	node.Key = n.key()
	switch node.Tag {
	case "!!map", "!!imap", "!!set", "!!omap":
		node.Value = make(map[string]interface{})
	case "!!seq":
		node.Value = make([]interface{}, 0, 0)
	default:
		node.Value = n.value()
	}
	if parent != nil {
		val := reflect.ValueOf(parent.Value)
		typ := reflect.TypeOf(parent.Value)
		switch typ.Kind() {
		case reflect.Map:
			key, elem := reflect.ValueOf(node.Key), reflect.ValueOf(node)
			keytyp := typ.Key()
			if !key.Type().ConvertibleTo(keytyp) {
				log.Fatalln("Invalid map key type -", node)
				return nil
			}
			key = key.Convert(keytyp)
			elemtyp := typ.Elem()
			// log.Printf("key=%s, elem=%s", key, elem)
			// log.Printf("keytyp=%s, elemtyp=%v", keytyp, elemtyp)
			if elemtyp.Kind() != val.Kind() && !elem.Type().ConvertibleTo(elemtyp) {
				log.Fatalln("Invalid element type -", node)
				return nil
			}
			val.SetMapIndex(key, elem.Convert(elemtyp))
		case reflect.Slice:
			parentval := reflect.ValueOf(parent).Elem()
			parentFieldVal := parentval.FieldByName("Value")
			// log.Println("parentFieldVal", parentFieldVal.CanSet())
			newslice := reflect.Append(val, reflect.ValueOf(node))
			parentFieldVal.Set(newslice)
		}
		node.Parent = parent
	}
	// log.Println(node)
	return node
}

// addYnode - Add child node to the parent YNode
func (n *C.ynode) addYnode(parent *YNode, depth int, all bool) *YNode {
	node := n.createYNode(parent)
	if all || depth > 0 {
		parent = node
		for n := n.down(); n != nil; n = n.next() {
			n.addYnode(parent, depth-1, all)
		}
	}
	return node
}

func (n *C.ynode) empty() bool {
	empty := int(C.ydb_empty(n))
	if empty == 0 {
		return false
	}
	return true
}

func (n *C.ynode) size() int {
	return int(C.ydb_size(n))
}

func (n *C.ynode) find(key string) *C.ynode {
	k := C.CString(key)
	defer C.free(unsafe.Pointer(k))
	return C.ydb_find_child(n, k)
}

func (n *C.ynode) up() *C.ynode {
	return C.ydb_up(n)
}

func (n *C.ynode) down() *C.ynode {
	return C.ydb_down(n)
}

func (n *C.ynode) next() *C.ynode {
	return C.ydb_next(n)
}

func (n *C.ynode) prev() *C.ynode {
	return C.ydb_prev(n)
}

func (n *C.ynode) first() *C.ynode {
	return C.ydb_first(n)
}

func (n *C.ynode) last() *C.ynode {
	return C.ydb_last(n)
}

func (n *C.ynode) tag() string {
	return C.GoString(C.ydb_tag(n))
}

func (n *C.ynode) key() string {
	return C.GoString(C.ydb_key(n))
}

func (n *C.ynode) value() string {
	return C.GoString(C.ydb_value(n))
}