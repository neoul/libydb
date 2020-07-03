package ydb

import (
	"fmt"
	"reflect"
	"strings"
)

// DisassembleStructString - Disassemble the Structure name
// from from key or name value to a Struct and its StructField names if capable.
// Format: StructName[StructField=Value][StructField=Value]
func DisassembleStructString(StructName interface{}) (interface{}, map[string]string, bool) {
	if StructName == nil {
		return StructName, nil, false
	}
	name, ok := StructName.(string)
	if ok {
		delimStart := strings.Index(name, "[")
		if delimStart >= 0 {
			keyElem := name[:delimStart]
			// if len(keyElem) <= 0 {
			// 	return StructName, nil, false
			// }
			m := map[string]string{}
			name = name[delimStart:]
			delimStart = 0
			for delimStart >= 0 {
				delimEnd := strings.Index(name, "]")
				if delimEnd < 0 {
					break
				}
				kvpair := strings.Trim(name[delimStart:delimEnd], "[]")
				kvdelim := strings.Index(kvpair, "=") // key=val delimiter
				if kvdelim >= 0 {
					n := kvpair[:kvdelim]
					v := kvpair[kvdelim+1:]
					m[n] = v
				} else {
					break
				}
				name = name[delimEnd+1:]
				delimStart = strings.Index(name, "[")
			}
			// fmt.Println(keyElem, m)
			return keyElem, m, true
		}
		if len(name) > 0 {
			return StructName, nil, false
		}
		return StructName, nil, false
	}
	// A StructName without StructFields
	return StructName, nil, false
}

// CheckAndExtractStructName - Checks and extracts the Structure name
// from key or name value (StructName[StructField=Value][StructField=Value])
func CheckAndExtractStructName(key interface{}) (interface{}, bool) {
	if key == nil {
		return key, false
	}
	name, ok := key.(string)
	if ok {
		delimStart := strings.Index(name, "[")
		if delimStart >= 0 {
			keyElem := name[:delimStart]
			if len(keyElem) <= 0 {
				return "", false
			}
			return keyElem, true
		}
		if len(name) > 0 {
			return name, false
		}
		return key, false
	}
	return key, false
}

// valStructFieldFind - Find a struct field by the name (struct field name or Tag) from sv (struct)
func valStructFieldFind(sv reflect.Value, name string) (reflect.StructField, reflect.Value, bool) {
	st := sv.Type()
	ft, ok := st.FieldByName(name)
	if ok {
		fv := sv.FieldByName(name)
		return ft, fv, true
	}
	if EnableTagLookup {
		for i := 0; i < sv.NumField(); i++ {
			fv := sv.Field(i)
			ft := st.Field(i)
			if !fv.IsValid() || !fv.CanSet() {
				continue
			}
			if n, ok := ft.Tag.Lookup(TagLookupKey); ok && n == name {
				return ft, fv, true
			}
		}
	}
	return reflect.StructField{}, reflect.Value{}, false
}

// ValStructFieldFind - Find a struct field by the name (struct field name or Tag) from sv (struct)
func ValStructFieldFind(sv reflect.Value, name interface{}) (reflect.StructField, reflect.Value, bool) {
	if !sv.IsValid() {
		return reflect.StructField{}, reflect.Value{}, false
	}
	fieldname, _ := CheckAndExtractStructName(name)
	return valStructFieldFind(sv, fieldname.(string))
}

// ValStructFieldSet - Set the field of the struct.
func ValStructFieldSet(sv reflect.Value, name interface{}, val interface{}) error {
	if !sv.IsValid() {
		return fmt.Errorf("invalid struct")
	}
	ft, fv, ok := ValStructFieldFind(sv, name)
	if !ok {
		return fmt.Errorf("%s not found in %s", name, sv.Type())
	}
	if !fv.CanSet() {
		return fmt.Errorf("not settable field %s", name)
	}
	ftt := ft.Type
	if IsTypeInterface(ftt) { // That means it is not a specified type.
		if val == nil {
			return fmt.Errorf("not specified field or val type for %s", name)
		}
		ftt = reflect.TypeOf(val)
		if IsTypeScalar(ftt) {
			return ValScalarSet(fv, val)
		}
		nv := reflect.ValueOf(val)
		fv.Set(nv)
		return nil
	}
	if IsTypeScalar(ftt) {
		return ValScalarSet(fv, val)
	}
	nv, err := ValNew(ftt)
	if err != nil {
		return err
	}
	fv.Set(nv)
	return nil
}

// ValStructFieldUnset - Remove the field of the struct.
func ValStructFieldUnset(sv reflect.Value, name interface{}) error {
	if !sv.IsValid() {
		return fmt.Errorf("invalid struct")
	}
	ft, fv, ok := ValStructFieldFind(sv, name)
	if !ok {
		return fmt.Errorf("%s not found in %s", name, sv.Type())
	}
	if IsTypeScalar(ft.Type) {
		return ValScalarSet(fv, "")
	}
	nv, err := ValNew(ft.Type)
	if err != nil {
		return err
	}
	fv.Set(nv)
	return nil
}

// ValStructNew - Create new struct value and fill out all struct field.
// val - must be a string formed as "StructName[StructField1:VAL1][StructField2:VAL2]"
func ValStructNew(t reflect.Type, val interface{}, initAllfields bool) (reflect.Value, error) {
	if t == reflect.TypeOf(nil) {
		return reflect.Value{}, fmt.Errorf("nil type")
	}
	if t.Kind() != reflect.Struct {
		return reflect.Value{}, fmt.Errorf("not struct")
	}
	_, fields, _ := DisassembleStructString(val)

	pv := reflect.New(t)
	pve := pv.Elem()
	if initAllfields {
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
				srv, err := ValStructNew(ft.Type, nil, initAllfields)
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
	}
	if fields != nil {
		for fieldname, fieldval := range fields {
			err := ValStructFieldSet(pve, fieldname, fieldval)
			if err != nil {
				return reflect.Value{}, err
			}
		}
	}
	return pve, nil
}
