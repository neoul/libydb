package main // import "github.com/neoul/libydb/go/ygot"

import (
	"fmt"
	"os"
	"reflect"

	"github.com/neoul/libydb/go/ydb"
	"github.com/neoul/libydb/go/ygot/model/schema"

	"github.com/op/go-logging"
	"github.com/openconfig/goyang/pkg/yang"
	"github.com/openconfig/ygot/ytypes"
)

// Generate rule to create the example structs:
//go:generate go run ../../../../../github.com/openconfig/ygot/generator/generator.go -path=yang -output_file=model/schema/example.go -package_name=schema -generate_fakeroot -fakeroot_name=example model/yang/example.yang

var (
	// Schema is schema information generated by ygot
	Schema *ytypes.Schema
	// Entries is yang.Entry list rearranged by name
	Entries map[string][]*yang.Entry
	ylog    *logging.Logger
)

func init() {
	// ylog = ydb.SetLog("ydb2ygot", os.Stderr, logging.DEBUG, "%{message}")
	ylog = ydb.SetLog("ydb2ygot", os.Stderr, logging.DEBUG, "")

	schema, err := schema.Schema()
	if err != nil {
		ylog.Panicf("%s\n", err)
	}
	Schema = schema
	Entries = make(map[string][]*yang.Entry)
	for _, branch := range schema.SchemaTree {
		entries, _ := Entries[branch.Name]
		entries = append(entries, branch)
		for _, leaf := range branch.Dir {
			entries = append(entries, leaf)
		}
		Entries[branch.Name] = entries
		// if branch.Annotation["schemapath"] == "/" {
		// 	SchemaRoot = branch
		// }
	}
	// for _, i := range Entries {
	// 	for _, j := range i {
	// 		ylog.Debug(j)
	// 	}
	// }
}

func find(entry *yang.Entry, keys ...string) *yang.Entry {
	var found *yang.Entry
	if entry == nil {
		return nil
	}
	if len(keys) > 1 {
		found = entry.Dir[keys[0]]
		if found == nil {
			return nil
		}
		found = find(found, keys[1:]...)
	} else {
		found = entry.Dir[keys[0]]
	}
	return found
}

// GoStruct - Go Struct for YDB unmarshal
type GoStruct schema.Example

// Create - constructs schema.Example
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

// Create - constructs schema.Example
func (example *GoStruct) Create(keys []string, key string, tag string, value string) error {
	ylog.Debugf("GoStruct.Create %v %v %v %v {", keys, key, tag, value)
	// err := merge(example, keys, key, tag, value)
	v := reflect.ValueOf(example)
	err := ydb.SetInterfaceValue(v, v, keys, key, tag, value)
	ylog.Debugf("}")
	return err
}

// Replace - constructs schema.Example
func (example *GoStruct) Replace(keys []string, key string, tag string, value string) error {
	ylog.Debugf("GoStruct.Replace %v %v %v %v {", keys, key, tag, value)
	// err := merge(example, keys, key, tag, value)
	v := reflect.ValueOf(example)
	err := ydb.SetInterfaceValue(v, v, keys, key, tag, value)
	ylog.Debugf("}")
	return err
}

// Delete - constructs schema.Example
func (example *GoStruct) Delete(keys []string, key string) error {
	ylog.Debugf("GoStruct.Delete %v %v %v %v {", keys, key)
	v := reflect.ValueOf(example)
	err := ydb.UnsetInterfaceValue(v, v, keys, key)
	ylog.Debugf("}")
	return err
	// return nil
}

func main() {
	example := Schema.Root
	ylog.Debug(example)

	db, close := ydb.Open("mydb")
	defer close()
	r, err := os.Open("model/data/example.yaml")
	defer r.Close()
	if err != nil {
		ylog.Fatal(err)
	}
	dec := db.NewDecoder(r)
	dec.Decode()

	var user interface{}
	user, err = db.Convert(ydb.RetrieveAll())
	ylog.Debug(user)

	// var user map[string]interface{}
	// user = map[string]interface{}{}
	// _, err = db.Convert(ydb.RetrieveAll(), ydb.RetrieveStruct(user))
	// ylog.Debug(user)

	// gs := GoStruct{}
	// _, err = db.Convert(ydb.RetrieveAll(), ydb.RetrieveStruct(&gs))
	// ylog.Debug(gs)
	// fmt.Println(*gs.Country["United Kingdom"].Name, *gs.Country["United Kingdom"].CountryCode, *gs.Country["United Kingdom"].DialCode)
	// fmt.Println(*&gs.Company.Address)

	// gs := schema.Example{}
	// _, err = db.Convert(ydb.RetrieveAll(), ydb.RetrieveStruct(&gs))
	// ylog.Debug(gs)
	// fmt.Println(*gs.Country["United Kingdom"].Name, *gs.Country["United Kingdom"].CountryCode, *gs.Country["United Kingdom"].DialCode)
	// fmt.Println(*&gs.Company.Address)

	// gs := schema.Example{}
	// db, close := ydb.OpenWithTargetStruct("running", &gs)
	// defer close()
	// r, err := os.Open("model/data/example.yaml")
	// defer r.Close()
	// if err != nil {
	// 	ylog.Fatal(err)
	// }
	// dec := db.NewDecoder(r)
	// dec.Decode()
}