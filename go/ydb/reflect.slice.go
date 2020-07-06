package ydb

import (
	"fmt"
	"reflect"
)

// slice operations
// The settable slice is the pointed by ptr value or structField.

// SliceFind - Search an element by key.
func SliceFind(slice interface{}, key interface{}) (int, bool) {
	v := reflect.ValueOf(slice)
	return ValSliceFind(v, key)
}

// SliceDelete - Delete an element indexed by i.
func SliceDelete(slice interface{}, i int) error {
	v := reflect.ValueOf(slice).Elem()
	if v.CanSet() {
		_, err := ValSliceDelete(v, i)
		return err
	}
	return fmt.Errorf("not settable value")
}

// SliceInsert - Insert an element to the index.
func SliceInsert(slice interface{}, i int, val interface{}) error {
	v := reflect.ValueOf(slice).Elem()
	if v.CanSet() {
		_, err := ValSliceInsert(v, i, val)
		return err
	}
	return fmt.Errorf("not settable value")
}

// slice operations for reflect.Value

// ValSliceVal2Element - Search an element by value.
func ValSliceVal2Element(v reflect.Value, val interface{}) reflect.Value {
	et := v.Type().Elem()
	if IsTypeInterface(et) { // That means it is not a specified type.
		et = reflect.TypeOf(val)
	}
	ev, _ := ValNew(et, val)
	return ev
}

// ValSliceFind - Search an element by value.
func ValSliceFind(v reflect.Value, val interface{}) (int, bool) {
	ev := ValSliceVal2Element(v, val)
	length := v.Len()
	for i := 0; i < length; i++ {
		if reflect.DeepEqual(v.Index(i).Interface(), ev.Interface()) {
			return i, true
		}
	}
	return length, false
}

// ValSliceIndex - Get an element by index.
func ValSliceIndex(v reflect.Value, index interface{}) (reflect.Value, bool) {
	i, ok := index.(int)
	if !ok {
		idxv, err := ValScalarNew(reflect.TypeOf(0), index)
		if !idxv.IsValid() || err != nil {
			return reflect.Value{}, false
		}
		i = idxv.Interface().(int)
	}
	if v.Len() <= i {
		return reflect.Value{}, false
	}
	ev := v.Index(i)
	if !ev.IsValid() {
		return reflect.Value{}, false
	}
	return ev, true
}

// ValSliceDelete - Delete an element indexed by i. If the slice is not settable, return a new slice.
func ValSliceDelete(v reflect.Value, i int) (reflect.Value, error) {
	if v.Kind() != reflect.Slice {
		return v, fmt.Errorf("not slice")
	}
	if i >= v.Len() || i < 0 {
		return v, fmt.Errorf("invalid index")
	}
	if v.CanSet() {
		v.Set(reflect.AppendSlice(v.Slice(0, i), v.Slice(i+1, v.Len())))
		return v, nil
	}
	tmp := reflect.MakeSlice(v.Type(), 0, v.Len()-1)
	tmp = reflect.AppendSlice(
		reflect.AppendSlice(tmp, v.Slice(0, i)),
		v.Slice(i+1, v.Len()))
	return tmp, nil
}

// ValSliceInsert - Insert an element to the index. If the slice is not settable, return a new slice.
func ValSliceInsert(v reflect.Value, i int, val interface{}) (reflect.Value, error) {
	if v.Kind() != reflect.Slice {
		return v, fmt.Errorf("not slice")
	}
	if i > v.Len() || i < 0 {
		return v, fmt.Errorf("invalid index")
	}
	et := v.Type().Elem()
	if IsTypeInterface(et) { // That means it is not a specified type.
		et = reflect.TypeOf(val)
	}
	var err error
	var ev reflect.Value
	ev, err = ValNew(et, val)
	if err != nil || !ev.IsValid() {
		return v, fmt.Errorf("invalid element (%v)", err)
	}
	if v.CanSet() {
		if i >= v.Len() {
			v.Set(reflect.Append(v, ev))
		} else {
			if i < 0 {
				i = 0
			}
			v.Set(reflect.AppendSlice(v.Slice(0, i+1), v.Slice(i, v.Len())))
			v.Index(i).Set(ev)
		}
		return v, nil
	}
	if i >= v.Len() {
		tmp := reflect.MakeSlice(v.Type(), v.Len(), v.Len())
		reflect.Copy(tmp, v)
		return reflect.Append(tmp, ev), nil
	}
	if i < 0 {
		i = 0
	}
	tmp := reflect.MakeSlice(v.Type(), 0, v.Len()+1)
	tmp = reflect.AppendSlice(
		reflect.AppendSlice(tmp, v.Slice(0, i+1)),
		v.Slice(i, v.Len()))
	tmp.Index(i).Set(ev)
	return tmp, nil
}

// ValSliceAppend - Insert an element to the index.
func ValSliceAppend(v reflect.Value, val interface{}) (reflect.Value, error) {
	if v.Kind() != reflect.Slice {
		return v, fmt.Errorf("not slice")
	}

	et := v.Type().Elem()
	if IsTypeInterface(et) { // That means it is not a specified type.
		et = reflect.TypeOf(val)
	}
	var err error
	var ev reflect.Value
	ev, err = ValNew(et, val)
	if err != nil || !ev.IsValid() {
		return v, fmt.Errorf("invalid element (%v)", err)
	}
	if v.CanSet() {
		v.Set(reflect.Append(v, ev))
		return v, nil
	}
	tmp := reflect.MakeSlice(v.Type(), v.Len(), v.Len())
	reflect.Copy(tmp, v)
	return reflect.Append(tmp, ev), nil
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
