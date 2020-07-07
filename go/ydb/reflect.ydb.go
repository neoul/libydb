package ydb

import (
	"fmt"
	"reflect"
)

// ValYdbSet - constructs the non-updater struct
func ValYdbSet(v reflect.Value, keys []string, key string, tag string, value string) error {
	// keys, key = keyListing(keys, key)
	var pkey string
	var pv reflect.Value
	for _, k := range keys {
		cv, ok := ValFindOrInit(v, k, GetLastEntry)
		if !ok || !cv.IsValid() {
			return fmt.Errorf("%s not found", k)
		}
		if IsValScalar(cv) {
			if !IsTypeInterface(cv.Type()) {
				return fmt.Errorf("%s value can be contained to the value of %s type", key, k)
			}
			nv := reflect.ValueOf(map[string]interface{}{})
			if _, err := ValChildDirectSet(v, k, nv); err != nil {
				return err
			}
			cv = nv
		}
		pv = v
		v = cv
		pkey = k
	}

	ct, ok := TypeFind(v.Type(), key)
	if ok && IsTypeInterface(ct) {
		switch tag {
		case "!!map", "!!imap", "!!omap", "!!set":
			nv := reflect.ValueOf(map[string]interface{}{})
			vv, err := ValChildDirectSet(v, key, nv)
			if vv != v {
				_, err := ValChildDirectSet(pv, pkey, vv)
				return err
			}
			return err
		case "!!seq":
			nv := reflect.ValueOf([]interface{}{})
			vv, err := ValChildDirectSet(v, key, nv)
			if vv != v {
				_, err := ValChildDirectSet(pv, pkey, vv)
				return err
			}
			return err
		default:
		}
	}
	rv, err := ValChildSet(v, key, value, NoSearch)
	if err != nil {
		return err
	}
	if rv.Kind() == reflect.Slice {
		_, err = ValChildDirectSet(pv, pkey, rv)
	}
	return err
}

// ValYdbUnset - constructs the non-updater struct
func ValYdbUnset(v reflect.Value, keys []string, key string) error {
	// keys, key = keyListing(keys, key)
	var pkey string
	var pv reflect.Value
	for _, k := range keys {
		cv, ok := ValFind(v, k, GetFirstEntry)
		if !ok || !cv.IsValid() {
			return fmt.Errorf("key %s not found", k)
		}
		pv = v
		v = cv
		pkey = k
	}
	rv, err := ValChildUnset(v, key, NoSearch)
	if err != nil {
		return err
	}
	if rv != v {
		_, err = ValChildDirectSet(pv, pkey, rv)
	}
	return err
}
