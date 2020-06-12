package ydb

import (
	"fmt"
	"reflect"
)

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
			err := ValStructFieldSet(v, key, nil)
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

// IsValNilOrDefault returns true if either isValueNil(value) or the default
// value for the type.
func IsValNilOrDefault(value interface{}) bool {
	if isValueNil(value) {
		return true
	}
	return value == reflect.New(reflect.TypeOf(value)).Elem().Interface()
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
