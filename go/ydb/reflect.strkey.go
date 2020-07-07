package ydb

import (
	"fmt"
	"reflect"
	"strings"
)

// structuredkey (structured key)
// Format: StructName[FieldName=Value][FieldName=Value]... for a structure and structure field.

var delimiters string = "[]'\""

type delimOffset struct {
	delimiter rune
	offset    int
}

func delimBracket(s string) ([]int, error) {
	lvl := 0
	slen := len(s)
	stack := append([]delimOffset{}, delimOffset{delimiter: 0, offset: 0})
	base := strings.IndexAny(s, delimiters)
	if base < 0 {
		return []int{0, slen}, nil
	}
	result := []int{0, base}
	for offset, r := range s[base:] {
		if strings.IndexAny(delimiters, string(r)) >= 0 {
			// fmt.Println("lvl ", lvl, string(r))
			if r == ']' {
				if stack[lvl].delimiter == '[' {
					if lvl == 1 { // Square brackets are valid on 1 level.
						result = append(result,
							stack[lvl].offset+base, offset+base)
					}
					stack = stack[:lvl]
					lvl--
				} else if stack[lvl].delimiter == 0 {
					return []int{}, fmt.Errorf("invalid value format")
				}
			} else if stack[lvl].delimiter == r && (r == '"' || r == '\'') {
				stack = stack[:lvl]
				lvl--
			} else {
				stack = append(stack, delimOffset{delimiter: r, offset: offset + 1})
				lvl++
			}
		}
	}
	if len(stack) > 1 {
		return []int{}, fmt.Errorf("invalid value format")
	}
	return result, nil
}

// ExtractStrKeyNameAndValue - Extract the struct name, field values
func ExtractStrKeyNameAndValue(s interface{}) (string, map[string]string, error) {
	if s == nil {
		return "", nil, fmt.Errorf("nil value")
	}
	name, ok := s.(string)
	if ok {
		offsets, err := delimBracket(name)
		if err != nil {
			return name, nil, err
		}
		offsetnum := len(offsets)
		if offsetnum == 2 {
			return name[offsets[0]:offsets[1]], nil, nil
		}
		m := map[string]string{}
		for i := 2; i < offsetnum; i = i + 2 {
			fieldNameVal := name[offsets[i]:offsets[i+1]]
			kvdelim := strings.Index(fieldNameVal, "=") // key=val delimiter
			if kvdelim >= 0 {
				n := fieldNameVal[:kvdelim]
				v := strings.Trim(fieldNameVal[kvdelim+1:], "'\"")
				m[n] = v
			} else {
				return "", nil, fmt.Errorf("invalid field name & value")
			}
		}
		return name[offsets[0]:offsets[1]], m, nil
	}
	return "", nil, fmt.Errorf("no string value")
}

// ExtractStrKeyNameAndSubstring - Extract the struct name, field values
func ExtractStrKeyNameAndSubstring(s interface{}) (string, string, error) {
	if s == nil {
		return "", "", fmt.Errorf("nil value")
	}
	name, ok := s.(string)
	if ok {
		offsets, err := delimBracket(name)
		if err != nil {
			return name, "", err
		}
		offsetnum := len(offsets)
		if offsetnum == 2 {
			return name[offsets[0]:offsets[1]], "", nil
		}

		ss := name[offsets[2]-1 : offsets[offsetnum-1]+1]
		return name[offsets[0]:offsets[1]], ss, nil
	}
	return "", "", fmt.Errorf("no string value")
}

// StrKeyStructFieldFind - Find the target structuredkey value from the struct value in depth.
func StrKeyStructFieldFind(sv reflect.Value, structuredkey interface{}, searchtype SearchType) (reflect.Value, bool) {
	if !sv.IsValid() {
		return reflect.Value{}, false
	}
	fieldname, remains, err := ExtractStrKeyNameAndSubstring(structuredkey)
	if err != nil {
		return reflect.Value{}, false
	}
	_, cv, ok := valStructFieldFind(sv, fieldname)
	if !ok {
		return reflect.Value{}, ok
	}
	if remains != "" {
		return ValFind(cv, remains, searchtype)
	}
	return cv, ok
}

