package schema

import (
	"fmt"
	"os"
	"reflect"
	"strings"

	"github.com/neoul/libydb/go/ydb"
	"github.com/op/go-logging"
	"github.com/openconfig/ygot/ytypes"
)

//go:generate go run ../../../../../../../github.com/openconfig/ygot/generator/generator.go -path=yang -output_file=generated.go -package_name=schema -generate_fakeroot -fakeroot_name=device ../yang/example.yang

func init() {
	ydb.SetLog("ydb", os.Stdout, logging.ERROR, "%{message}")
	ydb.ValFindByContent = true
}

func keyListing(keys []string, key string) ([]string, string) {
	var keylist []string
	if len(key) > 0 {
		for _, k := range keys {
			i := strings.Index(k, "[")
			if i > 0 {
				ename := k[:i]
				kname := strings.Trim(k[i:], "[]")
				i = strings.Index(kname, "=")
				if i > 0 {
					kname = kname[i+1:]
					keylist = append(keylist, ename)
					keylist = append(keylist, kname)
					continue
				}
			}
			keylist = append(keylist, k)
		}
	}
	i := strings.Index(key, "[")
	if i > 0 {
		ename := key[:i]
		kname := strings.Trim(key[i:], "[]")
		i = strings.Index(kname, "=")
		if i > 0 {
			kname = kname[i+1:]
			keylist = append(keylist, ename)
			key = kname
		}
	}
	return keylist, key
}

// Merge - constructs Device
func merge(device *Device, keys []string, key string, tag string, value string) error {
	keys, key = keyListing(keys, key)
	fmt.Printf("Device.merge %v %v %v %v\n", keys, key, tag, value)
	v := reflect.ValueOf(device)
	for _, k := range keys {
		cv, ok := ydb.ValFindOrInit(v, k, ydb.SearchByContent)
		if !ok || !cv.IsValid() {
			return fmt.Errorf("key %s not found", k)
		}
		v = cv
	}
	ct, ok := ydb.TypeFind(v.Type(), key)
	if ok && value != "" {
		cv, err := ytypes.StringToType(ct, value)
		if err == nil {
			_, err = ydb.ValChildDirectSet(v, key, cv)
			return err
		}
	}
	cv, err := ydb.ValChildSet(v, key, value)
	if err == nil {
		ydb.DebugValueString(cv.Interface(), 1, func(x ...interface{}) { fmt.Print(x...) })
	} else {
		fmt.Println(err)
	}
	return nil
}

// Merge - constructs Device
func delete(device *Device, keys []string, key string) error {
	keys, key = keyListing(keys, key)
	fmt.Printf("Device.delete %v %v\n", keys, key)
	v := reflect.ValueOf(device)
	for _, k := range keys {
		cv, ok := ydb.ValFind(v, k, ydb.SearchByContent)
		if !ok || !cv.IsValid() {
			return fmt.Errorf("key %s not found", k)
		}
		v = cv
	}
	err := ydb.ValChildUnset(v, key)
	if err == nil {
		ydb.DebugValueString(v.Interface(), 1, func(x ...interface{}) { fmt.Print(x...) })
	} else {
		fmt.Println(err)
	}
	return nil
}

// Create - constructs Device
func (device *Device) Create(keys []string, key string, tag string, value string) error {
	fmt.Printf("Device.Create %v %v %v %v {\n", keys, key, tag, value)
	err := merge(device, keys, key, tag, value)
	fmt.Println("}", err)
	return err
}

// Replace - constructs Device
func (device *Device) Replace(keys []string, key string, tag string, value string) error {
	fmt.Printf("Device.Replace %v %v %v %v {\n", keys, key, tag, value)
	err := merge(device, keys, key, tag, value)
	fmt.Println("}", err)
	return err
}

// Delete - constructs Device
func (device *Device) Delete(keys []string, key string) error {
	fmt.Printf("Device.Delete %v %v {\n", keys, key)
	err := delete(device, keys, key)
	fmt.Println("}", err)
	return err
}
