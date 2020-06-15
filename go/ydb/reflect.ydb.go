package ydb

import (
	"fmt"
	"reflect"
)

// ValYdbSet - constructs the non-updater struct
func ValYdbSet(v reflect.Value, keys []string, key string, tag string, value string) error {
	// keys, key = keyListing(keys, key)
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
			_, err := ValChildDirectSet(v, k, nv)
			if err != nil {
				return err
			}
			cv = nv
		}
		v = cv
	}
	// ct, ok := TypeFind(v.Type(), key)
	// if ok && value != "" {
	// 	cv, err := ytypes.StringToType(ct, value)
	// 	if err == nil {
	// 		cv, err = ValChildDirectSet(v, key, cv)
	// 		return err
	// 	}
	// }
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
