# This file was automatically generated by SWIG (http://www.swig.org).
# Version 3.0.12
#
# Do not make changes to this file unless you know what you are doing--modify
# the SWIG interface file instead.

from sys import version_info as _swig_python_version_info
if _swig_python_version_info >= (2, 7, 0):
    def swig_import_helper():
        import importlib
        pkg = __name__.rpartition('.')[0]
        mname = '.'.join((pkg, '_ydb')).lstrip('.')
        try:
            return importlib.import_module(mname)
        except ImportError:
            return importlib.import_module('_ydb')
    _ydb = swig_import_helper()
    del swig_import_helper
elif _swig_python_version_info >= (2, 6, 0):
    def swig_import_helper():
        from os.path import dirname
        import imp
        fp = None
        try:
            fp, pathname, description = imp.find_module('_ydb', [dirname(__file__)])
        except ImportError:
            import _ydb
            return _ydb
        try:
            _mod = imp.load_module('_ydb', fp, pathname, description)
        finally:
            if fp is not None:
                fp.close()
        return _mod
    _ydb = swig_import_helper()
    del swig_import_helper
else:
    import _ydb
del _swig_python_version_info

try:
    _swig_property = property
except NameError:
    pass  # Python < 2.2 doesn't have 'property'.

try:
    import builtins as __builtin__
except ImportError:
    import __builtin__

def _swig_setattr_nondynamic(self, class_type, name, value, static=1):
    if (name == "thisown"):
        return self.this.own(value)
    if (name == "this"):
        if type(value).__name__ == 'SwigPyObject':
            self.__dict__[name] = value
            return
    method = class_type.__swig_setmethods__.get(name, None)
    if method:
        return method(self, value)
    if (not static):
        if _newclass:
            object.__setattr__(self, name, value)
        else:
            self.__dict__[name] = value
    else:
        raise AttributeError("You cannot add attributes to %s" % self)


def _swig_setattr(self, class_type, name, value):
    return _swig_setattr_nondynamic(self, class_type, name, value, 0)


def _swig_getattr(self, class_type, name):
    if (name == "thisown"):
        return self.this.own()
    method = class_type.__swig_getmethods__.get(name, None)
    if method:
        return method(self)
    raise AttributeError("'%s' object has no attribute '%s'" % (class_type.__name__, name))


def _swig_repr(self):
    try:
        strthis = "proxy of " + self.this.__repr__()
    except __builtin__.Exception:
        strthis = ""
    return "<%s.%s; %s >" % (self.__class__.__module__, self.__class__.__name__, strthis,)

try:
    _object = object
    _newclass = 1
except __builtin__.Exception:
    class _object:
        pass
    _newclass = 0

YDB_OK = _ydb.YDB_OK
YDB_W_UPDATED = _ydb.YDB_W_UPDATED
YDB_W_MORE_RECV = _ydb.YDB_W_MORE_RECV
YDB_W_DISCONN = _ydb.YDB_W_DISCONN
YDB_WARNING_MIN = _ydb.YDB_WARNING_MIN
YDB_WARNING_MAX = _ydb.YDB_WARNING_MAX
YDB_ERROR = _ydb.YDB_ERROR
YDB_E_CTRL = _ydb.YDB_E_CTRL
YDB_E_SYSTEM_FAILED = _ydb.YDB_E_SYSTEM_FAILED
YDB_E_STREAM_FAILED = _ydb.YDB_E_STREAM_FAILED
YDB_E_PERSISTENCY_ERR = _ydb.YDB_E_PERSISTENCY_ERR
YDB_E_INVALID_ARGS = _ydb.YDB_E_INVALID_ARGS
YDB_E_TYPE_ERR = _ydb.YDB_E_TYPE_ERR
YDB_E_INVALID_PARENT = _ydb.YDB_E_INVALID_PARENT
YDB_E_NO_ENTRY = _ydb.YDB_E_NO_ENTRY
YDB_E_MEM_ALLOC = _ydb.YDB_E_MEM_ALLOC
YDB_E_FULL_BUF = _ydb.YDB_E_FULL_BUF
YDB_E_INVALID_YAML_TOKEN = _ydb.YDB_E_INVALID_YAML_TOKEN
YDB_E_YAML_INIT_FAILED = _ydb.YDB_E_YAML_INIT_FAILED
YDB_E_YAML_PARSING_FAILED = _ydb.YDB_E_YAML_PARSING_FAILED
YDB_E_MERGE_FAILED = _ydb.YDB_E_MERGE_FAILED
YDB_E_DELETE_FAILED = _ydb.YDB_E_DELETE_FAILED
YDB_E_INVALID_MSG = _ydb.YDB_E_INVALID_MSG
YDB_E_ENTRY_EXISTS = _ydb.YDB_E_ENTRY_EXISTS
YDB_E_NO_CONN = _ydb.YDB_E_NO_CONN
YDB_E_CONN_FAILED = _ydb.YDB_E_CONN_FAILED
YDB_E_CONN_CLOSED = _ydb.YDB_E_CONN_CLOSED
YDB_E_FUNC = _ydb.YDB_E_FUNC
YDB_E_HOOK_ADD = _ydb.YDB_E_HOOK_ADD
YDB_E_UNKNOWN_TARGET = _ydb.YDB_E_UNKNOWN_TARGET
YDB_E_DENIED_DELETE = _ydb.YDB_E_DENIED_DELETE

