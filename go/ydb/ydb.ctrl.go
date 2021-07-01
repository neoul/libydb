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

static ydb_res ydb_read_hook_register(ydb *datablock, char *path, void *U1)
{
	return ydb_read_hook_add(datablock, path, (ydb_read_hook)ydb_read_hooker, 1, U1);
}

static void ydb_read_hook_unregister(ydb *datablock, char *path)
{
	ydb_read_hook_delete(datablock, path);
}

static ydb_res ydb_parses_wrapper(ydb *datablock, void *d, size_t dlen)
{
	return ydb_parses(datablock, (char *)d, dlen);
}

// The return must be free.
static bufinfo ydb_path_fprintf_wrapper(ydb *datablock, char *path)
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

// The return must be free.
static bufinfo ydb_fprintf_wrapper(ydb *datablock, void *y, size_t ylen)
{
    FILE *fp;
    char *buf = NULL;
	size_t buflen = 0;
	fp = open_memstream(&buf, &buflen);
	if (fp)
	{
		int n;
		if (y)
			n = ydb_fprintf(fp, datablock, "%.*s", ylen, y);
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

static ydb_res ydb_delete_wrapper(ydb *datablock, void *input_yaml, size_t ylen)
{
	ydb_res res = YDB_OK;
	if (input_yaml)
		res = ydb_delete(datablock, "%.*s", ylen, input_yaml);
	return res;
}

static ydb_res ydb_sync_wrapper(ydb *datablock, void *input_yaml, size_t ylen)
{
	ydb_res res = YDB_OK;
	if (input_yaml)
		res = ydb_sync(datablock, "%.*s", ylen, input_yaml);
	return res;
}

static ydb_res ydb_path_write_wrapper(ydb *datablock, char *path)
{
	if (path) {
		return ydb_path_write(datablock, "%s", path);
	}
	return YDB_OK;
}

static ydb_res ydb_path_delete_wrapper(ydb *datablock, char *path)
{
	if (path)
		return ydb_path_delete(datablock, "%s", path);
	return YDB_OK;
}

static char *ydb_path_read_wrapper(ydb *datablock, char *path)
{
	if (path)
		return (char *) ydb_path_read(datablock, "%s", path);
	return NULL;
}

static ydb_res ydb_path_sync_wrapper(ydb *datablock, char *path)
{
	if (path)
		return ydb_path_sync(datablock, "%s", path);
	return YDB_OK;
}

static ynode *ydb_search_wrapper(ydb *datablock, char *path)
{
	if (path)
		return ydb_search(datablock, "%s", path);
	return NULL;
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
	"flag"
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

	"github.com/golang/glog"
	"golang.org/x/sys/unix"
)

// InternalLogLevel of YDB API (C Library)
type InternalLogLevel uint

const (
	// InternalLogCritical YDB log level
	InternalLogCritical InternalLogLevel = C.YLOG_CRITICAL
	// InternalLogError YDB log level
	InternalLogError InternalLogLevel = C.YLOG_ERROR
	// InternalLogWarn YDB log level
	InternalLogWarn InternalLogLevel = C.YLOG_WARN
	// InternalLogInfo YDB log level
	InternalLogInfo InternalLogLevel = C.YLOG_INFO
	// InternalLogInout YDB log level
	InternalLogInout InternalLogLevel = C.YLOG_INOUT
	// InternalLogDebug YDB log level
	InternalLogDebug InternalLogLevel = C.YLOG_DEBUG
)

// SetInternalLog configures the log level of YDB
func SetInternalLog(level InternalLogLevel) {
	C.ylog_level = C.uint(level)
}

// LogLevel of YDB Go API
type LogLevel uint

const (
	// LogCritical YDB log level
	LogCritical LogLevel = 0
	// LogError YDB log level
	LogError LogLevel = 1
	// LogWarn YDB log level
	LogWarn LogLevel = 2
	// LogInfo YDB log level
	LogInfo LogLevel = 3
)

var ydbLevel LogLevel

// SetLog sets ydbLevel (LogLevel of YDB Go API)
func SetLog(level LogLevel) {
	ydbLevel = level
	if !flag.Parsed() {
		flag.Parse()
	}
}

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
	path   string
	all    bool
	depth  int
	user   interface{}
	nolock bool
}

// ConvertOption - The option to convert the YDB data to YNodes or user-defined go struct.
type ConvertOption func(*retrieveOption)

// func ordering(o func(cell, cell) bool) ConvertOption {
//     return func(s *retrieveOption) { s.ordering = o }
// }

// ConvertDepth - The option to set the depth of the converted data
func ConvertDepth(d int) ConvertOption {
	return func(s *retrieveOption) { s.depth = d }
}

// ConvertPath - The option to set the start point of the converting
func ConvertPath(path string) ConvertOption {
	return func(s *retrieveOption) { s.path = path }
}

// ConvertPathKey - The option to set the start point of the converting
func ConvertPathKey(k ...string) ConvertOption {
	return func(s *retrieveOption) { s.keys = k }
}

// convertingWithoutLock - The option to convert data without lock/unlock
func convertingWithoutLock() ConvertOption {
	return func(s *retrieveOption) { s.nolock = true }
}

// getUpdater from v to call the custom Updater interface.
func getUpdater(target interface{}, keys []string) (interface{}, []string) {
	var newtarget interface{} = target
	var newkey []string = keys
	if target == nil {
		return nil, keys
	}
	v := reflect.ValueOf(target)
	for i, key := range keys {
		var ok bool
		v, ok = ValFind(v, key, NoSearch)
		if !ok {
			break
		}
		if v.Type().NumMethod() > 0 && v.CanInterface() {
			switch u := v.Interface().(type) {
			case Updater:
				newtarget = u
				newkey = keys[i+1:]
			case DataUpdate:
				newtarget = u
				newkey = keys[i+1:]
			}
		}
	}
	return newtarget, newkey
}

func reverse(ss []string) {
	last := len(ss) - 1
	for i := 0; i < len(ss)/2; i++ {
		ss[i], ss[last-i] = ss[last-i], ss[i]
	}
}

func construct(target interface{}, op int, cur *C.ynode, new *C.ynode, root *C.ynode) error {
	var n *C.ynode = new
	if new == nil {
		n = cur
	}
	var keys = make([]string, 0, n.level(root))
	if root != n {
		for p := n.up(); p != nil && p != root; p = p.up() {
			key := p.key()
			if key != "" {
				keys = append(keys, key)
			}
		}
	} else {
		return nil
	}
	reverse(keys)
	newtarget, newkeys := getUpdater(target, keys)

	var err error = nil
	switch op {
	case 'c', 'r':
		if ydbLevel >= LogInfo {
			glog.Infof("node.%c(%s, %s, %s, %s)", op, keys, n.key(), n.tag(), n.value())
		}
	case 'd':
		if ydbLevel >= LogInfo {
			glog.Infof("node.%c(%s, %s)", op, keys, n.key())
		}
	default:
		return fmt.Errorf("%c %s, %s, %s, %s: unknown op", op, keys, n.key(), n.tag(), n.value())
	}

	switch u := newtarget.(type) {
	case Updater:
		switch op {
		case 'c':
			err = u.Create(newkeys, n.key(), n.tag(), n.value())
		case 'r':
			err = u.Replace(newkeys, n.key(), n.tag(), n.value())
		case 'd':
			err = u.Delete(newkeys, n.key())
		}
	case DataUpdate:
		var path string
		if n.up().tag() == "!!set" {
			if len(newkeys) > 0 {
				path = "/" + strings.Join(newkeys, "/")
			} else {
				path = "/"
			}
			switch op {
			case 'c':
				err = u.UpdateCreate(path, n.key())
			case 'r':
				err = u.UpdateReplace(path, n.key())
			case 'd':
				err = u.UpdateDelete(path)
			}
		} else {
			if len(newkeys) > 0 {
				path = "/" + strings.Join(newkeys, "/") + "/" + n.key()
			} else {
				path = "/" + n.key()
			}
			switch op {
			case 'c':
				err = u.UpdateCreate(path, n.value())
			case 'r':
				err = u.UpdateReplace(path, n.value())
			case 'd':
				err = u.UpdateDelete(path)
			}
		}
	default:
		v := reflect.ValueOf(target)
		switch op {
		case 'c':
			err = ValYdbSet(v, keys, n.key(), n.tag(), n.value())
		case 'r':
			err = ValYdbSet(v, keys, n.key(), n.tag(), n.value())
		case 'd':
			err = ValYdbUnset(v, keys, n.key())
		}
	}
	if err != nil {
		return fmt.Errorf("%c %s, %s, %s, %s: %v", op, keys, n.key(), n.tag(), n.value(), err)
	}
	return nil
}

//export manipulate
// manipulate -- Update YAML Data Block instance
func manipulate(ygo unsafe.Pointer, op C.char, cur *C.ynode, new *C.ynode) {
	var db *YDB = (*YDB)(ygo)
	if db.target != nil {
		err := construct(db.target, int(op), cur, new, db.top())
		if err != nil {
			db.Errors = append(db.Errors, err)
		}
	}
}

//export updateStartEnd
// updateStartEnd -- indicates the YDB update start and end.
func updateStartEnd(ygo unsafe.Pointer, started C.int) {
	var db *YDB = (*YDB)(ygo)
	if db.target != nil {
		if startEnd, ok := db.target.(UpdaterStartEnd); ok {
			if started != 0 {
				startEnd.UpdaterStart()
			} else {
				startEnd.UpdaterEnd()
			}
		} else if startEnd, ok := db.target.(DataUpdateStartEnd); ok {
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
	if db.target == nil {
		return
	}
	if SyncResponse, ok := db.target.(UpdaterSyncResponse); ok {
		var b []byte
		keylist, err := ToSliceKeys(C.GoString(path))
		if err != nil {
			return
		}
		if klen := len(keylist); klen > 0 {
			b = SyncResponse.SyncResponse(keylist[:klen-1], keylist[klen-1])
		} else {
			b = SyncResponse.SyncResponse(nil, "")
		}
		blen := len(b)
		if blen > 0 {
			C.fwrite(unsafe.Pointer(&b[0]), C.ulong(blen), 1, stream)
		}
	} else if SyncResponse, ok := db.target.(DataUpdateSyncResponse); ok {
		var b []byte
		b = SyncResponse.SyncResponse(C.GoString(path))
		blen := len(b)
		if blen > 0 {
			C.fwrite(unsafe.Pointer(&b[0]), C.ulong(blen), 1, stream)
		}
	}
}

type dataUpdateEntry struct {
	path     string
	value    string
	isDelete bool
}

// YDB (YAML YNode type) to indicate an YDB instance
type YDB struct {
	block *C.ydb
	*sync.RWMutex
	fd               int
	Name             string
	target           interface{}
	Errors           []error
	dataUpdate       []*dataUpdateEntry
	atomicDataUpdate bool
	returnWarning    bool
	enabledErrors    bool
}

func convert(db *YDB, userStruct interface{}, op int, n *C.ynode, root *C.ynode) {
	err := construct(userStruct, op, n, nil, root)
	if err != nil {
		db.Errors = append(db.Errors, err)
	} else {
		for cn := n.down(); cn != nil; cn = cn.next() {
			convert(db, userStruct, op, cn, root)
		}
	}
}

// ConvertToYNode - returns data that consists of YNodes.
func (db *YDB) ConvertToYNode(options ...ConvertOption) (*YNode, error) {
	var node, parent *YNode
	var opt retrieveOption
	for _, o := range options {
		o(&opt)
	}
	db.RLock()
	defer db.RUnlock()
	n := db.top()
	if len(opt.keys) > 0 {
		for _, key := range opt.keys {
			n = n.find(key)
			if n == nil {
				return nil, fmt.Errorf("not found (%s)", key)
			}
		}
	} else if opt.path != "" {
		n = db.findNode(opt.path)
	}
	node = n.createYNode(nil)
	if opt.all || opt.depth > 0 {
		parent = node
		for n := n.down(); n != nil; n = n.next() {
			n.addYnode(parent, opt.depth-1, opt.all)
		}
	}
	return node, nil
}

// Convert - Convert the YDB data to the target struct or map[string]interface{}.
func (db *YDB) Convert(target interface{}, options ...ConvertOption) error {
	var opt retrieveOption
	for _, o := range options {
		o(&opt)
	}
	if opt.depth > 0 {
		return fmt.Errorf("ConvertDepth must be >= 0")
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
				return fmt.Errorf("Not found data for '%s'", key)
			}
		}
	} else if opt.path != "" {
		n = db.findNode(opt.path)
	}
	top := n
	errCount := len(db.Errors)
	for n := n.down(); n != nil; n = n.next() {
		convert(db, target, 'c', n, top)
	}
	if len(db.Errors) > errCount {
		return fmt.Errorf("%T converting is failed", target)
	}
	return nil
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

// Open an YDB instance with the name
// name: The name of the creating YDB instance
func Open(name string) (*YDB, func()) {
	return OpenWithSync(name, nil)
}

// New creates an YDB instance with the name
// name: The name of the creating YDB instance
func New(name string) *YDB {
	db, _ := OpenWithSync(name, nil)
	return db
}

// OpenWithSync - Open an YDB instance within sync mode.
//  - name: The name of the creating YDB instance
//  - target: a go struct or map[string]interface{} synced with the YDB instance.
// The user must lock and hold to handle the data block using (*YDB).Lock(), Unlock()
func OpenWithSync(name string, target interface{}) (*YDB, func()) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	db := YDB{
		Name:    name,
		RWMutex: new(sync.RWMutex),
		block:   C.ydb_open(cname),
		target:  target,
	}
	// C.ydb_disable_variable_arguments(db.block)
	C.ydb_write_hook_register(db.block, unsafe.Pointer(&db.block))
	return &db, func() {
		db.Close()
	}
}

// SetTarget - replace the Target structure
// Warning - If SetTarget() is called without sync, the data inconsistency happens!
func (db *YDB) SetTarget(target interface{}, sync bool) error {
	db.Lock()
	defer db.Unlock()
	if sync && target != nil {
		if err := db.Convert(target, convertingWithoutLock()); err != nil {
			return err
		}
	}
	db.target = target
	return nil
}

// EnableWarning - enables or disables warning return of the YDB instance.
func (db *YDB) EnableWarning(enable bool) {
	db.returnWarning = enable
}

// EnableAtomicUpdate - enables or disables atomic DataUpdate interface for the YDB instance.
func (db *YDB) EnableAtomicUpdate(enable bool) {
	db.atomicDataUpdate = enable
}

func (db *YDB) resultErr(res C.ydb_res) error {
	if db.returnWarning {
		if res == C.YDB_OK {
			return nil
		}
		return fmt.Errorf("%s", C.GoString(C.ydb_res_str(res)))
	}
	if res >= C.YDB_ERROR {
		return fmt.Errorf("%s", C.GoString(C.ydb_res_str(res)))
	}
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
	return db.resultErr(res)
}

// Disconnect to YDB IPC channel
func (db *YDB) Disconnect(addr string) error {
	caddr := C.CString(addr)
	defer C.free(unsafe.Pointer(caddr))
	db.Lock()
	defer db.Unlock()
	res := C.ydb_disconnect(db.block, caddr)
	return db.resultErr(res)
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
		glog.Errorf("Receive() already running.")
		return nil
	}
	for {
		db.fd = int(C.ydb_fd(db.block))
		if db.fd <= 0 {
			err := errors.New(C.GoString(C.ydb_res_str(C.YDB_E_CONN_FAILED)))
			glog.Errorf("ydb.fd: %v", err)
			return err
		}
		rfds.Set(db.fd)
		n, err := unix.Select(db.fd+1, &rfds, nil, nil, nil)
		if err != nil {
			if err == syscall.EINTR {
				continue
			}
			glog.Errorf("unix.Select: received %v", err)
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
				glog.Errorf("ydb.serve: %v", err)
				db.fd = 0
				return err
			}
		}
	}
	return nil
}

