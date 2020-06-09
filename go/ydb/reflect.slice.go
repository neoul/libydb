package ydb

import (
	"fmt"
	"reflect"
)

// slice operations
// The settable slice is the pointed by ptr value or structField.

// SliceSearch - Search an element by key.
func SliceSearch(slice interface{}, key interface{}) (int, bool) {
	v := reflect.ValueOf(slice)
	et := v.Type().Elem()
	kv := newValue(et, key)
	length := v.Len()
	for i := 0; i < length; i++ {
		if reflect.DeepEqual(v.Index(i).Interface(), kv.Interface()) {
			return i, true
		}
	}
	return length, false
}

// SliceDelete - Delete an element indexed by i.
func SliceDelete(slice interface{}, i int) {
	v := reflect.ValueOf(slice).Elem()
	v.Set(reflect.AppendSlice(v.Slice(0, i), v.Slice(i+1, v.Len())))
}

// SliceInsert - Insert an element to the index.
func SliceInsert(slice interface{}, i int, val interface{}) {
	v := reflect.ValueOf(slice).Elem()
	v.Set(reflect.AppendSlice(v.Slice(0, i+1), v.Slice(i, v.Len())))
	// v.Index(i).Set(reflect.ValueOf(val))
	ev := newValue(v.Type().Elem(), val)
	v.Index(i).Set(ev)
}

// SliceDeleteCopy - Create a slice without an element indexed by i.
func SliceDeleteCopy(slice interface{}, i int) {
	v := reflect.ValueOf(slice).Elem()
	tmp := reflect.MakeSlice(v.Type(), 0, v.Len()-1)
	v.Set(
		reflect.AppendSlice(
			reflect.AppendSlice(tmp, v.Slice(0, i)),
			v.Slice(i+1, v.Len())))
}

// SliceInsertCopy - Create a slice with an element to the index i.
func SliceInsertCopy(slice interface{}, i int, val interface{}) {
	v := reflect.ValueOf(slice).Elem()
	tmp := reflect.MakeSlice(v.Type(), 0, v.Len()+1)
	v.Set(reflect.AppendSlice(
		reflect.AppendSlice(tmp, v.Slice(0, i+1)),
		v.Slice(i, v.Len())))
	// v.Index(i).Set(reflect.ValueOf(val))
	ev := newValue(v.Type().Elem(), val)
	v.Index(i).Set(ev)
}

// slice operations for reflect.Value

// ValSliceSearch - Search an element by key.
func ValSliceSearch(v reflect.Value, key interface{}) (int, bool) {
	et := v.Type().Elem()
	kv := newValue(et, key)
	length := v.Len()
	for i := 0; i < length; i++ {
		if reflect.DeepEqual(v.Index(i).Interface(), kv.Interface()) {
			return i, true
		}
	}
	return length, false
}

// ValSliceDelete - Delete an element indexed by i.
func ValSliceDelete(v reflect.Value, i int) error {
	if v.CanSet() {
		v.Set(reflect.AppendSlice(v.Slice(0, i), v.Slice(i+1, v.Len())))
		return nil
	}
	return fmt.Errorf("Cannot set value")
}

// ValSliceInsert - Insert an element to the index.
func ValSliceInsert(v reflect.Value, i int, val interface{}) error {
	if v.CanSet() {
		v.Set(reflect.AppendSlice(v.Slice(0, i+1), v.Slice(i, v.Len())))
		v.Index(i).Set(reflect.ValueOf(val))
		return nil
	}
	return fmt.Errorf("Cannot set value")
}

// ValSliceDeleteCopy - Create a slice without an element indexed by i.
func ValSliceDeleteCopy(v reflect.Value, i int) error {
	if v.CanSet() {
		tmp := reflect.MakeSlice(v.Type(), 0, v.Len()-1)
		v.Set(
			reflect.AppendSlice(
				reflect.AppendSlice(tmp, v.Slice(0, i)),
				v.Slice(i+1, v.Len())))
		return nil
	}
	return fmt.Errorf("Cannot set value")
}

// ValSliceInsertCopy - Create a slice with an element to the index i.
func ValSliceInsertCopy(v reflect.Value, i int, val interface{}) error {
	if v.CanSet() {
		tmp := reflect.MakeSlice(v.Type(), 0, v.Len()+1)
		v.Set(reflect.AppendSlice(
			reflect.AppendSlice(tmp, v.Slice(0, i+1)),
			v.Slice(i, v.Len())))
		v.Index(i).Set(reflect.ValueOf(val))
	}
	return fmt.Errorf("Cannot set value")
}
