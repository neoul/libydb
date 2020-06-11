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
	return ValSliceSearch(v, key)
}

// SliceDelete - Delete an element indexed by i.
func SliceDelete(slice interface{}, i int) error {
	v := reflect.ValueOf(slice).Elem()
	return ValSliceDelete(v, i)
}

// SliceInsert - Insert an element to the index.
func SliceInsert(slice interface{}, i int, val interface{}) error {
	v := reflect.ValueOf(slice).Elem()
	return ValSliceInsert(v, i, val)
}

// SliceDeleteCopy - Delete an element indexed by i.
func SliceDeleteCopy(slice interface{}, i int) error {
	v := reflect.ValueOf(slice).Elem()
	return ValSliceDeleteCopy(v, i)
}

// SliceInsertCopy - Insert an element to the index i.
func SliceInsertCopy(slice interface{}, i int, val interface{}) error {
	v := reflect.ValueOf(slice).Elem()
	return ValSliceInsertCopy(v, i, val)
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
	return fmt.Errorf("not settable value")
}

// ValSliceInsert - Insert an element to the index.
func ValSliceInsert(v reflect.Value, i int, val interface{}) error {
	if v.CanSet() {
		v.Set(reflect.AppendSlice(v.Slice(0, i+1), v.Slice(i, v.Len())))
		// v.Index(i).Set(reflect.ValueOf(val))
		ev := newValue(v.Type().Elem(), val)
		v.Index(i).Set(ev)
		return nil
	}
	return fmt.Errorf("not settable value")
}

// ValSliceDeleteCopy - Delete an element indexed by i.
func ValSliceDeleteCopy(v reflect.Value, i int) error {
	if v.CanSet() {
		tmp := reflect.MakeSlice(v.Type(), 0, v.Len()-1)
		v.Set(
			reflect.AppendSlice(
				reflect.AppendSlice(tmp, v.Slice(0, i)),
				v.Slice(i+1, v.Len())))
		return nil
	}
	return fmt.Errorf("not settable value")
}

// ValSliceInsertCopy - Insert an element to the index i.
func ValSliceInsertCopy(v reflect.Value, i int, val interface{}) error {
	if v.CanSet() {
		tmp := reflect.MakeSlice(v.Type(), 0, v.Len()+1)
		v.Set(reflect.AppendSlice(
			reflect.AppendSlice(tmp, v.Slice(0, i+1)),
			v.Slice(i, v.Len())))
		// v.Index(i).Set(reflect.ValueOf(val))
		ev := newValue(v.Type().Elem(), val)
		v.Index(i).Set(ev)
		return nil
	}
	return fmt.Errorf("not settable value")
}

// ValSliceNew - Create a slice.
func ValSliceNew(t reflect.Type) (reflect.Value, error) {
	if t == reflect.TypeOf(nil) {
		return reflect.Value{}, fmt.Errorf("nil type")
	}
	if t.Kind() != reflect.Slice {
		return reflect.Value{}, fmt.Errorf("not slice")
	}
	v := reflect.MakeSlice(t, 0, 0)
	return v, nil
}