// parse - parse YAML bytes to update YDB
func (db *YDB) parse(yaml []byte, nolock bool) error {
	ylen := len(yaml)
	if ylen == 0 {
		return nil
	}
	if !nolock {
		db.Lock()
		defer db.Unlock()
	}
	res := C.ydb_parses_wrapper(db.block, unsafe.Pointer(&yaml[0]), C.ulong(ylen))
	return db.resultErr(res)
}

// Parse - parse YAML bytes to update YDB
func (db *YDB) Parse(yaml []byte) error {
	return db.parse(yaml, false)
}

// Write - writes YAML bytes to the YDB instance
func (db *YDB) Write(yaml []byte) error {
	return db.parse(yaml, false)
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
	res := C.ydb_delete_wrapper(db.block, unsafe.Pointer(&yaml[0]), C.ulong(ylen))
	return db.resultErr(res)
}

// Read - reads YAML bytes from the YDB instance
// e.g. returns the YAML with /foo/bar's data
//  foo:          >>   foo:
//   bar:               bar: "hello yaml"
func (db *YDB) Read(yaml []byte) []byte {
	db.RLock()
	defer db.RUnlock()
	var cptr C.bufinfo
	ylen := len(yaml)
	if ylen == 0 {
		cptr = C.ydb_fprintf_wrapper(db.block, unsafe.Pointer(nil), C.ulong(0))
	} else {
		cptr = C.ydb_fprintf_wrapper(db.block, unsafe.Pointer(&yaml[0]), C.ulong(ylen))
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
	ylen := len(yaml)
	if ylen == 0 {
		res = C.ydb_sync_wrapper(db.block, unsafe.Pointer(nil), C.ulong(0))
	} else {
		res = C.ydb_sync_wrapper(db.block, unsafe.Pointer(&yaml[0]), C.ulong(ylen))
	}
	return db.resultErr(res)
}

// Sync - request the update of the YDB instance and return the updated data
//  foo:          >>   foo:
//   bar:               bar: "hello yaml"
func (db *YDB) Sync(yaml []byte) ([]byte, error) {
	var cptr C.bufinfo
	db.Lock()
	defer db.Unlock()
	err := db.sync(yaml)
	if err != nil {
		return nil, err
	}
	ylen := len(yaml)
	if ylen == 0 {
		cptr = C.ydb_fprintf_wrapper(db.block, unsafe.Pointer(nil), C.ulong(0))
	} else {
		cptr = C.ydb_fprintf_wrapper(db.block, unsafe.Pointer(&yaml[0]), C.ulong(ylen))
	}
	if cptr.buf != nil {
		defer C.free(unsafe.Pointer(cptr.buf))
		byt := C.GoBytes(unsafe.Pointer(cptr.buf), cptr.buflen)
		return byt, nil
	}
	return nil, nil
}

// SyncTo - request the update of the YDB instance
//  foo:          >>   foo:
//   bar:               bar: "hello yaml"
func (db *YDB) SyncTo(path ...string) error {
	if len(path) == 0 {
		return nil
	} else if len(path) == 1 {
		db.Lock()
		defer db.Unlock()
		p := C.CString(path[0])
		defer C.free(unsafe.Pointer(p))
		res := C.ydb_path_sync_wrapper(db.block, p)
		return db.resultErr(res)
	}

	db.Lock()
	defer db.Unlock()
	var bb bytes.Buffer
	for i := range path {
		if path[i] == "" {
			continue
		}
		keylist, err := ToSliceKeys(path[i])
		if err != nil {
			return err
		}
		indent := 0
		for _, key := range keylist {
			// bb.WriteString(fmt.Sprintf("\n%s%s:", strings.Repeat(" ", indent), key))
			bb.WriteString("\n")
			bb.WriteString(strings.Repeat(" ", indent))
			bb.WriteString(key)
			bb.WriteString(":")
			indent++
		}
	}
	return db.sync(bb.Bytes())
}

// WriteTo - writes the value string to the target path in the YDB instance
func (db *YDB) WriteTo(path string, value string) error {
	db.Lock()
	defer db.Unlock()
	var pathvalue string
	if value != "" {
		pathvalue = path + "=" + value
	} else {
		pathvalue = path
	}

	pv := C.CString(pathvalue)
	defer C.free(unsafe.Pointer(pv))
	res := C.ydb_path_write_wrapper(db.block, pv)
	return db.resultErr(res)
}

// DeleteFrom - deletes the value at the target path from the YDB instance
func (db *YDB) DeleteFrom(path string) error {
	db.Lock()
	defer db.Unlock()
	p := C.CString(path)
	defer C.free(unsafe.Pointer(p))
	res := C.ydb_path_delete_wrapper(db.block, p)
	return db.resultErr(res)
}

// ReadFrom - reads the value(string) from the target path in the YDB instance
func (db *YDB) ReadFrom(path string) string {
	db.RLock()
	defer db.RUnlock()
	p := C.CString(path)
	defer C.free(unsafe.Pointer(p))
	value := C.ydb_path_read_wrapper(db.block, p)
	if value == nil {
		return ""
	}
	vstr := C.GoString(value)
	return vstr
}

// Timeout - Sets the timeout of the YDB instance for sync.
func (db *YDB) Timeout(t time.Duration) error {
	db.RLock()
	defer db.RUnlock()
	ms := t / time.Millisecond
	// fmt.Println(int(ms))
	res := C.ydb_timeout(db.block, C.int(ms))
	return db.resultErr(res)
}

// AddSyncResponse - registers the path where the UpdaterSyncResponse or DataUpdateSyncResponse interface is executed.
func (db *YDB) AddSyncResponse(path string) error {
	db.Lock()
	defer db.Unlock()
	p := C.CString(path)
	defer C.free(unsafe.Pointer(p))
	res := C.ydb_read_hook_register(db.block, p, unsafe.Pointer(&db.block))
	return db.resultErr(res)
}

// DelSyncResponse - unregisters the path where the UpdaterSyncResponse or DataUpdateSyncResponse interface is executed.
func (db *YDB) DelSyncResponse(path string) {
	db.Lock()
	defer db.Unlock()
	p := C.CString(path)
	defer C.free(unsafe.Pointer(p))
	C.ydb_read_hook_unregister(db.block, p)
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
	p := C.CString("/")
	defer C.free(unsafe.Pointer(p))
	cptr := C.ydb_path_fprintf_wrapper(enc.db.block, p)
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
				glog.Errorf("invalid map key type: %v", node)
				return nil
			}
			key = key.Convert(keytyp)
			elemtyp := typ.Elem()
			if elemtyp.Kind() != val.Kind() && !elem.Type().ConvertibleTo(elemtyp) {
				glog.Errorf("invalid element type %v", node)
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

func (db *YDB) findNode(path string) *C.ynode {
	p := C.CString(path)
	defer C.free(unsafe.Pointer(p))
	return C.ydb_search_wrapper(db.block, p)
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

func (n *C.ynode) level(top *C.ynode) int {
	return int(C.ydb_level(top, n))
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

// EnhansedSyncTo - request the update to remote YDB instances to refresh the data nodes.
func (db *YDB) EnhansedSyncTo(syncIgnoredTime time.Duration, prefixSearching bool, paths ...string) error {
	return fmt.Errorf("EnhansedSyncTo is deprecated")
}

//export ylogGo
// ylogGo - Logging function for YDB native logging facility.
func ylogGo(level C.int, f *C.char, line C.int, buf *C.char, buflen C.int) {
	if flag.Parsed() {
		switch InternalLogLevel(level) {
		case InternalLogDebug:
			glog.Infof("%s %d %s", C.GoString(f), line, C.GoStringN(buf, buflen))
		case InternalLogInout:
			glog.Infof("%s %d %s", C.GoString(f), line, C.GoStringN(buf, buflen))
		case InternalLogInfo:
			glog.Infof("%s %d %s", C.GoString(f), line, C.GoStringN(buf, buflen))
		case InternalLogWarn:
			glog.Warningf("%s %d %s", C.GoString(f), line, C.GoStringN(buf, buflen))
		case InternalLogError:
			glog.Errorf("%s %d %s", C.GoString(f), line, C.GoStringN(buf, buflen))
		case InternalLogCritical:
			glog.Fatalf("%s %d %s", C.GoString(f), line, C.GoStringN(buf, buflen))
		}
	}
}

func init() {
	C.ylog_init()
}
