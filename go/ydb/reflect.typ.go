package ydb

import "reflect"

// TypeFind - finds a child type from the struct, map or slice value using the key.
func TypeFind(pt reflect.Type, key string) (reflect.Type, bool) {
	if pt == reflect.TypeOf(nil) {
		return pt, false
	}
	if key == "" {
		return pt, true
	}

	switch pt.Kind() {
	case reflect.Ptr, reflect.Interface:
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
