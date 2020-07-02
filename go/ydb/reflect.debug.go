package ydb

import (
	"bufio"
	"fmt"
	"io"
	"reflect"
	"strings"
)

// DebugValueString returns a string representation of value which may be a value, ptr,
// or struct type.
// - value: The value to print.
// - depth: The depth of the printed values and types.
// - print: The print function
func DebugValueString(value interface{}, depth int, print func(a ...interface{}), excludedField ...string) string {
	v := reflect.ValueOf(value)
	s := debugValueStr(v, depth, 0, "", false, false, excludedField...)
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

func isExcludedField(fieldname string, excludedField ...string) bool {
	if len(excludedField) > 0 {
		for _, exf := range excludedField {
			if exf == fieldname {
				return true
			}
		}
	}
	return false
}

func debugValueStr(v reflect.Value, depth, ptrcnt int, indent string, disableIndent bool, noIndent bool, excludedField ...string) string {
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
			return fmt.Sprintf("%s(%v)", v.Type(), v)
		}
		return indent + fmt.Sprintf("%s(%v)", v.Type(), v)
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
	_depth := depth
	switch v.Kind() {
	case reflect.Ptr:
		ptrcnt++
		out = "*" + debugValueStr(v.Elem(), depth, ptrcnt, indent, true, noIndent, excludedField...)
	case reflect.Interface:
		ptrcnt++
		out = "٭" + debugValueStr(v.Elem(), depth, ptrcnt, indent, true, noIndent, excludedField...)
	case reflect.Slice:
		out = fmt.Sprintf("%s(", v.Type())
		for i := 0; i < v.Len(); i++ {
			if !noIndent && depth > 0 {
				out += "\n"
			}
			out += debugValueStr(v.Index(i), depth-1, 0, indent+"• ", false, noIndent, excludedField...)
		}
		out += ")"
	case reflect.Struct:
		t := v.Type()
		out = fmt.Sprintf("%s(", v.Type())
		for i := 0; i < v.NumField(); i++ {
			fv := v.Field(i)
			ft := t.Field(i)
			if areSameType(ft.Type, t) {
				continue
			}
			if isExcludedField(ft.Name, excludedField...) {
				depth = 0
			}
			if noIndent {
				if fv.CanInterface() {
					out += fmt.Sprintf("\n%s:%v", ft.Name, debugValueStr(fv, depth-1, 0, indent+"• ", true, noIndent, excludedField...))
				} else {
					out += fmt.Sprintf("\n%s:%v", ft.Name, fv)
				}
			} else {
				if _depth > 0 {
					out += fmt.Sprintf("\n%s", indent+"• ")
				} else {
					out += " "
				}
				if fv.CanInterface() {
					out += fmt.Sprintf("%s:%v", ft.Name, debugValueStr(fv, depth-1, 0, indent+"• ", true, noIndent, excludedField...))
				} else {
					out += fmt.Sprintf("%s:%v", ft.Name, fv)
				}
			}
			depth = _depth
		}
		out += ")"
	case reflect.Map:
		out = fmt.Sprintf("%s(", v.Type())
		iter := v.MapRange()
		for iter.Next() {
			k := iter.Key()
			e := iter.Value()
			if k.Kind() == reflect.String {
				if isExcludedField(k.Interface().(string), excludedField...) {
					depth = 0
				}
			}
			if noIndent {
				out += fmt.Sprintf("\n%v:%s", k, debugValueStr(e, depth-1, 0, indent+"• ", true, noIndent, excludedField...))
			} else {
				if _depth > 0 {
					out += fmt.Sprintf("\n%s", indent+"• ")
				} else {
					out += " "
				}
				out += fmt.Sprintf("%v:%s", k, debugValueStr(e, depth-1, 0, indent+"• ", true, noIndent, excludedField...))
			}
			depth = _depth
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
