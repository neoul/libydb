package ydb

import (
	"fmt"
	"reflect"
)

// ValFindByContent - enable content search for slice in ValFind and ValFindOrInit.
var ValFindByContent bool = false

// ValFind - finds a child value from the struct, map or slice value using the key.
func ValFind(v reflect.Value, key interface{}) (reflect.Value, bool) {
	if !v.IsValid() {
		return reflect.Value{}, false
	}
	cur := v
	if key == "" {
		return cur, true
	}
	switch cur.Kind() {
	case reflect.Ptr, reflect.Interface:
		return ValFind(cur.Elem(), key)
	case reflect.Struct:
		_, sfv, ok := ValStructFieldFind(cur, key)
		if !ok {
			return reflect.Value{}, false
		}
		cur = sfv
	case reflect.Map:
		ev, ok := ValMapFind(cur, key)
		if !ok {
			return reflect.Value{}, false
		}
		cur = ev
	case reflect.Slice, reflect.Array:
		var ev reflect.Value
		var ok bool
		var idx int
		if ValFindByContent {
			idx, ok = ValSliceFind(cur, key)
			if !ok {
				return reflect.Value{}, false
			}
			ev, ok = ValSliceIndex(cur, idx)
		} else {
			ev, ok = ValSliceIndex(cur, key)
		}
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
func ValFindOrInit(v reflect.Value, key interface{}) (reflect.Value, bool) {
	if !v.IsValid() {
		return reflect.Value{}, false
	}
	cur := v
	if key == "" {
		return cur, true
	}
	switch cur.Kind() {
	case reflect.Ptr, reflect.Interface:
		return ValFindOrInit(cur.Elem(), key)
	case reflect.Struct:
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
	case reflect.Slice, reflect.Array:
		var ev reflect.Value
		var ok bool
		var idx int
		if ValFindByContent {
			idx, ok = ValSliceFind(cur, key)
			if !ok {
				err := ValSliceAppend(cur, key)
				if err != nil {
					return reflect.Value{}, false
				}
				idx = cur.Len() - 1
			}
			ev, ok = ValSliceIndex(cur, idx)
		} else {
			ev, ok = ValSliceIndex(cur, key)
		}
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
	if key == "" {
		return cur, nil
	}
	switch cur.Kind() {
	case reflect.Ptr, reflect.Interface:
		return ValChildSet(cur.Elem(), key, val)
	case reflect.Struct:
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
	log.Debug("SetValueChild", pv.Type(), cv.Type(), key)
	v := GetNonIfOrPtrValueDeep(pv)
	switch v.Type().Kind() {
	case reflect.Array, reflect.Complex64, reflect.Complex128, reflect.Chan, reflect.Interface:
		return pv, fmt.Errorf("Not supported parent type (%s)", v.Type().Kind())
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
			return reflect.Value{}, fmt.Errorf("invalid key: %s", key)
		}
		v.SetMapIndex(kv, cv)
	case reflect.Slice:
		v.Set(reflect.Append(v, cv))
	default:
		if !pv.CanSet() {
			return pv, fmt.Errorf("Not settable type(%s)", pv.Type())
		}
		pv.Set(cv)
	}
	return pv, nil
}

// IsTypeInterface reports whether v is an interface.
func IsTypeInterface(t reflect.Type) bool {
	if t == reflect.TypeOf(nil) {
		return false
	}
	return t.Kind() == reflect.Interface
}

// IsValScalar - true if built-in simple variable type
func IsValScalar(t reflect.Type) bool {
	switch t.Kind() {
	case reflect.Ptr, reflect.Interface:
		return IsValScalar(t.Elem())
	case reflect.Array, reflect.Slice, reflect.Map, reflect.Struct:
		return false
	default:
		return true
	}
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