// StrKeyStructFieldSet - Set the field of the struct.
func StrKeyStructFieldSet(sv reflect.Value, structuredkey interface{}, val interface{}, insertType SearchType) error {
	if !sv.IsValid() {
		return fmt.Errorf("invalid struct")
	}
	fieldname, remainedkey, err := ExtractStrKeyNameAndSubstring(structuredkey)
	if err != nil {
		return err
	}
	ft, fv, ok := valStructFieldFind(sv, fieldname)
	if !ok {
		return fmt.Errorf("%s not found in %s", structuredkey, sv.Type())
	}
	if !fv.CanSet() {
		return fmt.Errorf("not settable field %s", structuredkey)
	}
	ftt := ft.Type
	if IsTypeInterface(ftt) { // That means it is not a specified type.
		ftt = reflect.TypeOf(val)
		if IsTypeScalar(ftt) {
			return ValScalarSet(fv, val)
		}
		nv := reflect.ValueOf(val)
		fv.Set(nv)
		return nil
	}
	if IsNilOrInvalidValue(fv) {
		nv, err := ValNew(ftt, nil)
		if err != nil {
			return err
		}
		fv.Set(nv)
	}
	if remainedkey != "" {
		_, err := ValChildSet(fv, remainedkey, val, insertType)
		if err != nil {
			return err
		}
		return nil
	}
	if IsTypeScalar(ftt) {
		return ValScalarSet(fv, val)
	}
	return nil
}

// StrKeyStructNew - Create new struct value and fill out all struct field.
// val - must be a string formed as "StructName[StructField1:VAL1][StructField2:VAL2]"
func StrKeyStructNew(t reflect.Type, val interface{}) (reflect.Value, error) {
	if t == reflect.TypeOf(nil) {
		return reflect.Value{}, fmt.Errorf("nil type")
	}
	if t.Kind() != reflect.Struct {
		return reflect.Value{}, fmt.Errorf("not struct")
	}
	_, fields, _ := ExtractStrKeyNameAndValue(val)
	fmt.Println("StrKeyStructNew val", t, val)

	pv := reflect.New(t)
	pve := pv.Elem()
	if InitChildenOnSet {
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
				srv, err := ValStructNew(ft.Type, InitChildenOnSet)
				if err != nil {
					return srv, fmt.Errorf("%s not created", ft.Name)
				}
				fv.Set(srv)
			case reflect.Ptr:
				srv, err := ValNew(ft.Type, val)
				if err != nil {
					return srv, fmt.Errorf("%s not created (%v)", ft.Name, err)
				}
				fv.Set(srv)
			default:
			}
		}
	}
	if fields != nil {
		for fieldname, fieldval := range fields {
			ft, fv, ok := valStructFieldFind(pve, fieldname)
			if !ok {
				return reflect.Value{}, fmt.Errorf("%s not found in %s", fieldname, pve.Type())
			}
			if !fv.CanSet() {
				return reflect.Value{}, fmt.Errorf("not settable field %s", fieldname)
			}
			rv, err := StrKeyValNew(ft.Type, fieldval)
			if err != nil {
				return reflect.Value{}, err
			}
			fv.Set(rv)
		}
	}
	return pve, nil
}

// StrKeyValNew - Create a new structured key of the t type.
func StrKeyValNew(t reflect.Type, key interface{}) (reflect.Value, error) {
	if t == reflect.TypeOf(nil) {
		return reflect.Value{}, fmt.Errorf("nil type")
	}
	switch t.Kind() {
	case reflect.Ptr:
		cv, err := StrKeyValNew(t.Elem(), key)
		return newPtrOfValue(cv), err
	case reflect.Interface:
		return StrKeyValNew(t.Elem(), key)
	case reflect.Array, reflect.Complex64, reflect.Complex128, reflect.Chan:
		return reflect.Value{}, fmt.Errorf("not supported type: %s", t.Kind())
	case reflect.Slice, reflect.Map:
		return reflect.Value{}, fmt.Errorf("not supported StrKey type: %s", t.Kind())
	case reflect.Struct:
		return StrKeyStructNew(t, key)
	default:
		k, kfields, err := ExtractStrKeyNameAndValue(key)
		if err != nil {
			return reflect.Value{}, fmt.Errorf("StrKey extraction failed from %s", key)
		}
		for _, v := range kfields {
			return ValScalarNew(t, v)
		}
		return ValScalarNew(t, k)
	}
}
