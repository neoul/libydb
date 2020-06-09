package schema

import (
	"fmt"
	"os"
	"reflect"

	"github.com/neoul/libydb/go/ydb"
	"github.com/op/go-logging"
	"github.com/openconfig/ygot/ytypes"
)

//go:generate go run ../../../../../../../github.com/openconfig/ygot/generator/generator.go -path=yang -output_file=generated.go -package_name=schema -generate_fakeroot -fakeroot_name=device ../yang/example.yang

func init() {
	ydb.SetLog("ydb", os.Stdout, logging.ERROR, "%{message}")
}

// Merge - constructs Device
func Merge(device *Device, keys []string, key string, tag string, value string) error {
	var pkey string
	var pv, cv reflect.Value
	var err error
	var ok bool

	v := reflect.ValueOf(device)
	cv = v
	if len(keys) > 0 {
		pv, cv, pkey, ok = ydb.FindValueWithParent(cv, cv, keys...)
		if !cv.IsValid() {
			return fmt.Errorf("Invalid parent value")
		}
		if !ok {
			return fmt.Errorf("Parent value not found: %s", pkey)
		}
	}

	cv = ydb.GetNonInterfaceValueDeep(cv)
	if ydb.IsYamlSeq(tag) {
		nv := reflect.ValueOf([]interface{}{})
		cv, err = ydb.SetValueChild(cv, nv, key)
		if err != nil {
			return err
		}
	} else {
		var values []interface{}
		// Try to update value via ygot
		v = ydb.FindValue(cv, key)
		if v.IsValid() && value != "" {
			v, err = ytypes.StringToType(v.Type(), value)
			if err == nil {
				// if cv.Kind() == reflect.Ptr {
				// 	cv = cv.Elem()
				// }
				// fmt.Printf("%T %s", v.Interface(), v)
				cv, err = ydb.SetValueChild(cv, v, key)
				return err
			}
		}

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
			pv, err = ydb.SetValueChild(pv, nv, pkey)
			if err != nil {
				return err
			}
		}
	}
	return nil
}

// Create - constructs Device
func (device *Device) Create(keys []string, key string, tag string, value string) error {
	fmt.Printf("Device.Create %v %v %v %v {\n", keys, key, tag, value)
	err := Merge(device, keys, key, tag, value)
	fmt.Println("}", err)
	return err
}

// Replace - constructs Device
func (device *Device) Replace(keys []string, key string, tag string, value string) error {
	fmt.Printf("Device.Replace %v %v %v %v {\n", keys, key, tag, value)
	err := Merge(device, keys, key, tag, value)
	fmt.Println("}", err)
	return err
}

// Delete - constructs Device
func (device *Device) Delete(keys []string, key string) error {
	fmt.Printf("Device.Delete %v %v {\n", keys, key)
	v := reflect.ValueOf(device)
	err := ydb.UnsetValueDeep(v, v, keys, key)
	fmt.Println("}", err)
	return err
}
