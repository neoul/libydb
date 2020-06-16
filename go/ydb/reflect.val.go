package ydb

import (
	"fmt"
	"reflect"
)

// SearchType - Search option
type SearchType int

const (
	GetLastEntry = iota
	GetFirstEntry
	SearchByIndex
	SearchByContent
)

// IsValScalar - true if built-in simple variable type
func IsValScalar(v reflect.Value) bool {
	switch v.Kind() {
	case reflect.Ptr, reflect.Interface:
		return IsValScalar(v.Elem())
	case reflect.Array, reflect.Slice, reflect.Map, reflect.Struct:
		return false
	default:
		return true
	}
}

func emptykey(key interface{}) bool {
	kk, ok := key.(string)
	if ok {
		if kk == "" {
			return true
		}
	}
	return false
}

// ValFindByContent - enable content search for slice in ValFind and ValFindOrInit.
var ValFindByContent bool = false

// ValFind - finds a child value from the struct, map or slice value using the key.
func ValFind(v reflect.Value, key interface{}, searchtype SearchType) (reflect.Value, bool) {
	if !v.IsValid() {
		return reflect.Value{}, false
	}
	cur := v
	switch cur.Kind() {
	case reflect.Ptr, reflect.Interface:
		return ValFind(cur.Elem(), key, searchtype)
	case reflect.Struct:
		if emptykey(key) {
			return v, true
		}
		_, sfv, ok := ValStructFieldFind(cur, key)
		if !ok {
			return reflect.Value{}, false
		}
		cur = sfv
	case reflect.Map:
		if emptykey(key) {
			return reflect.Value{}, false
		}
		ev, ok := ValMapFind(cur, key)
		if !ok {
			return reflect.Value{}, false
		}
		cur = ev
	case reflect.Slice:
		if searchtype == GetLastEntry {
			len := cur.Len()
			if len <= 0 {
				return reflect.Value{}, false
			}
			key = len - 1
		} else if searchtype == GetFirstEntry {
			len := cur.Len()
			if len <= 0 {
				return reflect.Value{}, false
			}
			key = 0
		} else if searchtype == SearchByContent {
			idx, ok := ValSliceFind(cur, key)
			if !ok {
				return reflect.Value{}, false
			}
			key = idx
		} else if searchtype == SearchByIndex {

		} else {
			return reflect.Value{}, false
		}
		ev, ok := ValSliceIndex(cur, key)
		if !ok {
			return reflect.Value{}, false
		}
		cur = ev
	default:
		return reflect.Value{}, false
	}
	return cur, true
}

// ValFindOrInit - finds or initializes a child value if it is not exists.
func ValFindOrInit(v reflect.Value, key interface{}, searchtype SearchType) (reflect.Value, bool) {
	if !v.IsValid() {
		return reflect.Value{}, false
	}
	cur := v
	switch cur.Kind() {
	case reflect.Ptr, reflect.Interface:
		return ValFindOrInit(cur.Elem(), key, searchtype)
	case reflect.Struct:
		if emptykey(key) {
			return reflect.Value{}, false
		}
		_, sfv, ok := ValStructFieldFind(cur, key)
		if !ok {
			return reflect.Value{}, false
		}
		if IsNilOrInvalidValue(sfv) {
			err := ValStructFieldSet(cur, key, nil)
			if err != nil {
				return reflect.Value{}, false
			}
			_, sfv, ok = ValStructFieldFind(cur, key)
			if !ok {
				return reflect.Value{}, false
			}
		}
		cur = sfv
	case reflect.Map:
		if emptykey(key) {
			return reflect.Value{}, false
		}
		ev, ok := ValMapFind(cur, key)
		if !ok {
			err := ValMapSet(cur, key, nil)
			if err != nil {
				return reflect.Value{}, false
			}
			ev, ok = ValMapFind(cur, key)
			if !ok {
				return reflect.Value{}, false
			}
		}
		cur = ev
	case reflect.Slice:
		if searchtype == GetLastEntry {
			len := cur.Len()
			if len <= 0 {
				return reflect.Value{}, false
			}
			key = len - 1
		} else if searchtype == GetFirstEntry {
			len := cur.Len()
			if len <= 0 {
				return reflect.Value{}, false
			}
			key = 0
		} else if searchtype == SearchByContent {
			idx, ok := ValSliceFind(cur, key)
			if !ok {
				err := ValSliceAppend(cur, key)
				if err != nil {
					return reflect.Value{}, false
				}
				idx = cur.Len() - 1
			}
			key = idx
		} else if searchtype == SearchByIndex {

		} else {
			return reflect.Value{}, false
		}
		ev, ok := ValSliceIndex(cur, key)
		if !ok {
			return reflect.Value{}, false
		}
		cur = ev
	default:
		return reflect.Value{}, false
	}
	return cur, true
}

