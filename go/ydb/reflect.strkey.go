package ydb

import (
	"fmt"
	"reflect"
	"strings"
)

// structuredkey (structured key) for value data expression
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
	var rPrev rune
	for offset, r := range s[base:] {
		if strings.IndexAny(delimiters, string(r)) >= 0 {
			// fmt.Println("lvl ", lvl, string(r))
			if r == ']' {
				if stack[lvl].delimiter == '[' && rPrev != '\\' {
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
		rPrev = r
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
				m[n] = strings.ReplaceAll(v, "\\]", "]")
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
		// remove \]
		ss = strings.ReplaceAll(ss, "\\]", "]")
		return name[offsets[0]:offsets[1]], ss, nil
	}
	return "", "", fmt.Errorf("no string value")
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
	case reflect.String:
		k, kfields, err := ExtractStrKeyNameAndValue(key)
		if err != nil {
			return reflect.Value{}, fmt.Errorf("StrKey extraction failed from %s", key)
		}
		for _, v := range kfields {
			return ValScalarNew(t, v)
		}
		return ValScalarNew(t, k)
	default:
		return ValScalarNew(t, key)
	}

}

// StrKeyGen - Generate the Structured key
// Multiple keyName must be separated by space.
func StrKeyGen(kv reflect.Value, structName, keyName string) (string, error) {
	if !kv.IsValid() {
		return "", fmt.Errorf("invalid Key value inserted for StrKey")
	}
	if structName == "" {
		return "", fmt.Errorf("no node name inserted for StrKey")
	}
	knamelist := strings.Split(keyName, " ")
	switch kv.Kind() {
	case reflect.Ptr, reflect.Interface:
		return StrKeyGen(kv.Elem(), structName, keyName)
	case reflect.Array, reflect.Complex64, reflect.Complex128, reflect.Chan:
		return "", fmt.Errorf("not supported type: %s", kv.Kind())
	case reflect.Slice, reflect.Map:
		return "", fmt.Errorf("not supported StrKey type: %s", kv.Kind())
	case reflect.Struct:
		strkey := structName
		for _, kname := range knamelist {
			_, cv, ok := valStructFieldFind(kv, kname)
			if !ok {
				return "", fmt.Errorf("not found key from key value")
			}
			strkey = strkey + fmt.Sprintf("[%s=%v]", keyName, cv.Interface())
		}
		return strkey, nil
	default:
		strkey := structName
		if len(knamelist) != 1 {
			return "", fmt.Errorf("not multiple key value")
		}
		strkey = strkey + fmt.Sprintf("[%s=%v]", keyName, kv.Interface())
		return strkey, nil
	}
}

var pathDelimiters string = "/[]'\""

// ToSliceKeys - Get the sliced key list from the path
func ToSliceKeys(path string) ([]string, error) {
	lvl := 0
	keylist := make([]string, 0, 8)
	stack := make([]delimOffset, 0, 8)
	stack = append(stack, delimOffset{delimiter: '/', offset: -1})
	var rPrev rune
	for offset, r := range path {
		if strings.IndexAny(pathDelimiters, string(r)) >= 0 {
			if r == '/' {
				if stack[lvl].delimiter == '/' {
					if offset > 0 {
						keylist = append(keylist, path[stack[lvl].offset+1:offset])
					}
					stack = append(stack, delimOffset{delimiter: '/', offset: offset})
					lvl++
				}
			} else if r == '[' {
				stack = append(stack, delimOffset{delimiter: r, offset: offset})
				lvl++
			} else if r == ']' {
				if stack[lvl].delimiter == '[' && rPrev != '\\' {
					stack = stack[:lvl]
					lvl--
				}
			} else if r == stack[lvl].delimiter { // ' or "
				stack = stack[:lvl]
				lvl--
			} else { // ' or "
				stack = append(stack, delimOffset{delimiter: r, offset: offset})
				lvl++
			}
		}
		rPrev = r
	}
	if stack[lvl].delimiter != '/' {
		fmt.Println(stack)
		return nil, fmt.Errorf("invalid path input")
	}
	if rPrev != '/' {
		keylist = append(keylist, path[stack[lvl].offset+1:])
	}
	return keylist, nil
}
