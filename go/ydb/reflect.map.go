package ydb

import (
	"fmt"
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
	if v.Kind() != reflect.Map {
		return reflect.Value{}, false
	}
	mt := v.Type()
	kv, err := mapKeyNew(mt, key)
	if err != nil || !kv.IsValid() {
		return reflect.Value{}, false
	}
	ev := v.MapIndex(kv)
	if !ev.IsValid() {
		return reflect.Value{}, false
	}
	return ev, true
}

// ValMapSet - Set an element to the map.
func ValMapSet(mv reflect.Value, key interface{}, element interface{}) error {
	if mv.Kind() != reflect.Map {
		return fmt.Errorf("not a map")
	}
	mt := mv.Type()
	kv, err := mapKeyNew(mt, key)
	if err != nil || !kv.IsValid() {
		return err
	}
	var ev reflect.Value
	var et reflect.Type
	if IsTypeInterface(mt.Elem()) { // That means it is not a specified type.
		if element == nil {
			return fmt.Errorf("not specified key or element type for %s", key)
		}
		et = reflect.TypeOf(element)
	} else {
		et = mt.Elem()
	}
	ev, err = ValNew(et, element)
	if err != nil || !ev.IsValid() {
		return fmt.Errorf("invalid element: %s", element)
	}
	mv.SetMapIndex(kv, ev)
	return nil
}

// ValMapUnset - Remove an element from the map
func ValMapUnset(mv reflect.Value, key interface{}) error {
	if mv.Kind() != reflect.Map {
		return fmt.Errorf("not a map")
	}
	mt := mv.Type()
	kv, err := mapKeyNew(mt, key)
	if err != nil || !kv.IsValid() {
		return fmt.Errorf("invalid key: %s", key)
	}
	mv.SetMapIndex(kv, reflect.Value{})
	return nil
}

// mapKeyNew - used to support structure key.
// The key value must be a string with the following format.
// StructName[StructField1=Value1][StructField2=Value2]
func mapKeyNew(mt reflect.Type, key interface{}) (reflect.Value, error) {
	kt := mt.Key()
	if IsTypeInterface(kt) { // That means it is not a specified type.
		kt = reflect.TypeOf(key)
	}
	if IsTypeScalar(kt) && IsTypeInterface(mt.Elem()) {
		// If the map element type is undefined as empty interface{}
		// and the map key is scalar, disable structure key generation.
		return ValNew(kt, key)
	}
	kv, err := StrKeyValNew(kt, key)
	if err != nil || !kv.IsValid() {
		return reflect.Value{}, err
	}
	return kv, nil
}