def ydb_res_str(res):
    return _ydb.ydb_res_str(res)
ydb_res_str = _ydb.ydb_res_str

def ydb_connection_log(enable):
    return _ydb.ydb_connection_log(enable)
ydb_connection_log = _ydb.ydb_connection_log

def str2yaml(cstr):
    return _ydb.str2yaml(cstr)
str2yaml = _ydb.str2yaml
class Ydb(_object):
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, Ydb, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, Ydb, name)
    __repr__ = _swig_repr

    def __init__(self, name):
        this = _ydb.new_Ydb(name)
        try:
            self.this.append(this)
        except __builtin__.Exception:
            self.this = this
    __swig_destroy__ = _ydb.delete_Ydb
    __del__ = lambda self: None

    def write(self, yaml):
        return _ydb.Ydb_write(self, yaml)

    def remove(self, yaml):
        return _ydb.Ydb_remove(self, yaml)

    def get(self, *args):
        return _ydb.Ydb_get(self, *args)

    def path_write(self, path):
        return _ydb.Ydb_path_write(self, path)

    def path_remove(self, path):
        return _ydb.Ydb_path_remove(self, path)

    def path_get(self, path):
        return _ydb.Ydb_path_get(self, path)

    def connect(self, addr, flags):
        return _ydb.Ydb_connect(self, addr, flags)

    def disconnect(self, addr):
        return _ydb.Ydb_disconnect(self, addr)

    def is_connected(self, addr):
        return _ydb.Ydb_is_connected(self, addr)

    def fd(self):
        return _ydb.Ydb_fd(self)

    def serve(self, timeout):
        return _ydb.Ydb_serve(self, timeout)

    def path(self, node):
        return _ydb.Ydb_path(self, node)

    def path_and_value(self, node):
        return _ydb.Ydb_path_and_value(self, node)

    def search(self, path):
        return _ydb.Ydb_search(self, path)

    def top(self):
        return _ydb.Ydb_top(self)

    def empty(self, node):
        return _ydb.Ydb_empty(self, node)

    def find(self, base, path):
        return _ydb.Ydb_find(self, base, path)

    def up(self, node):
        return _ydb.Ydb_up(self, node)

    def down(self, node):
        return _ydb.Ydb_down(self, node)

    def prev(self, node):
        return _ydb.Ydb_prev(self, node)

    def next(self, node):
        return _ydb.Ydb_next(self, node)

    def first(self, node):
        return _ydb.Ydb_first(self, node)

    def last(self, node):
        return _ydb.Ydb_last(self, node)

    def tag(self, node):
        return _ydb.Ydb_tag(self, node)

    def value(self, node):
        return _ydb.Ydb_value(self, node)

    def key(self, node):
        return _ydb.Ydb_key(self, node)

    def index(self, node):
        return _ydb.Ydb_index(self, node)

    def level(self, base, node):
        return _ydb.Ydb_level(self, base, node)

    def path_list(self, depth, path):
        return _ydb.Ydb_path_list(self, depth, path)
Ydb_swigregister = _ydb.Ydb_swigregister
Ydb_swigregister(Ydb)

# This file is compatible with both classic and new-style classes.


