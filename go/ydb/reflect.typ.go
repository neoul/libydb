package ydb

import "reflect"

// IsTypeInterface reports whether v is an interface.
func IsTypeInterface(t reflect.Type) bool {
	if t == reflect.TypeOf(nil) {
		return false
	}
	return t.Kind() == reflect.Interface
}

// IsTypeScalar - true if built-in simple variable type
func IsTypeScalar(t reflect.Type) bool {
	switch t.Kind() {
	case reflect.Ptr:
		return IsTypeScalar(t.Elem())
	case reflect.Interface:
		return false
	case reflect.Array, reflect.Slice, reflect.Map, reflect.Struct:
		return false
	default:
		return true
	}
}

// TypeFind - finds a child type from the struct, map or slice value using the key.
func TypeFind(pt reflect.Type, key string) (reflect.Type, bool) {
	if pt == reflect.TypeOf(nil) {
		return pt, false
	}
	if key == "" {
		return pt, true
	}

	switch pt.Kind() {
	case reflect.Interface:
		return pt, false
	case reflect.Ptr:
		return TypeFind(pt.Elem(), key)
	case reflect.Struct:
		ft, ok := pt.FieldByName(key)
		if ok {
			return ft.Type, true
		}
		if EnableTagLookup {
			for i := 0; i < pt.NumField(); i++ {
				ft := pt.Field(i)
				if n, ok := ft.Tag.Lookup(TagLookupKey); ok && n == key {
					return ft.Type, true
				}
			}
		}
	case reflect.Map, reflect.Slice, reflect.Array:
		return pt.Elem(), true
	}
	return pt, false
}