// ValChildSet - finds and sets a child value.
func ValChildSet(pv reflect.Value, key interface{}, val interface{}) (reflect.Value, error) {
	if !pv.IsValid() {
		return reflect.Value{}, fmt.Errorf("invalid parent value")
	}
	cur := pv
	switch cur.Kind() {
	case reflect.Ptr, reflect.Interface:
		return ValChildSet(cur.Elem(), key, val)
	case reflect.Struct:
		if emptykey(key) {
			return cur, nil
		}
		_, sfv, ok := ValStructFieldFind(cur, key)
		if !ok {
			return reflect.Value{}, fmt.Errorf("not found %s", key)
		}
		err := ValStructFieldSet(cur, key, val)
		if err != nil {
			return reflect.Value{}, fmt.Errorf("set failed (%v)", err)
		}
		_, sfv, ok = ValStructFieldFind(cur, key)
		if !ok {
			return reflect.Value{}, fmt.Errorf("not found %s", key)
		}
		cur = sfv
	case reflect.Map:
		if emptykey(key) {
			return cur, nil
		}
		ev, ok := ValMapFind(cur, key)
		if !ok {
			err := ValMapSet(cur, key, val)
			if err != nil {
				return reflect.Value{}, fmt.Errorf("set failed (%v)", err)
			}
			ev, ok = ValMapFind(cur, key)
			if !ok {
				return reflect.Value{}, fmt.Errorf("not found %s", key)
			}
		}
		cur = ev
	case reflect.Slice, reflect.Array:
		var ev reflect.Value
		var ok bool
		var idx int
		if ValFindByContent {
			idx, ok = ValSliceFind(cur, key)
			if !ok {
				err := ValSliceAppend(cur, key)
				if err != nil {
					return reflect.Value{}, fmt.Errorf("set failed (%v)", err)
				}
				idx = cur.Len() - 1
			}
			ev, ok = ValSliceIndex(cur, idx)
		} else {
			ev, ok = ValSliceIndex(cur, key)
		}
		if !ok {
			return reflect.Value{}, fmt.Errorf("set failed")
		}
		cur = ev
	default:
		return reflect.Value{}, fmt.Errorf("not container type %s", cur.Kind())
	}
	return cur, nil
}

// ValChildUnset - Unset a child value indicated by the key from parents
func ValChildUnset(v reflect.Value, key interface{}) error {
	if !v.IsValid() {
		return fmt.Errorf("invalid value")
	}

	t := v.Type()
	switch t.Kind() {
	case reflect.Ptr, reflect.Interface:
		return ValChildUnset(v.Elem(), key)
	case reflect.Array, reflect.Complex64, reflect.Complex128, reflect.Chan:
		return fmt.Errorf("not supported value type (%s)", t.Kind())
	case reflect.Struct:
		return ValStructFieldUnset(v, key)
	case reflect.Map:
		return ValMapUnset(v, key)
	case reflect.Slice:
		if ValFindByContent {
			if index, ok := ValSliceFind(v, key); ok {
				return ValSliceDelete(v, index)
			}
		} else {
			var index int
			if reflect.TypeOf(key).Kind() == reflect.Int {
				index = key.(int)
			} else {
				idxv, err := ValScalarNew(reflect.TypeOf(0), key)
				if !idxv.IsValid() || err != nil {
					return err
				}
				index = idxv.Interface().(int)
			}
			return ValSliceDelete(v, index)
		}
		return fmt.Errorf("not found unset value")
	default:
		return fmt.Errorf("not supported scalar value unset")
	}
}

