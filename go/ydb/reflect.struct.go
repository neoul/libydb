package ydb

import (
	"fmt"
	"reflect"
)

// ValFindStructField - Find a struct field by the name (struct field name or Tag) from sv (struct)
func ValFindStructField(sv reflect.Value, name interface{}) (reflect.StructField, reflect.Value, bool) {
	if !sv.IsValid() {
		return reflect.StructField{}, reflect.Value{}, false
	}
	st := sv.Type()
	fnv, err := ValNewScalar(reflect.TypeOf(""), name)
	if !fnv.IsValid() || err != nil {
		return reflect.StructField{}, reflect.Value{}, false
	}
	sname := fnv.Interface().(string)
	ft, ok := st.FieldByName(sname)
	if ok {
		fv := sv.FieldByName(sname)
		return ft, fv, true
	}
	if EnableTagLookup {
		for i := 0; i < sv.NumField(); i++ {
			fv := sv.Field(i)
			ft := st.Field(i)
			if !fv.IsValid() || !fv.CanSet() {
				continue
			}
			if n, ok := ft.Tag.Lookup(TagLookupKey); ok && n == sname {
				return ft, fv, true
			}
		}
	}
	return reflect.StructField{}, reflect.Value{}, false
}

// ValSetStructField - Set the field of the struct.
func ValSetStructField(sv reflect.Value, name interface{}, val interface{}) error {
	if !sv.IsValid() || !sv.CanSet() {
		return fmt.Errorf("invalid or not settable")
	}
	ft, fv, ok := findStructField(sv, name)
	if !ok {
		return fmt.Errorf("%s not found in %s", name, sv.Type())
	}
	if IsValScalar(ft.Type) {
		// nv := NewValue(ft.Type, val)
		// fv.Set(nv)
		return ValSetScalar(fv, val)
	}
	nv, err := ValNew(ft.Type)
	if err != nil {
		return err
	}
	fv.Set(nv)
	return nil
}

// ValNewStruct - Create new struct value and fill out all struct field.
func ValNewStruct(t reflect.Type) (reflect.Value, error) {
	if t == reflect.TypeOf(nil) {
		return reflect.Value{}, fmt.Errorf("nil type")
	}
	if t.Kind() != reflect.Struct {
		return reflect.Value{}, fmt.Errorf("not slice")
	}
	pv := reflect.New(t)
	pve := pv.Elem()
	for i := 0; i < pve.NumField(); i++ {
		fv := pve.Field(i)
		ft := pve.Type().Field(i)
		if !fv.IsValid() || !fv.CanSet() {
			continue
		}
		switch ft.Type.Kind() {
		case reflect.Array, reflect.Complex64, reflect.Complex128, reflect.Chan:
			return reflect.Value{}, fmt.Errorf("not supported type: %s", t.Kind())
		case reflect.Map:
			fv.Set(reflect.MakeMap(ft.Type))
		case reflect.Slice:
			fv.Set(reflect.MakeSlice(ft.Type, 0, 0))
		case reflect.Struct:
			srv, err := ValNewStruct(ft.Type)
			if err != nil {
				return srv, fmt.Errorf("%s not created", ft.Name)
			}
			fv.Set(srv)
		case reflect.Ptr:
			srv, err := ValNew(ft.Type)
			if err != nil {
				return srv, fmt.Errorf("%s not created (%v)", ft.Name, err)
			}
			fv.Set(srv)
		// case reflect.Chan:
		// 	fv.Set(reflect.MakeChan(ft.Type, 0))
		// case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64:
		// 	fv.SetInt(0)
		// case reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64, reflect.Uintptr:
		// 	fv.SetUint(0)
		// case reflect.Float32, reflect.Float64:
		// 	fv.SetFloat(0)
		// case reflect.Bool:
		// 	fv.SetBool(false)
		// case reflect.String:
		// 	fv.SetString("")
		default:
		}
	}
	return pve, nil
}
