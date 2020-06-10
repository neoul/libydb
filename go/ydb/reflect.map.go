package ydb

import (
	"reflect"
)

// map operations
// The settable m is the pointed by ptr value or structField.

// MapSearch - Search an element by key.
func MapSearch(m interface{}, key interface{}) (reflect.Value, bool) {
	v := reflect.ValueOf(m)
	return ValMapSearch(v, key)
}

// map operations for reflect.Value

// ValMapSearch - Search an element by the key.
func ValMapSearch(v reflect.Value, key interface{}) (reflect.Value, bool) {
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
