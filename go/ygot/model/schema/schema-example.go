package schema

import (
	"fmt"
	"os"
	"reflect"

	"github.com/neoul/libydb/go/ydb"
	"github.com/op/go-logging"
)

//go:generate go run ../../../../../../../github.com/openconfig/ygot/generator/generator.go -path=yang -output_file=generated.go -package_name=schema -generate_fakeroot -fakeroot_name=device ../yang/example.yang

var ylog *logging.Logger

func init() {
	// ylog = ydb.SetLog("ydb2ygot", os.Stdout, logging.DEBUG, "%{message}")
	ylog = ydb.SetLog("schema", os.Stderr, logging.DEBUG, "")
}

// GoStruct - Go Struct for YDB unmarshal
type GoStruct Device

// Create - constructs Device
func merge(example *GoStruct, keys []string, key string, tag string, value string) error {
	var pkey string
	var pv, cv reflect.Value

	dv := reflect.ValueOf(example)
	cv = dv
	if len(keys) > 0 {
		pv, cv, pkey = ydb.FindValueWithParent(cv, cv, keys...)
		if !cv.IsValid() {
			return fmt.Errorf("invalid parent value")
		}
	}
	// if cv.IsNil() {
	// 	cv = ydb.NewValue(cv.Type())
	// 	dv.Set(cv)
	// }
	var values []interface{}
	switch tag {
	case "!!map", "!!imap", "!!omap":
		values = []interface{}{key, value}
	case "!!set":
		values = []interface{}{key}
	case "!!seq":
		values = []interface{}{value}
	default: // other scalar types:
		if ydb.IsValueSlice(cv) {
			if key == "" {
				values = []interface{}{value}
			} else {
				values = []interface{}{key}
			}
		} else {
			values = []interface{}{key, value}
		}
	}
	nv := ydb.SetValue(cv, values...)
	if nv.Kind() == reflect.Slice {
		var err error
		pv, err = ydb.SetValueChild(pv, nv, pkey)
		if err != nil {
			return err
		}
	}
	return nil
}

// Create - constructs Device
func (example *GoStruct) Create(keys []string, key string, tag string, value string) error {
	ylog.Debugf("GoStruct.Create %v %v %v %v {", keys, key, tag, value)
	// err := merge(example, keys, key, tag, value)
	v := reflect.ValueOf(example)
	err := ydb.SetInterfaceValue(v, v, keys, key, tag, value)
	ylog.Debugf("}")
	return err
}

// Replace - constructs Device
func (example *GoStruct) Replace(keys []string, key string, tag string, value string) error {
	ylog.Debugf("GoStruct.Replace %v %v %v %v {", keys, key, tag, value)
	// err := merge(example, keys, key, tag, value)
	v := reflect.ValueOf(example)
	err := ydb.SetInterfaceValue(v, v, keys, key, tag, value)
	ylog.Debugf("}")
	return err
}

// Delete - constructs Device
func (example *GoStruct) Delete(keys []string, key string) error {
	ylog.Debugf("GoStruct.Delete %v %v %v %v {", keys, key)
	v := reflect.ValueOf(example)
	err := ydb.UnsetInterfaceValue(v, v, keys, key)
	ylog.Debugf("}")
	return err
	// return nil
}
