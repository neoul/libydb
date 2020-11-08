package ydb

/*
#cgo LDFLAGS: -lydb -lyaml
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ylog.h>
#include <ydb.h>

typedef struct {
	int buflen;
	void *buf;
} bufinfo;

extern void manipulate(void *p, char op, ynode *old, ynode *cur);
static void ydb_write_hooker(ydb *datablock, char op, ynode *_base, ynode *_cur, ynode *_new, void *U1)
{
	manipulate(U1, op, _cur, _new);
}

extern void updateStartEnd(void *p, int started);
static void ydb_onchange(ydb *datablock, int started, void *user)
{
	updateStartEnd(user, started);
}

static void ydb_write_hook_register(ydb *datablock, void *U1)
{
	ydb_onchange_hook_add(datablock, ydb_onchange, U1);
	ydb_write_hook_add(datablock, "/", 0, ydb_write_hooker	, 1, U1);
}

static void ydb_write_hook_unregister(ydb *datablock)
{
	ydb_write_hook_delete(datablock, "/");
	ydb_onchange_hook_delete(datablock);
}

extern void syncUpdate(void *p, char *path, FILE *stream);
static ydb_res ydb_read_hooker(ydb *datablock, char *path, FILE *stream, void *U1)
{
	syncUpdate(U1, path, stream);
	return YDB_OK;
}

static ydb_res ydb_read_hook_register(ydb *datablock, void *path, void *U1)
{
	return ydb_read_hook_add(datablock, (char *) path, (ydb_read_hook)ydb_read_hooker, 1, U1);
}

static void ydb_read_hook_unregister(ydb *datablock, void *path)
{
	ydb_read_hook_delete(datablock, (char *) path);
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
		if (path)
			n = ydb_path_fprintf(fp, datablock, "%s", path);
		else
			n = ydb_path_fprintf(fp, datablock, "/");
		fclose(fp);
	}
	bufinfo bi;
	bi.buflen = buflen;
	bi.buf = (void *) buf;
	return bi;
}

// The return must be free.
static bufinfo ydb_fprintf_wrapper(ydb *datablock, void *input_yaml)
{
    FILE *fp;
    char *buf = NULL;
	size_t buflen = 0;
	fp = open_memstream(&buf, &buflen);
	if (fp)
	{
		int n;
		if (input_yaml)
			n = ydb_fprintf(fp, datablock, "%s", input_yaml);
		else
			n = ydb_path_fprintf(fp, datablock, "/");
		fclose(fp);
	}
	bufinfo bi;
	bi.buflen = buflen;
	bi.buf = (void *) buf;
	return bi;
}

static bufinfo ydb_ynode2yaml_wrapper(ydb *datablock, ynode *node)
{
	int buflen = 0;
	char *buf = ydb_ynode2yaml(datablock, node, &buflen);
	bufinfo bi;
	bi.buflen = buflen;
	bi.buf = (void *) buf;
	return bi;
}

static ydb_res ydb_delete_wrapper(ydb *datablock, void *input_yaml)
{
	ydb_res res = YDB_OK;
	if (input_yaml)
		res = ydb_delete(datablock, "%s", input_yaml);
	return res;
}

static ydb_res ydb_sync_wrapper(ydb *datablock, void *input_yaml)
{
	ydb_res res = YDB_OK;
	if (input_yaml)
		res = ydb_sync(datablock, "%s", input_yaml);
	return res;
}

static ydb_res ydb_path_write_wrapper(ydb *datablock, void *path, void *data)
{
	if (path) {
		if (data)
			return ydb_path_write(datablock, "%s=%s", path, data);
		else
			return ydb_path_write(datablock, "%s=%s", path, "");
	}
	return YDB_OK;
}

static ydb_res ydb_path_delete_wrapper(ydb *datablock, void *path)
{
	if (path)
		return ydb_path_delete(datablock, "%s", path);
	return YDB_OK;
}

static char *ydb_path_read_wrapper(ydb *datablock, void *path)
{
	if (path)
		return (char *) ydb_path_read(datablock, "%s", path);
	return NULL;
}

static ydb_res ydb_path_sync_wrapper(ydb *datablock, void *path)
{
	if (path)
		return ydb_path_sync(datablock, "%s", path);
	return YDB_OK;
}

extern void ylogGo(int level, char *f, int line, char *buf, int buflen);
static int ylog_go(
    int level, const char *f, int line, const char *format, ...)
{
	FILE *fp;
    char *buf = NULL;
	size_t buflen = 0;
	fp = open_memstream(&buf, &buflen);
	if (fp)
	{
		va_list args;
		va_start(args, format);
		vfprintf(fp, format, args);
		va_end(args);
		fclose(fp);
		if (buflen > 0) {
			// remove tail \n
			if (buf[buflen - 1] == '\n')
				buflen--;
			ylogGo(level, (char *)f, line, buf, buflen);
		}
		if (!buf)
			free(buf);
		return buflen;
	}
	return 0;
}

static void ylog_init()
{
	ylog_register(ylog_go);
}

*/
import "C"
import (
	"bytes"
	"errors"
	"fmt"
	"io"
	"io/ioutil"
	"os"
	"os/signal"
	"reflect"
	"strings"
	"sync"
	"syscall"
	"time"
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

// Search - Search a child from the current YNode
func (node *YNode) Search(keys ...string) *YNode {
	found := node
	if len(keys) > 0 {
		for _, key := range keys {
			if found == nil {
				return nil
			}
			found = found.Find(key)
		}
	}
	return found
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
	keys   []string
	all    bool
	depth  int
	user   interface{}
	nolock bool
}

// RetrieveOption - The option to retrieve YNodes from an YDB instance.
type RetrieveOption func(*retrieveOption)

// func ordering(o func(cell, cell) bool) RetrieveOption {
//     return func(s *retrieveOption) { s.ordering = o }
// }

// RetrieveStruct - The option to set the depth of the YDB retrieval
func RetrieveStruct(user interface{}) RetrieveOption {
	return func(s *retrieveOption) { s.user = user }
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

// retrieveWithoutLock - The option to retrieve data without lock
func retrieveWithoutLock() RetrieveOption {
	return func(s *retrieveOption) { s.nolock = true }
}

// getUpdater from v to call the custom Updater interface.
func getUpdater(v reflect.Value, keys []string) (Updater, DataUpdate, []string) {
	var updater Updater = nil
	var dataUpdate DataUpdate = nil
	var newkey []string = keys
	if v.Type().NumMethod() > 0 && v.CanInterface() {
		if u, ok := v.Interface().(Updater); ok {
			updater = u
		} else if u, ok := v.Interface().(DataUpdate); ok {
			dataUpdate = u
		}
	}
	for i, key := range keys {
		var ok bool
		v, ok = ValFind(v, key, NoSearch)
		if !ok {
			return updater, dataUpdate, newkey
		}
		if v.Type().NumMethod() > 0 && v.CanInterface() {
			switch u := v.Interface().(type) {
			case Updater:
				updater = u
				dataUpdate = nil
			case DataUpdate:
				updater = nil
				dataUpdate = u
			}
		}
		newkey = keys[i+1:]
	}

	// if len(keys) > 0 {
	// 	fv := FindValue(v, keys...)
	// 	if !fv.IsValid() {
	// 		return updater, dataUpdate, newkey
	// 	}
	// 	v = fv
	// 	if v.Type().NumMethod() > 0 && v.CanInterface() {
	// 		if u, ok := v.Interface().(Updater); ok {
	// 			updater = u
	// 			dataUpdate = nil
	// 			newkey = []string{}
	// 		} else if u, ok := v.Interface().(DataUpdate); ok {
	// 			updater = nil
	// 			dataUpdate = u
	// 			newkey = []string{}
	// 		}
	// 	}
	// }
	return updater, dataUpdate, newkey
}

func yCreate(v reflect.Value, keys []string, key string, tag string, value string) error {
	log.Debugf("Node.Create(%v, %s, %s, %s, %s) {", v, keys, key, tag, value)
	err := ValYdbSet(v, keys, key, tag, value)
	log.Debug("}", err)
	return err
}

func yReplace(v reflect.Value, keys []string, key string, tag string, value string) error {
	log.Debugf("Node.Replace(%v, %s, %s, %s, %s) {", v, keys, key, tag, value)
	err := ValYdbSet(v, keys, key, tag, value)
	log.Debug("}", err)
	return err
}

// yDelete - constructs the non-updater struct
func yDelete(v reflect.Value, keys []string, key string) error {
	log.Debugf("Node.Delete(%v, %s, %s) {", v, keys, key)
	err := ValYdbUnset(v, keys, key)
	log.Debug("}", err)
	return err
}

func construct(target interface{}, op int, cur *C.ynode, new *C.ynode) error {
	var n *C.ynode
	var keys = make([]string, 0, 0)
	if new != nil {
		n = new
	} else {
		n = cur
	}
	for p := n.up(); p != nil; p = p.up() {
		key := p.key()
		if key != "" {
			keys = append([]string{key}, keys...)
		}
	}
	if len(keys) >= 1 {
		// log.Debug(keys, "==>", keys[1:])
		keys = keys[1:]
	} else {
		// ignore root node deletion.
		if op == 'd' {
			return nil
		}
		switch n.tag() {
		case "!!map", "!!seq", "!!imap", "!!omap", "!!set":
			return nil
		}
		v := reflect.ValueOf(target)
		rv := SetValue(v, n.value())
		if !rv.IsValid() {
			return fmt.Errorf("%c %s, %s, %s, %s: %s", op, keys, n.key(), n.tag(), n.value(), "Set failed")
		}
		return nil
	}
	v := reflect.ValueOf(target)
	updater, dataUpdate, newkeys := getUpdater(v, keys)
	if updater != nil {
		var err error = nil
		switch op {
		case 'c':
			err = updater.Create(newkeys, n.key(), n.tag(), n.value())
		case 'r':
			err = updater.Replace(newkeys, n.key(), n.tag(), n.value())
		case 'd':
			err = updater.Delete(newkeys, n.key())
		default:
			err = fmt.Errorf("unknown op")
		}
		if err != nil {
			return fmt.Errorf("%c %s, %s, %s, %s: %s", op, keys, n.key(), n.tag(), n.value(), err)
		}
	} else if dataUpdate != nil {
		var err error = nil
		path := fmt.Sprintf("%s/%s", strings.Join(newkeys, "/"), n.key())
		if !strings.HasPrefix(path, "/") {
			path = "/" + path
		}
		switch op {
		case 'c':
			err = dataUpdate.UpdateCreate(path, n.value())
		case 'r':
			err = dataUpdate.UpdateReplace(path, n.value())
		case 'd':
			err = dataUpdate.UpdateDelete(path)
		default:
			err = fmt.Errorf("unknown op")
		}
		if err != nil {
			return fmt.Errorf("%c %s, %s: %s", op, path, n.value(), err)
		}
	} else {
		var err error = nil
		switch op {
		case 'c':
			err = yCreate(v, keys, n.key(), n.tag(), n.value())
		case 'r':
			err = yReplace(v, keys, n.key(), n.tag(), n.value())
		case 'd':
			err = yDelete(v, keys, n.key())
		default:
			err = fmt.Errorf("unknown op")
		}
		if err != nil {
			return fmt.Errorf("%c %s, %s, %s, %s: %s", op, keys, n.key(), n.tag(), n.value(), err)
		}
	}
	return nil
}

//export manipulate
// manipulate -- Update YAML Data Block instance
func manipulate(ygo unsafe.Pointer, op C.char, cur *C.ynode, new *C.ynode) {
	var db *YDB = (*YDB)(ygo)
	if db.Target != nil {
		err := construct(db.Target, int(op), cur, new)
		if err != nil {
			db.Errors = append(db.Errors, err)
		}
	}
}

//export updateStartEnd
// updateStartEnd -- indicates the YDB update start and end.
func updateStartEnd(ygo unsafe.Pointer, started C.int) {
	var db *YDB = (*YDB)(ygo)
	if db.Target != nil {
		startEnd, ok := db.Target.(UpdaterStartEnd)
		if ok {
			if started != 0 {
				startEnd.UpdateStart()
			} else {
				startEnd.UpdateEnd()
			}
		}
	}
}

//export syncUpdate
// syncUpdate -- Update YAML Data Block instance upon ydb_sync request.
func syncUpdate(ygo unsafe.Pointer, path *C.char, stream *C.FILE) {
	var db *YDB = (*YDB)(ygo)
	if db.Target != nil {
		SyncUpdate, ok := db.Target.(SyncUpdater)
		if ok {
			var b []byte
			keylist, err := ToSliceKeys(C.GoString(path))
			if err != nil {
				return
			}
			if klen := len(keylist); klen > 0 {
				b = SyncUpdate.SyncUpdate(keylist[:klen-1], keylist[klen-1])
			} else {
				b = SyncUpdate.SyncUpdate(nil, "")
			}
			blen := len(b)
			if blen > 0 {
				C.fwrite(unsafe.Pointer(&b[0]), C.ulong(blen), 1, stream)
			}
		}
	}
}

// SetInternalLog configures the log level of YDB
func SetInternalLog(loglevel uint) {
	C.ylog_level = C.uint(loglevel)
}

// YDB (YAML YNode type) to indicate an YDB instance
type YDB struct {
	block *C.ydb
	sync.RWMutex
	fd     int
	Name   string
	Target interface{}
	Errors []error
	syncCtrl
}

// Retrieve - Retrieve the data that consists of YNodes.
func (db *YDB) Retrieve(options ...RetrieveOption) (*YNode, error) {
	var node, parent *YNode
	var opt retrieveOption
	for _, o := range options {
		o(&opt)
	}
	if opt.user != nil {
		return nil, fmt.Errorf("ydb.RetrieveStruct is used only for Convert()")
	}
	db.RLock()
	defer db.RUnlock()
	n := db.top()
	node = n.createYNode(nil)
	if len(opt.keys) > 0 {
		for _, key := range opt.keys {
			parent = node
			n = n.find(key)
			if n == nil {
				return nil, fmt.Errorf("not found (%s)", key)
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
	return node, nil
}

func convert(db *YDB, userStruct interface{}, op int, n *C.ynode) {
	err := construct(userStruct, op, n, nil)
	if err != nil {
		db.Errors = append(db.Errors, err)
	} else {
		for cn := n.down(); cn != nil; cn = cn.next() {
			convert(db, userStruct, op, cn)
		}
	}
}

// Convert - Convert the YDB data to the target struct.
func (db *YDB) Convert(options ...RetrieveOption) (interface{}, error) {
	var user interface{}
	var opt retrieveOption
	for _, o := range options {
		o(&opt)
	}
	if opt.depth > 0 {
		return nil, fmt.Errorf("RetrieveDepth not supported")
	}
	if opt.user == nil {
		user = map[string]interface{}{}
	} else {
		user = opt.user
	}
	if !opt.nolock {
		db.RLock()
		defer db.RUnlock()
	}
	n := db.top()
	if len(opt.keys) > 0 {
		for _, key := range opt.keys {
			n = n.find(key)
			if n == nil {
				return nil, fmt.Errorf("Not found data (%s)", key)
			}
		}
	}
	// top := n
	errCount := len(db.Errors)
	for n := n.down(); n != nil; n = n.next() {
		convert(db, user, 'c', n)
	}
	if len(db.Errors) > errCount {
		return user, fmt.Errorf("Coversion failed in some struct")
	}
	return user, nil
}

// Close the YDB instance
func (db *YDB) Close() {
	db.Lock()
	defer db.Unlock()
	if db.block != nil {
		// C.ydb_write_hook_unregister(db.block)
		C.ydb_close(db.block)
		db.block = nil
	}
}

// Open an YDB instance with a name
// name: The name of the creating YDB instance
// top: The go structure instance synced with the YDB instance
func Open(name string) (*YDB, func()) {
	return OpenWithTargetStruct(name, nil)
}

// OpenWithTargetStruct - Open an YDB instance with a target Go struct
// name: The name of the creating YDB instance
// top: The go structure instance synced with the YDB instance
func OpenWithTargetStruct(name string, targetStruct interface{}) (*YDB, func()) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	if targetStruct == nil {
		targetStruct = &EmptyGoStruct{}
	}
	db := YDB{
		Name:   name,
		block:  C.ydb_open(cname),
		Target: targetStruct,
		syncCtrl: syncCtrl{
			ToBeIgnored: make(map[string]syncInfo),
		},
	}
	C.ydb_write_hook_register(db.block, unsafe.Pointer(&db.block))
	return &db, func() {
		db.Close()
	}
}

// RelaceTargetStruct - replace the Target structure
// Warning - If RelaceTargetStruct() is called without sync, the data inconsistency happens!!!
func (db *YDB) RelaceTargetStruct(targetStruct interface{}, sync bool) error {
	db.Lock()
	defer db.Unlock()
	if sync {
		_, err := db.Convert(RetrieveAll(), RetrieveStruct(targetStruct),
			retrieveWithoutLock())
		if err != nil {
			return err
		}
	}
	db.Target = targetStruct
	return nil
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
	db.Lock()
	defer db.Unlock()
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
	db.Lock()
	defer db.Unlock()
	res := C.ydb_disconnect(db.block, caddr)
	if res == C.YDB_OK {
		return nil
	}
	return fmt.Errorf("%s", C.GoString(C.ydb_res_str(res)))
}

// SetSignalFilter -- Set signals to ignore
func SetSignalFilter() chan bool {
	sigs := make(chan os.Signal, 1)
	done := make(chan bool, 1)
	signal.Notify(sigs, syscall.SIGPIPE, syscall.SIGINT, syscall.SIGTERM)
	go func() {
		for {
			sig := <-sigs
			fmt.Println("Received signal:", sig)
			if sig == syscall.SIGINT || sig == syscall.SIGTERM {
				done <- true
				break
			}
		}
	}()
	return done
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
		log.Info("Receive() already running.")
		return nil
	}
	for {
		db.fd = int(C.ydb_fd(db.block))
		if db.fd <= 0 {
			err := errors.New(C.GoString(C.ydb_res_str(C.YDB_E_CONN_FAILED)))
			log.Errorf("ydb.fd: %v", err)
			return err
		}
		rfds.Set(db.fd)
		n, err := unix.Select(db.fd+1, &rfds, nil, nil, nil)
		if err != nil {
			if err == syscall.EINTR {
				continue
			}
			log.Errorf("unix.Select: received %v", err)
			db.fd = 0
			return err
		}
		if n == 1 && rfds.IsSet(db.fd) {
			rfds.Clear(db.fd)
			db.Lock()
			res := C.ydb_serve(db.block, C.int(0))
			db.Unlock()
			if res >= C.YDB_ERROR {
				err = fmt.Errorf("%s", C.GoString(C.ydb_res_str(res)))
				log.Errorf("ydb.serve: %v", err)
				db.fd = 0
				return err
			}
		}
	}
	return nil
}

// Parse - parse YAML bytes to update YDB
func (db *YDB) Parse(yaml []byte) error {
	ylen := len(yaml)
	if ylen == 0 {
		return nil
	}
	db.Lock()
	defer db.Unlock()
	res := C.ydb_parses_wrapper(db.block, unsafe.Pointer(&yaml[0]), C.ulong(ylen))
	if res >= C.YDB_ERROR {
		return fmt.Errorf("%s", C.GoString(C.ydb_res_str(res)))
	}
	return nil
}

// Write - writes YAML bytes to the YDB instance
func (db *YDB) Write(yaml []byte) error {
	return db.Parse(yaml)
}

// Delete - delete the target data from the YDB instance.
// e.g. the following /foo/bar's data is deleted from the YDB instance.
//  foo:
//   bar:
func (db *YDB) Delete(yaml []byte) error {
	ylen := len(yaml)
	if ylen == 0 {
		return nil
	}
	db.Lock()
	defer db.Unlock()
	res := C.ydb_delete_wrapper(db.block, unsafe.Pointer(&yaml[0]))
	if res >= C.YDB_ERROR {
		return fmt.Errorf("%s", C.GoString(C.ydb_res_str(res)))
	}
	return nil
}

// Read - reads YAML bytes from the YDB instance
// e.g. returns the YAML with /foo/bar's data
//  foo:          >>   foo:
//   bar:               bar: "hello yaml"
func (db *YDB) Read(yaml []byte) []byte {
	db.RLock()
	defer db.RUnlock()
	var cptr C.bufinfo
	if len(yaml) == 0 {
		cptr = C.ydb_fprintf_wrapper(db.block, unsafe.Pointer(nil))
	} else {
		cptr = C.ydb_fprintf_wrapper(db.block, unsafe.Pointer(&yaml[0]))
	}
	if cptr.buf != nil {
		defer C.free(unsafe.Pointer(cptr.buf))
		byt := C.GoBytes(unsafe.Pointer(cptr.buf), cptr.buflen)
		return byt
	}
	return nil
}

// sync - synchronizes the target data from the remote YDB instance.
func (db *YDB) sync(yaml []byte) error {
	// db.Lock()
	// defer db.Unlock()
	var res C.ydb_res
	if len(yaml) == 0 {
		res = C.ydb_sync_wrapper(db.block, unsafe.Pointer(nil))
	} else {
		res = C.ydb_sync_wrapper(db.block, unsafe.Pointer(&yaml[0]))
	}
	if res >= C.YDB_ERROR {
		return fmt.Errorf("%s", C.GoString(C.ydb_res_str(res)))
	}
	return nil
}

// WriteTo - writes the value string to the target path in the YDB instance
func (db *YDB) WriteTo(path string, value string) error {
	db.Lock()
	defer db.Unlock()
	var pp, pv unsafe.Pointer
	if path != "" {
		pbyte := []byte(path)
		pp = unsafe.Pointer(&pbyte[0])
	}
	if value != "" {
		vbyte := []byte(value)
		pv = unsafe.Pointer(&vbyte[0])
	}
	res := C.ydb_path_write_wrapper(db.block, pp, pv)
	if res >= C.YDB_ERROR {
		return fmt.Errorf("%s", C.GoString(C.ydb_res_str(res)))
	}
	return nil
}

// DeleteFrom - deletes the value at the target path from the YDB instance
func (db *YDB) DeleteFrom(path string) error {
	db.Lock()
	defer db.Unlock()
	var pp unsafe.Pointer
	if path != "" {
		pbyte := []byte(path)
		pp = unsafe.Pointer(&pbyte[0])
	}
	res := C.ydb_path_delete_wrapper(db.block, pp)
	if res >= C.YDB_ERROR {
		return fmt.Errorf("%s", C.GoString(C.ydb_res_str(res)))
	}
	return nil
}

// ReadFrom - reads the value(string) from the target path in the YDB instance
func (db *YDB) ReadFrom(path string) string {
	db.RLock()
	defer db.RUnlock()
	var pp unsafe.Pointer
	if path != "" {
		pbyte := []byte(path)
		pp = unsafe.Pointer(&pbyte[0])
	}
	value := C.ydb_path_read_wrapper(db.block, pp)
	if value == nil {
		return ""
	}
	vstr := C.GoString(value)
	return vstr
}

// Timeout - Set the timeout of the YDB instance for sync.
func (db *YDB) Timeout(msec int) error {
	db.RLock()
	defer db.RUnlock()
	res := C.ydb_timeout(db.block, C.int(msec))
	if res >= C.YDB_ERROR {
		return fmt.Errorf("%s", C.GoString(C.ydb_res_str(res)))
	}
	return nil
}

// AddSyncUpdatePath - registers SyncUpdater
func (db *YDB) AddSyncUpdatePath(path string) error {
	db.Lock()
	defer db.Unlock()
	var pp unsafe.Pointer
	if path != "" {
		pbyte := []byte(path)
		pp = unsafe.Pointer(&pbyte[0])
	}
	res := C.ydb_read_hook_register(db.block, pp, unsafe.Pointer(&db.block))
	if res >= C.YDB_ERROR {
		return fmt.Errorf("%s", C.GoString(C.ydb_res_str(res)))
	}
	return nil
}

// DeleteSyncUpdatePath - registers SyncUpdater
func (db *YDB) DeleteSyncUpdatePath(path string) {
	db.Lock()
	defer db.Unlock()
	var pp unsafe.Pointer
	if path != "" {
		pbyte := []byte(path)
		pp = unsafe.Pointer(&pbyte[0])
	}
	C.ydb_read_hook_unregister(db.block, pp)
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
		dec.db.Lock()
		defer dec.db.Unlock()
		var res C.ydb_res
		blen := len(byt)
		if blen > 0 {
			res = C.ydb_parses_wrapper(dec.db.block, unsafe.Pointer(&byt[0]), C.ulong(blen))
		}
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
	enc.db.RLock()
	cptr := C.ydb_path_fprintf_wrapper(enc.db.block, unsafe.Pointer(nil))
	enc.db.RUnlock()
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
				log.Error("Invalid map key type -", node)
				return nil
			}
			key = key.Convert(keytyp)
			elemtyp := typ.Elem()
			if elemtyp.Kind() != val.Kind() && !elem.Type().ConvertibleTo(elemtyp) {
				log.Error("Invalid element type -", node)
				return nil
			}
			val.SetMapIndex(key, elem.Convert(elemtyp))
		case reflect.Slice:
			parentval := reflect.ValueOf(parent).Elem()
			parentFieldVal := parentval.FieldByName("Value")
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

func (n *C.ynode) findByPrefix(prefix string) *C.ynode {
	k := C.CString(prefix)
	defer C.free(unsafe.Pointer(k))
	return C.ydb_find_child_by_prefix(n, k)
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

func join(strs ...string) string {
	var sb strings.Builder
	for _, str := range strs {
		sb.WriteString(str)
	}
	return sb.String()
}

func (n *C.ynode) getpath(db *YDB) string {
	s := ""
	top := db.top()
	for ; n != nil && n != top; n = n.up() {
		s = join(n.key(), "/", s)
	}
	return s
}

func findSyncNode(n *C.ynode, key []string) *C.ynode {
	if n == nil {
		return nil
	}
	if len(key) == 0 {
		return n
	}
	if key[0] != "" {
		n = n.find(key[0])
		if n == nil {
			return nil
		}
	}
	return findSyncNode(n, key[1:])
}

func findSyncNodeByPrefix(n *C.ynode, prefixkey []string) []*C.ynode {
	if n == nil {
		return nil
	}
	if len(prefixkey) == 0 {
		return []*C.ynode{n}
	}
	nodes := []*C.ynode{}
	if prefixkey[0] == "" {
		prefixkey = prefixkey[1:]
	}
	for cn := n.findByPrefix(prefixkey[0]); cn != nil; cn = cn.next() {
		if !strings.HasPrefix(cn.key(), prefixkey[0]) {
			break
		}
		children := findSyncNodeByPrefix(cn, prefixkey[1:])
		nodes = append(nodes, children...)
	}
	return nodes
}

// syncInfo - information for sync
type syncInfo struct {
	syncSequence uint
}

type syncCtrl struct {
	ToBeIgnored  map[string]syncInfo
	syncSequence uint
}

// SyncTo - request the update to remote YDB instances to refresh the data nodes.
func (db *YDB) SyncTo(syncIgnoredTime time.Duration, prefixSearching bool, paths ...string) error {
	// retrieve the path ==> YNode list matched to the path
	// Check syncIgnoreTimer is running
	// Check the list of the previous sync requests.
	// send sync and update data from the remote YDB.
	// Store the path of the Ynode list.
	// Running syncIgnoreTimer

	db.Lock()
	defer db.Unlock()
	db.syncCtrl.syncSequence++
	sequence := db.syncCtrl.syncSequence
	n := db.top()
	if n == nil {
		return fmt.Errorf("no top data node")
	}
	nodelist := []*C.ynode{}
	for _, path := range paths {
		keylist, err := ToSliceKeys(path)
		if err != nil {
			continue
		}
		if prefixSearching {
			nodelist = append(nodelist, findSyncNodeByPrefix(n, keylist)...)
		} else {
			nodelist = append(nodelist, findSyncNode(n, keylist))
		}
	}
	var syncTriggered bool
	var bb bytes.Buffer
	for _, node := range nodelist {
		path := node.getpath(db)
		if _, ok := db.syncCtrl.ToBeIgnored[path]; ok {
			continue
		}
		db.syncCtrl.ToBeIgnored[path] = syncInfo{syncSequence: sequence}
		cptr := C.ydb_ynode2yaml_wrapper(db.block, node)
		if cptr.buf != nil {
			bb.Write(C.GoBytes(unsafe.Pointer(cptr.buf), cptr.buflen))
			C.free(unsafe.Pointer(cptr.buf))
		}
		syncTriggered = true
	}
	if bb.Len() > 0 {
		db.sync(bb.Bytes())
	}
	if syncTriggered {
		timer := time.NewTimer(syncIgnoredTime)
		go func(db *YDB, sequence uint, timer *time.Timer) {
			select {
			case <-timer.C:
				db.Lock()
				defer db.Unlock()
				for k, v := range db.syncCtrl.ToBeIgnored {
					if v.syncSequence == sequence {
						delete(db.syncCtrl.ToBeIgnored, k)
					}
				}
			}
		}(db, sequence, timer)
	}
	return nil
}

//export ylogGo
// ylogGo - Logging function for YDB native logging facility.
func ylogGo(level C.int, f *C.char, line C.int, buf *C.char, buflen C.int) {
	if log == nil {
		return
	}
	switch level {
	case LogDebug:
		log.Debugf("%s %d %s", C.GoString(f), line, C.GoStringN(buf, buflen))
	case LogInout:
		log.Debugf("%s %d %s", C.GoString(f), line, C.GoStringN(buf, buflen))
	case LogInfo:
		log.Infof("%s %d %s", C.GoString(f), line, C.GoStringN(buf, buflen))
	case LogWarn:
		log.Warnf("%s %d %s", C.GoString(f), line, C.GoStringN(buf, buflen))
	case LogError:
		log.Errorf("%s %d %s", C.GoString(f), line, C.GoStringN(buf, buflen))
	case LogCritical:
		log.Fatalf("%s %d %s", C.GoString(f), line, C.GoStringN(buf, buflen))
	}
}

func init() {
	C.ylog_init()
}
