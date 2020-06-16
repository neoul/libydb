package ydb

import (
	"fmt"
	"reflect"
)

// ValYdbSet - constructs the non-updater struct
func ValYdbSet(v reflect.Value, keys []string, key string, tag string, value string) error {
	// keys, key = keyListing(keys, key)
	var pv reflect.Value
	for _, k := range keys {
		cv, ok := ValFindOrInit(v, k)
		if !ok || !cv.IsValid() {
			return fmt.Errorf("%s not found", k)
		}
		if IsValScalar(cv) {
			if !IsTypeInterface(cv.Type()) {
				return fmt.Errorf("%s value can be contained to the value of %s type", key, k)
			}
			nv := reflect.ValueOf(map[string]interface{}{})
			if err := ValChildDirectSet(v, k, nv); err != nil {
				return err
			}
			cv = nv
		}
		pv = v
		v = cv
	}
	// ct, ok := TypeFind(v.Type(), key)
	// if ok && value != "" {
	// 	cv, err := ytypes.StringToType(ct, value)
	// 	if err == nil {
	// 		return ValChildDirectSet(v, key, cv)
	// 	}
	// }
	if IsTypeInterface(v.Type()) {
		fmt.Println("v", v.Kind(), v.Type())
		fmt.Println("ve", v.Elem().Kind(), v.Elem().Type())
		var nv reflect.Value
		switch tag {
		case "!!map", "!!imap", "!!omap":
			nv = reflect.ValueOf(map[string]interface{}{})
			return ValChildDirectSet(v, key, nv)
		case "!!set", "!!seq":
			nv = reflect.ValueOf([]interface{}{})
			return ValChildDirectSet(v, key, nv)
		default: // other types:
			if v.Elem().CanSet() {
				_, err := ValChildSet(v, key, value)
				return err
			}
			// e.g. a slice element in a map.
			fmt.Println("Need to add new v to pv ...", pv.Kind())
			return nil
		}
	}

	_, err := ValChildSet(v, key, value)
	// cv, err := ValChildSet(v, key, value)
	// if err == nil {
	// 	DebugValueString(cv.Interface(), 1, func(x ...interface{}) { fmt.Print(x...) })
	// } else {
	// 	fmt.Println(err)
	// }
	return err
}

// ValYdbUnset - constructs the non-updater struct
func ValYdbUnset(v reflect.Value, keys []string, key string) error {
	// keys, key = keyListing(keys, key)
	for _, k := range keys {
		cv, ok := ValFind(v, k)
		if !ok || !cv.IsValid() {
			return fmt.Errorf("key %s not found", k)
		}
		v = cv
	}
	err := ValChildUnset(v, key)
	// if err == nil {
	// 	DebugValueString(v.Interface(), 1, func(x ...interface{}) { fmt.Print(x...) })
	// }
	return err
}
