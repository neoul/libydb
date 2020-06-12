package ydb

import (
	"bufio"
	"fmt"
	"io"
	"reflect"
	"strconv"
	"strings"
)

// EnableTagLookup - Enable the tag lookup of struct fields for searching value
var EnableTagLookup bool = true

// TagLookupKey - the key of the struct field tag to search a value.
var TagLookupKey string = "path"

func typeString(t reflect.Type) string {
	return fmt.Sprintf("%s", t)
}

// getBaseType returns not reflect.Ptr type.
func getBaseType(t reflect.Type) reflect.Type {
	for ; t.Kind() == reflect.Ptr; t = t.Elem() {
	}
	return t
}

// isTypeDeep reports whether t is k type.
func isTypeDeep(t reflect.Type, kinds ...reflect.Kind) bool {
	for ; t.Kind() == reflect.Ptr; t = t.Elem() {
	}
	for _, k := range kinds {
		if t.Kind() == k {
			return true
		}
	}
	return false
}

// isReferenceType returns true if t is a map, slice or channel
func isReferenceType(t reflect.Type) bool {
	switch t.Kind() {
	case reflect.Slice, reflect.Chan, reflect.Map:
		return true
	}
	return false
}

// isTypeStruct reports whether t is a struct type.
func isTypeStruct(t reflect.Type) bool {
	return t.Kind() == reflect.Struct
}

// isTypeStructPtr reports whether v is a struct ptr type.
func isTypeStructPtr(t reflect.Type) bool {
	if t == reflect.TypeOf(nil) {
		return false
	}
	return t.Kind() == reflect.Ptr && t.Elem().Kind() == reflect.Struct
}

// isTypeSlice reports whether v is a slice type.
func isTypeSlice(t reflect.Type) bool {
	return t.Kind() == reflect.Slice
}

// isTypeSlicePtr reports whether v is a slice ptr type.
func isTypeSlicePtr(t reflect.Type) bool {
	if t == reflect.TypeOf(nil) {
		return false
	}
	return t.Kind() == reflect.Ptr && t.Elem().Kind() == reflect.Slice
}

// isTypeMap reports whether v is a map type.
func isTypeMap(t reflect.Type) bool {
	if t == reflect.TypeOf(nil) {
		return false
	}
	return t.Kind() == reflect.Map
}

// isTypeInterface reports whether v is an interface.
func isTypeInterface(t reflect.Type) bool {
	if t == reflect.TypeOf(nil) {
		return false
	}
	return t.Kind() == reflect.Interface
}

// isTypeSliceOfInterface reports whether v is a slice of interface.
func isTypeSliceOfInterface(t reflect.Type) bool {
	if t == reflect.TypeOf(nil) {
		return false
	}
	return t.Kind() == reflect.Slice && t.Elem().Kind() == reflect.Interface
}

// isTypeChan reports whether v is a slice type.
func isTypeChan(t reflect.Type) bool {
	return t.Kind() == reflect.Chan
}

// areSameType returns true if t1 and t2 has the same reflect.Type,
// otherwise it returns false.
func areSameType(t1 reflect.Type, t2 reflect.Type) bool {
	b1 := getBaseType(t1)
	b2 := getBaseType(t2)
	return b1 == b2
}

// IsNilOrInvalidValue reports whether v is nil or reflect.Zero.
func IsNilOrInvalidValue(v reflect.Value) bool {
	return !v.IsValid() || (v.Kind() == reflect.Ptr && v.IsNil()) || IsValueNil(v.Interface())
}

// IsValueNil returns true if either value is nil, or has dynamic type {ptr,
// map, slice} with value nil.
func IsValueNil(value interface{}) bool {
	if value == nil {
		return true
	}
	switch reflect.TypeOf(value).Kind() {
	case reflect.Slice, reflect.Ptr, reflect.Map:
		return reflect.ValueOf(value).IsNil()
	}
	return false
}

// IsValueNilOrDefault returns true if either IsValueNil(value) or the default
// value for the type.
func IsValueNilOrDefault(value interface{}) bool {
	if IsValueNil(value) {
		return true
	}
	if !isValueScalar(reflect.ValueOf(value)) {
		// Default value is nil for non-scalar types.
		return false
	}
	return value == reflect.New(reflect.TypeOf(value)).Elem().Interface()
}

// isValuePtr reports whether v is a ptr type.
func isValuePtr(v reflect.Value) bool {
	return v.Kind() == reflect.Ptr
}

// IsValueInterface reports whether v is an interface type.
func IsValueInterface(v reflect.Value) bool {
	return v.Kind() == reflect.Interface
}

// isValueStruct reports whether v is a struct type.
func isValueStruct(v reflect.Value) bool {
	return v.Kind() == reflect.Struct
}

// isValueStructPtr reports whether v is a struct ptr type.
func isValueStructPtr(v reflect.Value) bool {
	return v.Kind() == reflect.Ptr && isValueStruct(v.Elem())
}

// isValueMap reports whether v is a map type.
func isValueMap(v reflect.Value) bool {
	return v.Kind() == reflect.Map
}

// IsValueSlice reports whether v is a slice type.
func IsValueSlice(v reflect.Value) bool {
	return v.Kind() == reflect.Slice
}

// isValueScalar reports whether v is a scalar type.
func isValueScalar(v reflect.Value) bool {
	if IsNilOrInvalidValue(v) {
		return false
	}
	if isValuePtr(v) {
		if v.IsNil() {
			return false
		}
		return isValueScalar(v.Elem())
	}
	return !isValueStruct(v) && !isValueMap(v) && !IsValueSlice(v)
}

// isValueInterfaceToStructPtr reports whether v is an interface that contains a
// pointer to a struct.
func isValueInterfaceToStructPtr(v reflect.Value) bool {
	return IsValueInterface(v) && isValueStructPtr(v.Elem())
}

