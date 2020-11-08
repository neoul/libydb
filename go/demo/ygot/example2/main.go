package main

import (
	"fmt"
	"os"
	"reflect"

	"github.com/neoul/gostruct-dump/dump"
	"github.com/neoul/libydb/go/demo/ygot/model/gostruct"
	"github.com/neoul/libydb/go/ydb"
	"github.com/openconfig/ygot/ytypes"
)

// ygot/example1 is an example to update ygot struct using the default YDB Updater interface.
// ygot/example2 is an example to update ygot struct using the user-defined YDB Updater interface.

// MyDevice is an wrapping type to define YDB Updater interface
type MyDevice gostruct.Device

// Create constructs the Model instance
func (device *MyDevice) Create(keys []string, key string, tag string, value string) error {
	return merge(device, keys, key, tag, value)
}

// Replace constructs the Model instance
func (device *MyDevice) Replace(keys []string, key string, tag string, value string) error {
	return merge(device, keys, key, tag, value)
}

// Delete constructs the Model instance
func (device *MyDevice) Delete(keys []string, key string) error {
	return delete(device, keys, key)
}

func merge(device *MyDevice, keys []string, key string, tag string, value string) error {
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
	_, err := ydb.ValChildSet(v, key, value, ydb.SearchByContent)
	return err
}

func delete(device *MyDevice, keys []string, key string) error {
	v := reflect.ValueOf(device)
	for _, k := range keys {
		cv, ok := ydb.ValFind(v, k, ydb.SearchByContent)
		if !ok || !cv.IsValid() {
			return fmt.Errorf("key %s not found", k)
		}
		v = cv
	}
	_, err := ydb.ValChildUnset(v, key, ydb.SearchByContent)
	return err
}

func main() {
	gs := MyDevice{}
	db, close := ydb.OpenWithTargetStruct("running", &gs)
	defer close()
	r, err := os.Open("../model/data/example-ygot.yaml")
	defer r.Close()
	if err != nil {
		panic(err)
	}
	dec := db.NewDecoder(r)
	dec.Decode()
	dump.Print(gs)
	fmt.Println("")
	fmt.Println(*gs.Country["United Kingdom"].Name, *gs.Country["United Kingdom"].CountryCode, *gs.Country["United Kingdom"].DialCode)
	fmt.Println(*&gs.Company.Address, gs.Company.Enumval)
	fmt.Println("")
}
