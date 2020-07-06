package ydb

import (
	"fmt"
	"strings"
)

// strval (structured values for non-scalar value creation in YDB.)
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

// ExtractStrValNameAndValue - Extract the struct name, field values
func ExtractStrValNameAndValue(s interface{}) (string, map[string]string, error) {
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

// ExtractStrValNameAndSubstring - Extract the struct name, field values
func ExtractStrValNameAndSubstring(s interface{}) (string, string, error) {
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