// isStructValueWithNFields returns true if the reflect.Value representing a
// struct v has n fields.
func isStructValueWithNFields(v reflect.Value, n int) bool {
	return isValueStruct(v) && v.NumField() == n
}

// isSimpleType - true if built-in simple variable type
func isSimpleType(t reflect.Type) bool {
	switch t.Kind() {
	case reflect.Ptr, reflect.Interface:
		return isSimpleType(t.Elem())
	case reflect.Array, reflect.Slice, reflect.Map, reflect.Struct:
		return false
	default:
		return true
	}
}

// isValidDeep reports whether v is valid.
func isValidDeep(v reflect.Value) bool {
	for ; v.Kind() == reflect.Ptr; v = v.Elem() {
		if !v.IsValid() {
			return false
		}
	}
	if !v.IsValid() {
		return false
	}
	return true
}

// isNilDeep reports whether v is nil
func isNilDeep(v reflect.Value) bool {
	for ; v.Kind() == reflect.Ptr; v = v.Elem() {
		if v.IsNil() {
			return true
		}
	}
	return false
}

// IsEmptyInterface reports whether x is empty interface
func IsEmptyInterface(x interface{}) bool {
	if x == nil {
		return true
	}
	return x == reflect.Zero(reflect.TypeOf(x)).Interface()
}

func isZero(v reflect.Value) bool {
	switch v.Kind() {
	case reflect.Func, reflect.Map, reflect.Slice:
		return v.IsNil()
	case reflect.Array:
		z := true
		for i := 0; i < v.Len(); i++ {
			z = z && isZero(v.Index(i))
		}
		return z
	case reflect.Struct:
		z := true
		for i := 0; i < v.NumField(); i++ {
			if v.Field(i).CanSet() {
				z = z && isZero(v.Field(i))
			}
		}
		return z
	case reflect.Ptr:
		return isZero(reflect.Indirect(v))
	}
	// Compare other types directly:
	z := reflect.Zero(v.Type())
	result := v.Interface() == z.Interface()

	return result
}

// GetNonInterfaceValueDeep - Find the non-interface reflect.value
func GetNonInterfaceValueDeep(v reflect.Value) reflect.Value {
	for ; v.Kind() == reflect.Interface; v = v.Elem() {
	}
	return v
}

// GetNonIfOrPtrValueDeep - Find the non-interface and non-ptr reflect.value
func GetNonIfOrPtrValueDeep(v reflect.Value) reflect.Value {
	for ; v.Kind() == reflect.Interface || v.Kind() == reflect.Ptr; v = v.Elem() {
	}
	return v
}

// DebugValueString returns a string representation of value which may be a value, ptr,
// or struct type.
// - value: The value to print.
// - depth: The depth of the printed values and types.
// - print: The print function
func DebugValueString(value interface{}, depth int, print func(a ...interface{})) string {
	v := reflect.ValueOf(value)
	s := debugValueStr(v, depth, 0, "", false, false)
	if print != nil {
		reader := bufio.NewReader(strings.NewReader(s))
		for {
			line, err := reader.ReadString('\n')
			if err == io.EOF {
				print(line)
				break
			}
			if err != nil {
				ss := fmt.Sprintf("ValueString error: %s", err)
				print(ss)
				break
			}
			print(line)
		}
		// print("\n")
		return ""
	}
	return s
}

// DebugValueStringInline returns a string representation of value which may be a value, ptr,
// or struct type.
// - value: The value to print.
// - depth: The depth of the printed values and types.
// - print: The print function
func DebugValueStringInline(value interface{}, depth int, print func(a ...interface{})) string {
	v := reflect.ValueOf(value)
	s := debugValueStr(v, depth, 0, "", false, true)
	s = strings.ReplaceAll(s, "\n", " ")
	if print != nil {
		print(s)
		return ""
	}
	return s
}

func debugValueStr(v reflect.Value, depth, ptrcnt int, indent string, disableIndent bool, noIndent bool) string {
	var out string
	if depth < 0 {
		return " ..."
	}
	if v.Type() == reflect.TypeOf(nil) {
		if disableIndent || noIndent {
			return "nil(nil)"
		}
		return indent + "nil(nil)"
	}
	if v.IsZero() {
		if disableIndent || noIndent {
			return fmt.Sprintf("%s(zero)", v.Type())
		}
		return indent + fmt.Sprintf("%s(zero)", v.Type())
	}
	if !v.IsValid() {
		if disableIndent || noIndent {
			return fmt.Sprintf("%s(invalid)", v.Type())
		}
		return indent + fmt.Sprintf("%s(invalid)", v.Type())
	}
	if v.Kind() == reflect.Ptr && v.IsNil() || IsValueNil(v.Interface()) {
		if disableIndent || noIndent {
			return fmt.Sprintf("%s(nil)", v.Type())
		}
		return indent + fmt.Sprintf("%s(nil)", v.Type())
	}

	switch v.Kind() {
	case reflect.Ptr:
		ptrcnt++
		out = "*" + debugValueStr(v.Elem(), depth, ptrcnt, indent, true, noIndent)
	case reflect.Interface:
		ptrcnt++
		out = "٭" + debugValueStr(v.Elem(), depth, ptrcnt, indent, true, noIndent)
	case reflect.Slice:
		out = fmt.Sprintf("%s(", v.Type())
		for i := 0; i < v.Len(); i++ {
			if !noIndent && depth > 0 {
				out += "\n"
			}
			out += debugValueStr(v.Index(i), depth-1, 0, indent+"• ", false, noIndent)
		}
		out += ")"
	case reflect.Struct:
		t := v.Type()
		out = fmt.Sprintf("%s(", v.Type())
		for i := 0; i < v.NumField(); i++ {
			fv := v.Field(i)
			ft := t.Field(i)
			// log.Debug(ft.Name, v.Type(), ft.Type)
			if areSameType(ft.Type, t) {
				continue
			}
			if noIndent {
				if fv.CanInterface() {
					out += fmt.Sprintf("\n%s:%v", ft.Name, debugValueStr(fv, depth-1, 0, indent+"• ", true, noIndent))
				} else {
					out += fmt.Sprintf("\n%s:%v", ft.Name, fv)
				}
			} else {
				if depth > 0 {
					out += fmt.Sprintf("\n%s", indent+"• ")
				} else {
					out += " "
				}
				if fv.CanInterface() {
					out += fmt.Sprintf("%s:%v", ft.Name, debugValueStr(fv, depth-1, 0, indent+"• ", true, noIndent))
				} else {
					out += fmt.Sprintf("%s:%v", ft.Name, fv)
				}
			}
		}
		out += ")"
	case reflect.Map:
		out = fmt.Sprintf("%s(", v.Type())
		iter := v.MapRange()
		for iter.Next() {
			k := iter.Key()
			e := iter.Value()
			if noIndent {
				out += fmt.Sprintf("\n%v:%s", k, debugValueStr(e, depth-1, 0, indent+"• ", true, noIndent))
			} else {
				if depth > 0 {
					out += fmt.Sprintf("\n%s", indent+"• ")
				} else {
					out += " "
				}
				out += fmt.Sprintf("%v:%s", k, debugValueStr(e, depth-1, 0, indent+"• ", true, noIndent))
			}
		}
		out += ")"
	default:
		out = fmt.Sprintf("%s(", v.Type())
		for i := 0; i < ptrcnt; i++ {
			out = out + "&"
		}
		out = out + fmt.Sprintf("%v)", v)
	}
	if disableIndent || noIndent {
		return out
	}
	return indent + out
}

