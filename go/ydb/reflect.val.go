package ydb

import (
	"fmt"
	"reflect"
)

// ValFindValue - finds a child value from the struct, map or slice value using the string key.
func ValFindValue(v reflect.Value, key string) (reflect.Value, bool) {
	if !v.IsValid() {
		return reflect.Value{}, false
	}
	cur := v
	if key == "" {
		return cur, true
	}
	switch cur.Kind() {
	case reflect.Ptr, reflect.Interface:
		return ValFindValue(cur.Elem(), key)
	case reflect.Struct:
		_, sfv, ok := findStructField(cur, key)
		if !ok {
			return reflect.Value{}, false
		}
		cur = sfv
	case reflect.Map:
		mt := cur.Type()
		kv, err := ValNewScalar(mt.Key(), key)
		if !kv.IsValid() || err != nil {
			return reflect.Value{}, false
		}
		vv := cur.MapIndex(kv)
		if !vv.IsValid() {
			return reflect.Value{}, false
		}
		cur = vv
	case reflect.Slice, reflect.Array:
		idxv, err := ValNewScalar(reflect.TypeOf(0), key)
		if !idxv.IsValid() || err != nil {
			return reflect.Value{}, false
		}
		index := idxv.Interface().(int)
		if cur.Len() <= index {
			return reflect.Value{}, false
		}
		vv := cur.Index(index)
		if !vv.IsValid() {
			return reflect.Value{}, false
		}
		cur = vv
	default:
		return reflect.Value{}, false
	}
	return cur, true
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

// ValNewScalar - Create a new value of the t type.
func ValNewScalar(t reflect.Type, val interface{}) (reflect.Value, error) {
	if t == reflect.TypeOf(nil) {
		return reflect.Value{}, fmt.Errorf("nil type")
	}
	switch t.Kind() {
	case reflect.Ptr:
		cv, err := ValNewScalar(t.Elem(), val)
		return newPtrOfValue(cv), err
	case reflect.Interface:
		return ValNewScalar(t.Elem(), val)
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

// ValSetScalar - Create a new value to the pointed value, v.
func ValSetScalar(v reflect.Value, val interface{}) error {
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
			nv, err := ValNewScalar(v.Type(), val)
			if err != nil {
				return err
			}
			v.Set(nv) // sets v to new value, nv.
			return nil
		}
		return ValSetScalar(v.Elem(), val)
	case reflect.Interface:
		return ValSetScalar(v.Elem(), val)
	case reflect.Array, reflect.Complex64, reflect.Complex128, reflect.Chan:
		return fmt.Errorf("not supported type: %s", t.Kind())
	case reflect.Struct, reflect.Map, reflect.Slice:
		return fmt.Errorf("non-scalar type: %s", t.Kind())
	default:
		if val == nil {
			val = ""
		}
		if !v.CanSet() {
			return fmt.Errorf("not settable value")
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
		return ValNewStruct(t)
	case reflect.Map:
		return reflect.MakeMap(t), nil
	case reflect.Slice:
		return reflect.MakeSlice(t, 0, 0), nil
	default:
		return ValNewScalar(t, nil)
	}
}
