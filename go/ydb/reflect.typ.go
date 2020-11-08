package ydb

import (
	"reflect"
	"strings"
	"unicode"
)

// IsTypeInterface reports whether v is an interface.
func IsTypeInterface(t reflect.Type) bool {
	if t == reflect.TypeOf(nil) {
		return false
	}
	return t.Kind() == reflect.Interface
}

// IsTypeScalar - true if built-in simple variable type
func IsTypeScalar(t reflect.Type) bool {
	// if t == reflect.TypeOf(nil) {
	// 	return false
	// }
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

// IsTypeMap - true if the type is map
func IsTypeMap(t reflect.Type) bool {
	if t == reflect.TypeOf(nil) {
		return false
	}
	switch t.Kind() {
	case reflect.Ptr:
		return IsTypeMap(t.Elem())
	case reflect.Map:
		return true
	default:
		return false
	}
}

// // IsTypeSlice - true if the type is slice
// func IsTypeSlice(t reflect.Type) bool {
// 	switch t.Kind() {
// 	case reflect.Ptr:
// 		return IsTypeSlice(t.Elem())
// 	case reflect.Map:
// 		return true
// 	default:
// 		return false
// 	}
// }

// IsTypeStruct - true if the type is struct
func IsTypeStruct(t reflect.Type) bool {
	switch t.Kind() {
	case reflect.Ptr:
		return IsTypeStruct(t.Elem())
	case reflect.Struct:
		return true
	default:
		return false
	}
}

// FindFieldByName finds struct field by the name.
func FindFieldByName(structType reflect.Type, name string) (reflect.StructField, bool) {
	if CaseInsensitiveFieldLookup {
		for i := 0; i < structType.NumField(); i++ {
			ft := structType.Field(i)
			if strings.EqualFold(ft.Name, name) {
				return ft, true
			}
		}
	} else {
		ft, ok := structType.FieldByName(name)
		if ok {
			return ft, ok
		}
	}
	if EnableTagLookup {
		for i := 0; i < structType.NumField(); i++ {
			ft := structType.Field(i)
			if n, ok := ft.Tag.Lookup(TagLookupKey); ok && n == name {
				return ft, true
			}
		}
	}
	return reflect.StructField{}, false
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
		ft, ok := FindFieldByName(pt, key)
		if ok {
			return ft.Type, true
		}
	case reflect.Map, reflect.Slice, reflect.Array:
		return pt.Elem(), true
	}
	return pt, false
}

// IsStartedWithUpper - check the case of a string
func IsStartedWithUpper(s string) bool {
	for _, r := range s {
		if unicode.IsUpper(r) && unicode.IsLetter(r) {
			return true
		}
		return false
	}
	return false
}

// TypeGetAll - Get All child values from the struct, map or slice value
func TypeGetAll(t reflect.Type) ([]reflect.Type, bool) {
	nt := reflect.TypeOf(nil)
	if t == nt {
		return []reflect.Type{}, false
	}
	// return []reflect.Type{t}, true
	switch t.Kind() {
	case reflect.Interface:
		// Interface type doesn't have any dedicated type assigned!!.
		return []reflect.Type{}, false
	case reflect.Ptr:
		return TypeGetAll(t.Elem())
	case reflect.Struct:
		length := t.NumField()
		rtype := make([]reflect.Type, 0)
		for i := 0; i < length; i++ {
			sft := t.Field(i)
			st := sft.Type
			if st != nt && IsStartedWithUpper(sft.Name) {
				rtype = append(rtype, st)
			}
		}
		return rtype, true
	case reflect.Map, reflect.Slice:
		return []reflect.Type{t.Elem()}, true
	default:
		return []reflect.Type{}, false
	}
}