func setMapValue(mv reflect.Value, key interface{}, element interface{}) error {
	if mv.Kind() == reflect.Ptr {
		if mv.IsNil() {
			cv := newValueMap(mv.Type())
			mv.Set(cv)
		}
		return setMapValue(mv.Elem(), key, element)
	}
	mt := mv.Type()
	kt := mt.Key()
	kv := newValue(kt, key)
	var vv reflect.Value
	if isTypeInterface(mt.Elem()) {
		vv = newValue(reflect.TypeOf(element), element)
	} else {
		vv = newValue(mt.Elem(), element)
	}
	if !kv.IsValid() {
		return fmt.Errorf("invalid key: %s", key)
	}
	if !vv.IsValid() {
		return fmt.Errorf("invalid element: %s", element)
	}
	mv.SetMapIndex(kv, vv)
	return nil
}

func setSliceValue(sv reflect.Value, element interface{}) reflect.Value {
	st := sv.Type()
	if st.Kind() == reflect.Ptr {
		if st.Elem().Kind() == reflect.Ptr {
			nv := setSliceValue(sv.Elem(), element)
			sv.Elem().Set(nv)
			return sv
		}
		if st.Elem().Kind() == reflect.Slice {
			et := st.Elem().Elem()
			ev := newValue(et, element)
			sv.Elem().Set(reflect.Append(sv.Elem(), ev))
			return sv
		}
	}
	var ev reflect.Value
	if isTypeInterface(st.Elem()) {
		ev = newValue(reflect.TypeOf(element), element)
	} else {
		ev = newValue(st.Elem(), element)
	}
	num := sv.Len()
	nslice := reflect.MakeSlice(st, num+1, num+1)
	reflect.Copy(nslice, sv)
	nslice.Index(num).Set(ev)
	return nslice
}

func copySliceValue(v reflect.Value) reflect.Value {
	sv := v
	st := sv.Type()
	if st.Kind() == reflect.Ptr {
		if st.Elem().Kind() == reflect.Ptr {
			cv := copySliceValue(sv.Elem())
			pv := newPtrOfValue(cv)
			return pv
		}
		sv = sv.Elem()
		st = st.Elem()
	}
	num := sv.Len()
	nslice := reflect.MakeSlice(st, num, num)
	reflect.Copy(nslice, sv)
	if sv != v {
		return newPtrOfValue(nslice)
	}
	return nslice
}