// ValChildDirectSet - Set a child value to the parent value.
func ValChildDirectSet(pv reflect.Value, key interface{}, cv reflect.Value) (reflect.Value, error) {
	v := pv
	switch v.Type().Kind() {
	case reflect.Interface:
		return ValChildDirectSet(v.Elem(), key, cv)
	case reflect.Ptr:
		rv, err := ValChildDirectSet(v.Elem(), key, cv)
		if err != nil {
			return v, err
		}
		if rv != v.Elem() {
			if v.CanSet() {
				nrv := newPtrOfValue(rv)
				v.Set(nrv)
				return v, err
			}
			return newPtrOfValue(rv), err
		}
		return v, err
	case reflect.Array, reflect.Complex64, reflect.Complex128, reflect.Chan:
		return pv, fmt.Errorf("not supported parent type (%s)", v.Type().Kind())
	case reflect.Struct:
		_, fv, ok := ValStructFieldFind(v, key)
		if !ok {
			return pv, fmt.Errorf("not found %s", key)
		}
		fv.Set(cv)
	case reflect.Map:
		t := v.Type()
		kt := t.Key()
		if IsTypeInterface(kt) { // That means it is not a specified type.
			kt = reflect.TypeOf(key)
		}
		kv, err := ValScalarNew(kt, key)
		if err != nil || !kv.IsValid() {
			return pv, fmt.Errorf("invalid key: %s", key)
		}
		v.SetMapIndex(kv, cv)
	case reflect.Slice:
		if !v.CanSet() {
			tempv := reflect.MakeSlice(v.Type(), v.Len(), v.Len())
			reflect.Copy(tempv, v)
			tempv = reflect.Append(tempv, cv)
			return tempv, nil
		}
		v.Set(reflect.Append(v, cv))
	default:
		if !v.CanSet() {
			tempv := reflect.New(v.Type()).Elem()
			tempv.Set(cv)
			return tempv, nil
		}
		v.Set(cv)
	}
	return pv, nil
}

// ValScalarNew - Create a new value of the t type.
func ValScalarNew(t reflect.Type, val interface{}) (reflect.Value, error) {
	if t == reflect.TypeOf(nil) {
		return reflect.Value{}, fmt.Errorf("nil type")
	}
	switch t.Kind() {
	case reflect.Ptr:
		cv, err := ValScalarNew(t.Elem(), val)
		return newPtrOfValue(cv), err
	case reflect.Interface:
		return reflect.Value{}, fmt.Errorf("no specified type: %s", t.Kind())
	case reflect.Array, reflect.Complex64, reflect.Complex128, reflect.Chan:
		return reflect.Value{}, fmt.Errorf("not supported type: %s", t.Kind())
	case reflect.Struct, reflect.Map, reflect.Slice:
		return reflect.Value{}, fmt.Errorf("non-scalar type: %s", t.Kind())
	default:
		nv := newValueScalar(t)
		if nv.IsValid() {
			if val != nil {
				err := setValueScalar(nv, val)
				return nv, err
			}
			return nv, nil
		}
		return reflect.Value{}, fmt.Errorf("not valid")
	}
}

// ValScalarSet - Create a new value to the pointed value, v.
func ValScalarSet(v reflect.Value, val interface{}) error {
	if !v.IsValid() {
		return fmt.Errorf("not valid scalar")
	}
	t := v.Type()
	switch t.Kind() {
	case reflect.Ptr:
		if v.IsNil() {
			if !v.CanSet() {
				return fmt.Errorf("not settable value")
			}
			nv, err := ValScalarNew(v.Type(), val)
			if err != nil {
				return err
			}
			v.Set(nv) // sets v to new value, nv.
			return nil
		}
		return ValScalarSet(v.Elem(), val)
	case reflect.Interface:
		return ValScalarSet(v.Elem(), val)
	case reflect.Array, reflect.Complex64, reflect.Complex128, reflect.Chan:
		return fmt.Errorf("not supported type: %s", t.Kind())
	case reflect.Struct, reflect.Map, reflect.Slice:
		return fmt.Errorf("non-scalar type: %s", t.Kind())
	default:
		if !v.CanSet() {
			return fmt.Errorf("not settable value")
		}
		if val == nil {
			nv, err := ValScalarNew(v.Type(), nil)
			if err == nil && nv.IsValid() {
				v.Set(nv)
			}
		}
		return setValueScalar(v, val)
	}
}

// ValNew - Create a new value (struct, map, slice) of the t type.
func ValNew(t reflect.Type) (reflect.Value, error) {
	if t == reflect.TypeOf(nil) {
		return reflect.Value{}, fmt.Errorf("nil type")
	}
	switch t.Kind() {
	case reflect.Ptr:
		cv, err := ValNew(t.Elem())
		return newPtrOfValue(cv), err
	case reflect.Interface:
		return ValNew(t.Elem())
	case reflect.Array, reflect.Complex64, reflect.Complex128, reflect.Chan:
		return reflect.Value{}, fmt.Errorf("not supported type: %s", t.Kind())
	case reflect.Struct:
		return ValStructNew(t)
	case reflect.Map:
		return reflect.MakeMap(t), nil
	case reflect.Slice:
		return reflect.MakeSlice(t, 0, 0), nil
	default:
		return ValScalarNew(t, nil)
	}
}
