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

// ValSliceVal2Element - Search an element by value.
func ValSliceVal2Element(v reflect.Value, val interface{}) reflect.Value {
	et := v.Type().Elem()
	if IsTypeInterface(et) { // That means it is not a specified type.
		et = reflect.TypeOf(val)
	}
	ev, _ := ValScalarNew(et, val)
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
	if !v.CanSet() {
		return fmt.Errorf("not settable value")
	}
	et := v.Type().Elem()
	if IsTypeInterface(et) { // That means it is not a specified type.
		et = reflect.TypeOf(val)
	}
	var err error
	var ev reflect.Value
	if IsTypeScalar(et) {
		ev, err = ValScalarNew(et, val)
	} else {
		ev, err = ValNew(et)
	}
	if err != nil || !ev.IsValid() {
		return fmt.Errorf("invalid element (%v)", err)
	}

	if i >= v.Len() {
		v.Set(reflect.Append(v, ev))
	} else {
		if i < 0 {
			i = 0
		}
		v.Set(reflect.AppendSlice(v.Slice(0, i+1), v.Slice(i, v.Len())))
		v.Index(i).Set(ev)
	}
	return nil
}

// ValSliceAppend - Insert an element to the index.
func ValSliceAppend(v reflect.Value, val interface{}) error {
	if !v.CanSet() {
		return fmt.Errorf("not settable value")
	}
	et := v.Type().Elem()
	if IsTypeInterface(et) { // That means it is not a specified type.
		et = reflect.TypeOf(val)
	}
	var err error
	var ev reflect.Value
	if IsTypeScalar(et) {
		ev, err = ValScalarNew(et, val)
	} else {
		ev, err = ValNew(et)
	}
	if err != nil || !ev.IsValid() {
		return fmt.Errorf("invalid element (%v)", err)
	}
	v.Set(reflect.Append(v, ev))
	return nil
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
	if !v.CanSet() {
		return fmt.Errorf("not settable value")
	}
	et := v.Type().Elem()
	if IsTypeInterface(et) { // That means it is not a specified type.
		et = reflect.TypeOf(val)
	}
	var err error
	var ev reflect.Value
	if IsTypeScalar(et) {
		ev, err = ValScalarNew(et, val)
	} else {
		ev, err = ValNew(et)
	}
	if err != nil || !ev.IsValid() {
		return fmt.Errorf("invalid element (%v)", err)
	}
	if i >= v.Len() {
		tmp := reflect.MakeSlice(v.Type(), v.Len(), v.Len())
		reflect.Copy(tmp, v)
		v.Set(reflect.Append(tmp, ev))
	} else {
		if i < 0 {
			i = 0
		}
		tmp := reflect.MakeSlice(v.Type(), 0, v.Len()+1)
		v.Set(reflect.AppendSlice(
			reflect.AppendSlice(tmp, v.Slice(0, i+1)),
			v.Slice(i, v.Len())))

		v.Index(i).Set(ev)
	}
	return nil
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
