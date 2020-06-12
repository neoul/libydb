package ydb

import (
	"reflect"
)

// map operations
// The settable m is the pointed by ptr value or structField.

// MapFind - Search an element by key.
func MapFind(m interface{}, key interface{}) (reflect.Value, bool) {
	v := reflect.ValueOf(m)
	return ValMapFind(v, key)
}

// map operations for reflect.Value

// ValMapFind - Search an element by the key.
func ValMapFind(v reflect.Value, key interface{}) (reflect.Value, bool) {
	mt := v.Type()
	kt := mt.Key()

	kv := newValue(kt, key)
	if !kv.IsValid() {
		return reflect.Value{}, false
	}
	ev := v.MapIndex(kv)
	if !ev.IsValid() {
		return reflect.Value{}, false
	}
	return ev, true
}