func setValueScalar(v reflect.Value, value interface{}) error {
	dv := v
	if dv.Kind() == reflect.Ptr {
		dv = v.Elem()
		if dv.Kind() == reflect.Ptr {
			if dv.IsNil() { // e.g. **type
				dv = reflect.New(dv.Type().Elem())
				v.Elem().Set(dv)
			}
			return setValueScalar(dv, value)
		}
	}
	dt := dv.Type()
	st := reflect.TypeOf(value)
	sv := reflect.ValueOf(value)
	if dt.Kind() == reflect.String {
		switch st.Kind() {
		case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64:
			dv.SetString(fmt.Sprintf("%d", sv.Int()))
			return nil
		case reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64, reflect.Uintptr:
			dv.SetString(fmt.Sprintf("%d", sv.Uint()))
			return nil
		case reflect.Float32, reflect.Float64:
			dv.SetString(fmt.Sprintf("%f", sv.Float()))
			return nil
		case reflect.Bool:
			dv.SetString(fmt.Sprint(sv.Bool()))
			return nil
		}
	}
	if st.Kind() == reflect.String {
		srcstring := value.(string)
		switch dt.Kind() {
		case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64:
			if len(srcstring) == 0 {
				dv.SetInt(0)
			} else {
				val, err := strconv.ParseInt(srcstring, 10, 64)
				if err != nil {
					return err
				}
				if dv.OverflowInt(val) {
					return fmt.Errorf("overflowInt: %s", DebugValueStringInline(val, 0, nil))
				}
				dv.SetInt(val)
			}
			return nil
		case reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64, reflect.Uintptr:
			if len(srcstring) == 0 {
				dv.SetUint(0)
			} else {
				val, err := strconv.ParseUint(srcstring, 10, 64)
				if err != nil {
					return err
				}
				if dv.OverflowUint(val) {
					return fmt.Errorf("OverflowUint: %s", DebugValueStringInline(val, 0, nil))
				}
				dv.SetUint(val)
			}
			return nil
		case reflect.Float32, reflect.Float64:
			if len(srcstring) == 0 {
				dv.SetFloat(0)
			} else {
				val, err := strconv.ParseFloat(srcstring, 64)
				if err != nil {
					return err
				}
				if dv.OverflowFloat(val) {
					return fmt.Errorf("OverflowFloat: %s", DebugValueStringInline(val, 0, nil))
				}
				dv.SetFloat(val)
			}
			return nil
		case reflect.Bool:
			if srcstring == "true" || srcstring == "True" || srcstring == "TRUE" {
				dv.SetBool(true)
			} else {
				dv.SetBool(false)
			}
			return nil
		}
	}
	if dt.Kind() == reflect.Bool {
		switch st.Kind() {
		case reflect.String:
			if len(sv.String()) > 0 && sv.String() == "true" || sv.String() == "True" || sv.String() == "TRUE" {
				dv.SetBool(true)
			} else {
				dv.SetBool(false)
			}
			return nil
		case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64:
			if sv.Int() != 0 {
				dv.SetBool(true)
			} else {
				dv.SetBool(false)
			}
			return nil
		case reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64, reflect.Uintptr:
			if sv.Uint() != 0 {
				dv.SetBool(true)
			} else {
				dv.SetBool(false)
			}
			return nil
		case reflect.Float32, reflect.Float64:
			if sv.Float() != 0 {
				dv.SetBool(true)
			} else {
				dv.SetBool(false)
			}
			return nil
		}
	}
	if st.Kind() == reflect.Bool {
		switch dt.Kind() {
		case reflect.String:
			if sv.Bool() {
				dv.SetString("true")
			} else {
				dv.SetString("false")
			}
			return nil
		case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64:
			if sv.Bool() {
				dv.SetInt(1)
			} else {
				dv.SetInt(0)
			}
			return nil
		case reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64, reflect.Uintptr:
			if sv.Bool() {
				dv.SetUint(1)
			} else {
				dv.SetUint(0)
			}
			return nil
		case reflect.Float32, reflect.Float64:
			if sv.Bool() {
				dv.SetFloat(1)
			} else {
				dv.SetFloat(0)
			}
			return nil
		}
	}
	if st.ConvertibleTo(dt) {
		dv.Set(sv.Convert(dt))
		return nil
	}
	return fmt.Errorf("Not Convertible: %s", DebugValueStringInline(v.Interface(), 0, nil))
}

func checkStructFieldTagName(ft reflect.StructField, name string) bool {
	tag := string(ft.Tag)
	index := strings.Index(tag, name)
	if index > 0 {
		if tag[index-1] == '"' || tag[index-1] == '\'' {
			l := len(name)
			if tag[index+l] == '"' || tag[index+l] == '\'' {
				return true
			}
		}
	}
	return false
}

