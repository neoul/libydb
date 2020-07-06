package ydb

import (
	"fmt"
	"reflect"
)

var (
	// EnableTagLookup - Enables the tag lookup of struct fields for searching value
	EnableTagLookup bool = true
	// TagLookupKey - the key of the struct field tag to search a value.
	TagLookupKey string = "path"
	// InitChildenOnSet - initalizes all child values on set.
	InitChildenOnSet bool = false
)

// SearchType - Search option for slice (list) value
type SearchType int

const (
	// NoSearch - Do not search of the content of the slice value.
	NoSearch = iota
	// GetLastEntry - return the last entry of the slice value.
	GetLastEntry
	// GetFirstEntry - return the first entry of the slice value.
	GetFirstEntry
	// SearchByIndex - return an entry of n th index from the slice value.
	SearchByIndex
	// SearchByContent - search an entry by this content.
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
			return reflect.Value{}, false
		}
		rv, ok := ValStructFieldFindInDepth(cur, key, searchtype)
		if !ok {
			return reflect.Value{}, false
		}
		cur = rv
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

// ValGetAll - Get All child values from the struct, map or slice value
func ValGetAll(v reflect.Value) ([]reflect.Value, bool) {
	rval := []reflect.Value{}
	if !v.IsValid() {
		return rval, false
	}
	switch v.Kind() {
	case reflect.Ptr, reflect.Interface:
		return ValGetAll(v.Elem())
	case reflect.Struct:
		for i := 0; i < v.NumField(); i++ {
			fv := v.Field(i)
			if !fv.IsValid() || !fv.CanSet() {
				continue
			}
			rval = append(rval, fv)
		}
	case reflect.Map:
		iter := v.MapRange()
		for iter.Next() {
			cv := iter.Value()
			rval = append(rval, cv)
		}
	case reflect.Slice:
		length := v.Len()
		for i := 0; i < length; i++ {
			cv := v.Index(i)
			rval = append(rval, cv)
		}
	default:
		return rval, false
	}
	return rval, true
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
		rv, ok := ValStructFieldFindInDepth(cur, key, searchtype)
		if !ok {
			return reflect.Value{}, false
		}
		if IsNilOrInvalidValue(rv) {
			err := ValStructFieldSet(cur, key, nil)
			if err != nil {
				return reflect.Value{}, false
			}
			rv, ok = ValStructFieldFindInDepth(cur, key, searchtype)
			if !ok {
				return reflect.Value{}, false
			}
		}
		cur = rv
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
		if searchtype == GetLastEntry || searchtype == GetFirstEntry {
			len := cur.Len()
			if len <= 0 {
				_, err := ValSliceAppend(cur, key)
				if err != nil {
					return reflect.Value{}, false
				}
				len = cur.Len()
			}
			if searchtype == GetFirstEntry {
				key = 0
			} else {
				key = len - 1
			}
		} else if searchtype == SearchByContent {
			idx, ok := ValSliceFind(cur, key)
			if !ok {
				_, err := ValSliceAppend(cur, key)
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
func ValChildSet(pv reflect.Value, key interface{}, val interface{}, insertType SearchType) (reflect.Value, error) {
	if !pv.IsValid() {
		return pv, fmt.Errorf("invalid parent value")
	}
	switch pv.Kind() {
	case reflect.Interface:
		return ValChildSet(pv.Elem(), key, val, insertType)
	case reflect.Ptr:
		rv, err := ValChildSet(pv.Elem(), key, val, insertType)
		if err != nil {
			return pv, err
		}
		if rv != pv.Elem() {
			if pv.CanSet() {
				nrv := newPtrOfValue(rv)
				pv.Set(nrv)
				return pv, err
			}
			return newPtrOfValue(rv), err
		}
		return pv, err
	case reflect.Struct:
		if emptykey(key) {
			return pv, nil
		}
		err := ValStructFieldSet(pv, key, val)
		if err != nil {
			return pv, fmt.Errorf("set failed in structure set (%v)", err)
		}
	case reflect.Map:
		if emptykey(key) {
			return pv, nil
		}
		err := ValMapSet(pv, key, val)
		if err != nil {
			return pv, fmt.Errorf("set failed in map set (%v)", err)
		}
	case reflect.Slice, reflect.Array:
		if insertType == NoSearch {
			vv, err := ValSliceAppend(pv, val)
			if err != nil {
				return pv, err
			}
			return vv, nil
		} else if insertType == SearchByContent {
			_, ok := ValSliceFind(pv, key)
			if !ok {
				vv, err := ValSliceAppend(pv, key)
				if err != nil {
					return pv, err
				}
				return vv, nil
			}
		} else {
			return pv, fmt.Errorf("not supported insert option")
		}
	default:
		return pv, fmt.Errorf("not container type %s", pv.Kind())
	}
	return pv, nil
}

// ValChildUnset - Unset a child value indicated by the key from parents
func ValChildUnset(v reflect.Value, key interface{}, deleteType SearchType) (reflect.Value, error) {
	if !v.IsValid() {
		return v, fmt.Errorf("invalid value")
	}
	switch v.Kind() {
	case reflect.Interface:
		return ValChildUnset(v.Elem(), key, deleteType)
	case reflect.Ptr:
		rv, err := ValChildUnset(v.Elem(), key, deleteType)
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
		return v, fmt.Errorf("not supported value type (%s)", v.Kind())
	case reflect.Struct:
		return v, ValStructFieldUnset(v, key)
	case reflect.Map:
		return v, ValMapUnset(v, key)
	case reflect.Slice:
		if deleteType == SearchByContent {
			if index, ok := ValSliceFind(v, key); ok {
				return ValSliceDelete(v, index)
			}
			return v, fmt.Errorf("not found unset value")
		} else if deleteType == NoSearch {
			return ValSliceDelete(v, 0)
		}
		return v, fmt.Errorf("invalid delete option")
	default:
		return v, fmt.Errorf("not supported scalar value unset")
	}
}

// ValChildDirectSet - Set a child value to the parent value.
func ValChildDirectSet(pv reflect.Value, key interface{}, cv reflect.Value) (reflect.Value, error) {
	switch pv.Kind() {
	case reflect.Interface:
		return ValChildDirectSet(pv.Elem(), key, cv)
	case reflect.Ptr:
		rv, err := ValChildDirectSet(pv.Elem(), key, cv)
		if err != nil {
			return pv, err
		}
		if rv != pv.Elem() {
			if pv.CanSet() {
				nrv := newPtrOfValue(rv)
				pv.Set(nrv)
				return pv, err
			}
			return newPtrOfValue(rv), err
		}
		return pv, err
	case reflect.Array, reflect.Complex64, reflect.Complex128, reflect.Chan:
		return pv, fmt.Errorf("not supported parent type (%s)", pv.Type().Kind())
	case reflect.Struct:
		_, fv, ok := ValStructFieldFind(pv, key)
		if !ok {
			return pv, fmt.Errorf("not found %s", key)
		}
		fv.Set(cv)
	case reflect.Map:
		t := pv.Type()
		kt := t.Key()
		if IsTypeInterface(kt) { // That means it is not a specified type.
			kt = reflect.TypeOf(key)
		}
		kv, err := ValScalarNew(kt, key)
		if err != nil || !kv.IsValid() {
			return pv, fmt.Errorf("invalid key: %s", key)
		}
		pv.SetMapIndex(kv, cv)
	case reflect.Slice:
		if !pv.CanSet() {
			tempv := reflect.MakeSlice(pv.Type(), pv.Len(), pv.Len())
			reflect.Copy(tempv, pv)
			tempv = reflect.Append(tempv, cv)
			return tempv, nil
		}
		pv.Set(reflect.Append(pv, cv))
	default:
		if !pv.CanSet() {
			tempv := reflect.New(pv.Type()).Elem()
			tempv.Set(cv)
			return tempv, nil
		}
		pv.Set(cv)
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
func ValNew(t reflect.Type, val interface{}) (reflect.Value, error) {
	if t == reflect.TypeOf(nil) {
		return reflect.Value{}, fmt.Errorf("nil type")
	}
	switch t.Kind() {
	case reflect.Ptr:
		cv, err := ValNew(t.Elem(), val)
		return newPtrOfValue(cv), err
	case reflect.Interface:
		return ValNew(t.Elem(), val)
	case reflect.Array, reflect.Complex64, reflect.Complex128, reflect.Chan:
		return reflect.Value{}, fmt.Errorf("not supported type: %s", t.Kind())
	case reflect.Struct:
		return ValStructNew(t, val, InitChildenOnSet)
	case reflect.Map: // [FIXME]
		return reflect.MakeMap(t), nil
	case reflect.Slice: // [FIXME]
		return reflect.MakeSlice(t, 0, 0), nil
	default:
		return ValScalarNew(t, val)
	}
}