func findStructField(sv reflect.Value, name interface{}) (reflect.StructField, reflect.Value, bool) {
	var st reflect.Type
	var fv reflect.Value
	st = sv.Type()
	fnv := newValue(reflect.TypeOf(""), name)
	if !fnv.IsValid() {
		return reflect.StructField{}, reflect.Value{}, false
	}
	sname := fnv.Interface().(string)
	ft, ok := st.FieldByName(sname)
	if ok {
		fv = sv.FieldByName(sname)
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

func setStructField(sv reflect.Value, name interface{}, value interface{}) error {
	if sv.Kind() == reflect.Ptr {
		if sv.IsNil() {
			cv := newValueStruct(sv.Type())
			sv.Set(cv)
		}
		return setStructField(sv.Elem(), name, value)
	}
	ft, fv, ok := findStructField(sv, name)
	if !ok {
		return fmt.Errorf("Not found %s.%s", sv.Type(), name)
	}
	if isSimpleType(ft.Type) {
		nv := NewValue(ft.Type, value)
		fv.Set(nv)
	} else {
		nv := NewValue(ft.Type, nil)
		fv.Set(nv)
	}
	return nil
}

func newValueStruct(t reflect.Type) reflect.Value {
	if t.Kind() == reflect.Ptr {
		cv := newValueStruct(t.Elem())
		pv := newPtrOfValue(cv)
		return pv
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
		case reflect.Map:
			fv.Set(reflect.MakeMap(ft.Type))
		case reflect.Slice:
			fv.Set(reflect.MakeSlice(ft.Type, 0, 0))
		case reflect.Chan:
			fv.Set(reflect.MakeChan(ft.Type, 0))
		case reflect.Struct:
			srv := newValueStruct(ft.Type)
			if !IsNilOrInvalidValue(srv) {
				fv.Set(srv)
			}
		case reflect.Ptr:
			// log.Debug(ft.Name, ft.Type)
			srv := newValue(ft.Type, nil)
			// log.Debug(fv, srv)
			if !IsNilOrInvalidValue(srv) {
				fv.Set(srv)
			}
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
	return pve
}

func newValueMap(t reflect.Type) reflect.Value {
	if t.Kind() == reflect.Ptr {
		cv := newValueMap(t.Elem())
		pv := newPtrOfValue(cv)
		return pv
	}
	return reflect.MakeMap(t)
}

func newValueSlice(t reflect.Type) reflect.Value {
	if t.Kind() == reflect.Ptr {
		cv := newValueSlice(t.Elem())
		pv := newPtrOfValue(cv)
		return pv
	}
	return reflect.MakeSlice(t, 0, 0)
}

func newValueChan(t reflect.Type) reflect.Value {
	if t.Kind() == reflect.Ptr {
		cv := newValueChan(t.Elem())
		pv := newPtrOfValue(cv)
		return pv
	}
	return reflect.MakeChan(t, 0)
}

func newValueInterface(t reflect.Type) reflect.Value {
	if t.Kind() == reflect.Ptr {
		cv := newValueChan(t.Elem())
		pv := newPtrOfValue(cv)
		return pv
	}
	return reflect.MakeChan(t, 0)
}

func newValueScalar(t reflect.Type) reflect.Value {
	if t.Kind() == reflect.Ptr {
		cv := newValueScalar(t.Elem())
		return newPtrOfValue(cv)
	}
	pv := reflect.New(t)
	pve := pv.Elem()
	// switch pve.Type().Kind() {
	// case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64:
	// 	pve.SetInt(0)
	// case reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64, reflect.Uintptr:
	// 	pve.SetUint(0)
	// case reflect.Float32, reflect.Float64:
	// 	pve.SetFloat(0)
	// case reflect.Bool:
	// 	pve.SetBool(false)
	// case reflect.String:
	// 	pve.SetString("")
	// default:
	// }
	return pve
}

func newValue(t reflect.Type, value interface{}) reflect.Value {
	if t == reflect.TypeOf(nil) {
		return reflect.Value{}
	}
	pt := getBaseType(t)
	if isTypeStruct(pt) {
		return newValueStruct(t)
	} else if isTypeMap(pt) {
		return newValueMap(t)
	} else if isTypeSlice(pt) {
		nv := newValueSlice(t)
		if value != nil || !IsNilOrInvalidValue(reflect.ValueOf(value)) {
			nv = setSliceValue(nv, value)
		}
		return nv
	} else if isTypeChan(pt) {
		return newValueChan(t)
	} else {
		v := newValueScalar(t)
		if IsValueNil(value) {
			return v
		}
		setValueScalar(v, value)
		return v
	}
}

// ptr wraps the given value with pointer: V => *V, *V => **V, etc.
func newPtrOfValue(v reflect.Value) reflect.Value {
	if !v.IsValid() {
		return reflect.Value{}
	}
	pt := reflect.PtrTo(v.Type()) // create a *T type.
	pv := reflect.New(pt.Elem())  // create a reflect.Value of type *T.
	pv.Elem().Set(v)              // sets pv to point to underlying value of v.
	return pv
}

// NewSimpleValue - Creates a reflect.Value of the simple type.
func NewSimpleValue(t reflect.Type, value interface{}) reflect.Value {
	if t == reflect.TypeOf(nil) {
		log.Warning("nil type:", t)
		return reflect.Value{}
	}
	if !isSimpleType(t) {
		log.Warning("not simple type:", t)
		return reflect.Value{}
	}
	v := newValueScalar(t)
	if IsValueNil(value) {
		log.Warning("simple type creation failed:", t)
		return reflect.Value{}
	}
	err := setValueScalar(v, value)
	if err != nil {
		log.Warning("simple type set failed:", err)
		return reflect.Value{}
	}
	return v
}

// NewValue - returns new Value based on t type
func NewValue(t reflect.Type, values ...interface{}) reflect.Value {
	if t == reflect.TypeOf(nil) {
		return reflect.Value{}
	}
	pt := getBaseType(t)
	switch pt.Kind() {
	case reflect.Array, reflect.Complex64, reflect.Complex128, reflect.Chan:
		log.Warning("Not supported value type:", pt.Kind())
		return reflect.Value{}
	case reflect.Struct:
		var key interface{} = nil
		nv := newValueStruct(t)
		for _, val := range values {
			if key == nil {
				key = val
			} else {
				setStructField(nv, key, val)
				key = nil
			}
		}
		return nv
	case reflect.Map:
		var key interface{} = nil
		nv := newValueMap(t)
		for _, val := range values {
			if key == nil {
				key = val
			} else {
				setMapValue(nv, key, val)
				key = nil
			}
		}
		return nv
	case reflect.Slice:
		nv := newValueSlice(t)
		for _, val := range values {
			nv = setSliceValue(nv, val)
		}
		return nv
	default:
		nv := newValueScalar(t)
		for _, val := range values {
			err := setValueScalar(nv, val)
			if err != nil {
				log.Warningf("Not settable value inserted '%s'", DebugValueStringInline(val, 0, nil))
			}
		}
		return nv
	}
}

// SetValue - Set a value.
func SetValue(v reflect.Value, values ...interface{}) reflect.Value {
	log.Debugf("SetValue T=%s, V=%v, VALUES=%s", v.Type(), v, values)
	if !v.IsValid() {
		return v
	}
	t := v.Type()
	pt := getBaseType(t)
	switch pt.Kind() {
	case reflect.Array, reflect.Complex64, reflect.Complex128, reflect.Chan:
		log.Warning("Not supported value type:", pt.Kind())
		return v
	case reflect.Struct:
		var key interface{} = nil
		for _, val := range values {
			if key == nil {
				key = val
			} else {
				err := setStructField(v, key, val)
				if err != nil {
					log.Warning("set failed:", err)
				}
				key = nil
			}
		}
		return v
	case reflect.Map:
		var key interface{} = nil
		for _, val := range values {
			if key == nil {
				key = val
			} else {
				err := setMapValue(v, key, val)
				if err != nil {
					log.Warning("set failed:", err)
				}
				key = nil
			}
		}
		return v
	case reflect.Slice:
		var nv reflect.Value
		isnil := isNilDeep(v)
		if isnil {
			nv = newValueSlice(t)
		} else {
			nv = copySliceValue(v)
		}
		for _, val := range values {
			nv = setSliceValue(nv, val)
		}
		if t.Kind() == reflect.Ptr {
			if v.Elem().CanSet() {
				v.Elem().Set(nv.Elem())
			} else {
				log.Warning("Not settable value", DebugValueStringInline(v.Interface(), 1, nil))
			}
		}
		return nv
	case reflect.Interface:
		// interface value is configured based on value's type.
		// log.Debug("reflect.Interface", v.Kind(), v.Elem().Kind())
		return SetValue(v.Elem(), values...)
	default:
		nv := v
		if t.Kind() != reflect.Ptr {
			nv = newValueScalar(t)
		}
		for _, val := range values {
			err := setValueScalar(nv, val)
			if err != nil {
				log.Warning("Not settable value:", DebugValueStringInline(v.Interface(), 0, nil))
			}
		}
		return nv
	}
}

// SetChildValue - Set a child value to the parent value.
func SetChildValue(pv reflect.Value, key interface{}, cv reflect.Value) error {
	log.Debug("SetChildValue", pv.Type(), cv.Type(), key)
	t := pv.Type()
	switch t.Kind() {
	case reflect.Array, reflect.Complex64, reflect.Complex128, reflect.Chan:
		return fmt.Errorf("Not supported parent type: %s", t.Kind())
	case reflect.Struct:
		_, fv, ok := findStructField(pv, key)
		if !ok {
			return fmt.Errorf("Not found %s.%s", pv.Type(), key)
		}
		fv.Set(cv)
		return nil
	case reflect.Map:
		kv := newValue(pv.Type().Key(), key)
		pv.SetMapIndex(kv, cv)
		log.Info("pv.", pv)
		return nil
	case reflect.Slice:
		// nv := copySliceValue(pv)
		// nv = setSliceValue(nv, cv.Interface())
		return nil
	default:
		pv.Set(cv)
		return nil
	}
}

func unsetMapValue(mv reflect.Value, key interface{}) error {
	if mv.Kind() == reflect.Ptr {
		return unsetMapValue(mv.Elem(), key)
	}
	if mv.Kind() != reflect.Map {
		return fmt.Errorf("not a Map value")
	}
	mt := mv.Type()
	kt := mt.Key()
	kv := newValue(kt, key)
	mv.SetMapIndex(kv, reflect.Value{})
	return nil
}

func unsetStructField(sv reflect.Value, name interface{}) error {
	if sv.Kind() == reflect.Ptr {
		return unsetStructField(sv.Elem(), name)
	}
	ft, fv, ok := findStructField(sv, name)
	if !ok {
		return fmt.Errorf("Not found %s.%s", sv.Type(), name)
	}
	nv := NewValue(ft.Type, "")
	fv.Set(nv)
	return nil
}

// UnsetValue - Unset a value indicated by the key from parents
func UnsetValue(v reflect.Value, key interface{}) error {
	log.Debugf("UnsetValue T=%s, V=%v, KEY=%s", v.Type(), v, key)
	if !v.IsValid() {
		return fmt.Errorf("Invalid value")
	}
	if !v.CanSet() {
		return fmt.Errorf("Not settable value")
	}
	t := v.Type()
	pt := getBaseType(t)
	switch pt.Kind() {
	case reflect.Array, reflect.Complex64, reflect.Complex128, reflect.Chan:
		return fmt.Errorf("Not supported value type (%s)", pt.Kind())
	case reflect.Struct:
		return unsetStructField(v, key)
	case reflect.Map:
		return unsetMapValue(v, key)
	case reflect.Slice:
		if index, ok := ValSliceFind(v, key); ok {
			return ValSliceDelete(v, index)
		}
		return fmt.Errorf("Not found unset value")
	case reflect.Interface:
		return UnsetValue(v.Elem(), key)
	default:
		return fmt.Errorf("Not supported scalar value unset")
	}
}

// GetValue - gets a value from the struct, map or slice value using the keys.
func GetValue(v reflect.Value, keys ...interface{}) reflect.Value {
	if !v.IsValid() {
		return reflect.Value{}
	}
	cur := v
	for i, key := range keys {
		switch cur.Kind() {
		case reflect.Ptr, reflect.Interface:
			rkeys := keys[i:]
			return GetValue(v.Elem(), rkeys...)
		case reflect.Struct:
			log.Warning("KEY", key)
			_, sfv, ok := findStructField(cur, key)
			if !ok {
				log.Warning("Not found:", key)
				return reflect.Value{}
			}
			cur = sfv
		case reflect.Map:
			mt := cur.Type()
			kv := newValue(mt.Key(), key)
			if !kv.IsValid() {
				log.Warning("Wrong key:", key)
				return reflect.Value{}
			}
			vv := cur.MapIndex(kv)
			if !vv.IsValid() {
				log.Warning("Not found:", key)
				return reflect.Value{}
			}
			cur = vv
		case reflect.Slice, reflect.Array:
			idxv := newValue(reflect.TypeOf(0), key)
			if !idxv.IsValid() {
				log.Warning("Wrong key:", key)
				return reflect.Value{}
			}
			index := idxv.Interface().(int)
			if cur.Len() <= index {
				log.Warning("Out of range:", index)
				return reflect.Value{}
			}
			vv := cur.Index(index)
			if !vv.IsValid() {
				log.Warning("Invalid value found:", key)
				return reflect.Value{}
			}
			cur = vv
		default:
			log.Warning("Not found:", key)
			return reflect.Value{}
		}
	}
	return cur
}

// FindValue - finds a value from the struct, map or slice value using the string keys.
func FindValue(v reflect.Value, keys ...string) reflect.Value {
	if !v.IsValid() {
		return reflect.Value{}
	}
	cur := v
	for i, key := range keys {
		switch cur.Kind() {
		case reflect.Ptr, reflect.Interface:
			rkeys := keys[i:]
			return FindValue(cur.Elem(), rkeys...)
		case reflect.Struct:
			_, sfv, ok := findStructField(cur, key)
			if !ok {
				// log.Debug("Not found:", key)
				return reflect.Value{}
			}
			cur = sfv
		case reflect.Map:
			mt := cur.Type()
			kv := newValue(mt.Key(), key)
			if !kv.IsValid() {
				// log.Debug("Wrong key:", key)
				return reflect.Value{}
			}
			vv := cur.MapIndex(kv)
			if !vv.IsValid() {
				// log.Debug("Not found:", key)
				return reflect.Value{}
			}
			cur = vv
		case reflect.Slice, reflect.Array:
			idxv := newValue(reflect.TypeOf(0), key)
			if !idxv.IsValid() {
				// log.Debug("Wrong key:", key)
				return reflect.Value{}
			}
			index := idxv.Interface().(int)
			if cur.Len() <= index {
				// log.Debug("Out of range:", index)
				return reflect.Value{}
			}
			vv := cur.Index(index)
			if !vv.IsValid() {
				// log.Debug("Invalid value found:", key)
				return reflect.Value{}
			}
			cur = vv
		default:
			// log.Debug("Not found:", key)
			return reflect.Value{}
		}
	}
	return cur
}

// FindValueWithParent - finds a value and its parent value from the struct, map or slice value using the string keys.
func FindValueWithParent(pv reflect.Value, cv reflect.Value, keys ...string) (reflect.Value, reflect.Value, string, bool) {
	var i int
	var key string
	if !cv.IsValid() {
		return reflect.Value{}, reflect.Value{}, "", false
	}
	if cv.Kind() == reflect.Ptr || cv.Kind() == reflect.Interface {
		log.Debug(" * cv:", cv.Kind(), keys)
		return FindValueWithParent(cv, cv.Elem(), keys...)
	}
	for i, key = range keys {
		log.Debug(" * cv:", cv.Kind(), keys)
		switch cv.Kind() {
		case reflect.Ptr, reflect.Interface:
			rkeys := keys[i:]
			return FindValueWithParent(cv, cv.Elem(), rkeys...)
		case reflect.Struct:
			_, sfv, ok := findStructField(cv, key)
			if !ok {
				log.Warning("Not found:", key)
				return reflect.Value{}, reflect.Value{}, "", false
			}
			pv = cv
			cv = sfv
		case reflect.Map:
			mt := cv.Type()
			kv := newValue(mt.Key(), key)
			if !kv.IsValid() {
				log.Warning("Wrong key:", key)
				return reflect.Value{}, reflect.Value{}, "", false
			}
			vv := cv.MapIndex(kv)
			if !vv.IsValid() {
				log.Warning("Not found:", key)
				return reflect.Value{}, reflect.Value{}, "", false
			}
			pv = cv
			cv = vv
		case reflect.Slice, reflect.Array:
			idxv := newValue(reflect.TypeOf(0), key)
			if !idxv.IsValid() {
				log.Warning("Wrong key:", key)
				return reflect.Value{}, reflect.Value{}, "", false
			}
			index := idxv.Interface().(int)
			if cv.Len() <= index {
				log.Warning("Out of range:", index)
				return reflect.Value{}, reflect.Value{}, "", false
			}
			vv := cv.Index(index)
			if !vv.IsValid() {
				log.Warning("Invalid value found:", key)
				return reflect.Value{}, reflect.Value{}, "", false
			}
			pv = cv
			cv = vv
		default:
			log.Warning("Not found:", key)
			return pv, cv, key, false
		}
	}
	return pv, cv, key, true
}

// IsYamlSeq - Return true if the tag is sequence object.
func IsYamlSeq(tag string) bool {
	switch tag {
	case "!!seq":
		return true
	default:
		return false
	}
}

// IsYamlMap - Return true if the tag is map object.
func IsYamlMap(tag string) bool {
	switch tag {
	case "!!map", "!!imap", "!!set", "!!omap":
		return true
	default:
		return false
	}
}

// IsYamlScalar - Return true if the tag is a scalar.
func IsYamlScalar(tag string) bool {
	switch tag {
	case "!!map", "!!imap", "!!set", "!!omap", "!!seq":
		return false
	default:
		return true
	}
}

// SetValueChild - Set a child value to the parent value.
func SetValueChild(pv reflect.Value, cv reflect.Value, key interface{}) (reflect.Value, error) {
	log.Debug("SetValueChild", pv.Type(), cv.Type(), key)
	v := GetNonIfOrPtrValueDeep(pv)
	switch v.Type().Kind() {
	case reflect.Array, reflect.Complex64, reflect.Complex128, reflect.Chan, reflect.Interface:
		return pv, fmt.Errorf("Not supported parent type (%s)", v.Type().Kind())
	case reflect.Struct:
		_, fv, ok := findStructField(v, key)
		if !ok {
			return pv, fmt.Errorf("Not found %s.%s", pv.Type(), key)
		}
		fv.Set(cv)
	case reflect.Map:
		kv := newValue(v.Type().Key(), key)
		v.SetMapIndex(kv, cv)
	case reflect.Slice:
		// nv := copySliceValue(pv)
		// nv = setSliceValue(nv, cv.Interface())
	default:
		if !pv.CanSet() {
			return pv, fmt.Errorf("Not settable type(%s)", pv.Type())
		}
		pv.Set(cv)
	}
	return pv, nil
}

// SetValueDeep - finds and sets a value.
func SetValueDeep(pv reflect.Value, cv reflect.Value, keys []string, key string, tag string, value string) error {
	var i int
	var k string
	var pkey string
	var err error
	// log.Debug("SetValueDeep", keys, key)
	if !cv.IsValid() {
		return fmt.Errorf("invalid parent value")
	}
	if cv.Kind() == reflect.Ptr || cv.Kind() == reflect.Interface {
		log.Debug(" * cv:", cv.Kind(), keys)
		return SetValueDeep(cv, cv.Elem(), keys, key, tag, value)
	}
	for i, k = range keys {
		log.Debug(" * cv:", cv.Kind(), keys)
		pkey = k
		switch cv.Kind() {
		case reflect.Ptr, reflect.Interface:
			rkeys := keys[i:]
			return SetValueDeep(cv, cv.Elem(), rkeys, key, tag, value)
		case reflect.Struct:
			_, sfv, ok := findStructField(cv, k)
			if !ok {
				return fmt.Errorf("Value not found(%s)", k)
			}
			pv = cv
			cv = sfv
		case reflect.Map:
			mt := cv.Type()
			kv := newValue(mt.Key(), k)
			if !kv.IsValid() {
				return fmt.Errorf("Wrong key(%s)", k)
			}
			vv := cv.MapIndex(kv)
			if !vv.IsValid() {
				return fmt.Errorf("Value not found(%s)", k)
			}
			pv = cv
			cv = vv
		case reflect.Slice:
			idxv := newValue(reflect.TypeOf(0), k)
			if !idxv.IsValid() {
				return fmt.Errorf("Wrong key(%s)", k)
			}
			index := idxv.Interface().(int)
			if cv.Len() <= index {
				return fmt.Errorf("Index is out of the range(%d)", index)
			}
			vv := cv.Index(index)
			if !vv.IsValid() {
				return fmt.Errorf("Invalid value found(%s)", k)
			}
			pv = cv
			cv = vv
		case reflect.Array:
			return fmt.Errorf("Not supported type (Array)")
		default: // scalar
		}

		if len(keys) == i+1 {
			if IsValueInterface(cv) && isValueScalar(cv.Elem()) {
				log.Debug("pv", pv.Kind(), "cv", cv.Kind(), "cv.Elem()", cv.Elem().Kind())
				cv = reflect.ValueOf(map[string]interface{}{})
				pv, err = SetValueChild(pv, cv, k)
				if err != nil {
					return err
				}
			}
		}
	}
	log.Debugf("Before pv: %s: %s", pv.Kind(), DebugValueStringInline(pv.Interface(), 1, nil))
	log.Debugf("Before cv: %s: %s", cv.Kind(), DebugValueStringInline(cv.Interface(), 1, nil))

	cv = GetNonInterfaceValueDeep(cv)
	if IsYamlSeq(tag) {
		nv := reflect.ValueOf([]interface{}{})
		cv, err = SetValueChild(cv, nv, key)
		if err != nil {
			return err
		}
	} else {
		var values []interface{}
		switch tag {
		case "!!map", "!!imap", "!!omap":
			values = []interface{}{key, value}
		case "!!set":
			values = []interface{}{key}
		case "!!seq":
			values = []interface{}{value}
		default: // other scalar types:
			if IsValueSlice(cv) {
				if key == "" {
					values = []interface{}{value}
				} else {
					values = []interface{}{key}
				}
			} else {
				values = []interface{}{key, value}
			}
		}
		nv := SetValue(cv, values...)
		if nv.Kind() == reflect.Slice {
			pv, err = SetValueChild(pv, nv, pkey)
			if err != nil {
				return err
			}
		}
	}

	log.Debugf("After cv: %s", cv.Kind())
	DebugValueString(cv.Interface(), 1, log.Debug)
	return nil
}

// UnsetValueDeep - finds and unsets a value in deep.
func UnsetValueDeep(pv reflect.Value, cv reflect.Value, keys []string, key string) error {
	var i int
	var k string
	// log.Debug("UnsetValueDeep", keys, key)
	if !cv.IsValid() {
		return fmt.Errorf("invalid parent value")
	}
	if cv.Kind() == reflect.Ptr || cv.Kind() == reflect.Interface {
		log.Debug(" * cv:", cv.Kind(), keys)
		return UnsetValueDeep(cv, cv.Elem(), keys, key)
	}
	for i, k = range keys {
		log.Debug(" * cv:", cv.Kind(), keys)
		switch cv.Kind() {
		case reflect.Ptr, reflect.Interface:
			rkeys := keys[i:]
			return UnsetValueDeep(cv, cv.Elem(), rkeys, key)
		case reflect.Struct:
			_, sfv, ok := findStructField(cv, k)
			if !ok {
				return fmt.Errorf("Value not found(%s)", k)
			}
			pv = cv
			cv = sfv
		case reflect.Map:
			mt := cv.Type()
			kv := newValue(mt.Key(), k)
			if !kv.IsValid() {
				return fmt.Errorf("Wrong key(%s)", k)
			}
			vv := cv.MapIndex(kv)
			if !vv.IsValid() {
				return fmt.Errorf("Value not found(%s)", k)
			}
			pv = cv
			cv = vv
		case reflect.Slice:
			idxv := newValue(reflect.TypeOf(0), k)
			if !idxv.IsValid() {
				return fmt.Errorf("Wrong key(%s)", k)
			}
			index := idxv.Interface().(int)
			if cv.Len() <= index {
				return fmt.Errorf("Index is out of the range(%d)", index)
			}
			vv := cv.Index(index)
			if !vv.IsValid() {
				return fmt.Errorf("Invalid value found(%s)", k)
			}
			pv = cv
			cv = vv
		case reflect.Array:
			return fmt.Errorf("Not supported type (Array)")
		default: // scalar
		}
	}
	log.Debugf("Before pv: %s: %s", pv.Kind(), DebugValueStringInline(pv.Interface(), 1, nil))
	log.Debugf("Before cv: %s: %s", cv.Kind(), DebugValueStringInline(cv.Interface(), 1, nil))

	var err error
	cv = GetNonInterfaceValueDeep(cv)
	if key != "" {
		err = UnsetValue(cv, key)
	}

	log.Debugf("After cv: %s", cv.Kind())
	DebugValueString(cv.Interface(), 1, log.Debug)
	return err
}
