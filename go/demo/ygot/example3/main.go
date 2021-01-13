package main

import (
	"fmt"
	"os"
	"reflect"

	"github.com/neoul/gdump"
	"github.com/neoul/gnxi/utilities/status"
	"github.com/neoul/gnxi/utilities/xpath"
	"github.com/neoul/libydb/go/demo/ygot/model/gostruct"
	"github.com/neoul/libydb/go/ydb"
	gnmipb "github.com/openconfig/gnmi/proto/gnmi"
	"github.com/openconfig/goyang/pkg/yang"
	"github.com/openconfig/ygot/ygot"
	"github.com/openconfig/ygot/ytypes"
)

// ygot/example1 is an example to update ygot struct using the default YDB Updater interface.
// ygot/example2 is an example to update ygot struct using the user-defined YDB Updater interface.

// device is an wrapping type to define YDB Updater interface
type device ytypes.Schema

func (d *device) RootSchema() *yang.Entry {
	return d.SchemaTree[reflect.TypeOf(d.Root).Elem().Name()]
}

func FindSchema(entry *yang.Entry, path *gnmipb.Path) *yang.Entry {
	for _, e := range path.GetElem() {
		entry = entry.Dir[e.GetName()]
		if entry == nil {
			return nil
		}
	}
	return entry
}

func (d *device) UpdateCreate(path string, value string) error {
	fmt.Println(":", path, value)
	gpath, err := xpath.ToGNMIPath(path)
	if err != nil {
		return err
	}
	if err := writeValue(d.RootSchema(), d.Root, gpath, value); err != nil {
		fmt.Println(path, ":::", err)
	}
	return nil
}

func (d *device) UpdateReplace(path string, value string) error {
	fmt.Println(":", path, value)
	gpath, err := xpath.ToGNMIPath(path)
	if err != nil {
		return err
	}
	if err := writeValue(d.RootSchema(), d.Root, gpath, value); err != nil {
		fmt.Println(path, ":::", err)
	}
	return nil
}

func (d *device) UpdateDelete(path string) error {
	gpath, err := xpath.ToGNMIPath(path)
	if err != nil {
		return err
	}
	return deleteValue(d.RootSchema(), d.Root, gpath)
}

func writeLeaf(schema *yang.Entry, base ygot.GoStruct, gpath *gnmipb.Path, t reflect.Type, v string) error {
	var typedValue *gnmipb.TypedValue
	if t.Kind() == reflect.Ptr {
		t = t.Elem()
	}
	vv, err := ytypes.StringToType(t, v)
	if err != nil {
		var yerr error
		vv, yerr = ydb.ValScalarNew(t, v)
		if yerr != nil {
			return err
		}
	}

	typedValue, err = ygot.EncodeTypedValue(vv.Interface(), gnmipb.Encoding_JSON_IETF)
	if err != nil {
		return err
	}
	err = ytypes.SetNode(schema, base, gpath, typedValue, &ytypes.InitMissingElements{})
	if err != nil {
		return fmt.Errorf("%s", status.FromError(err).Message)
	}
	return err
}

// call executes method of the gstruct (go struct) using childname
func callCreateDirectory(gstruct ygot.GoStruct, childname string, args ...interface{}) (interface{}, error) {
	inputs := make([]reflect.Value, len(args))
	for i := range args {
		inputs[i] = reflect.ValueOf(args[i])
	}
	reflect.ValueOf(gstruct).MethodByName(childname).Call(inputs)
	return nil, nil
}

func writeLeafList(schema *yang.Entry, base interface{}, gpath *gnmipb.Path, t reflect.Type, v string) error {
	sv, err := ydb.ValSliceNew(t)
	if err != nil {
		return err
	}
	vv, err := ytypes.StringToType(t.Elem(), v)
	if err != nil {
		var yerr error
		vv, yerr = ydb.ValScalarNew(t, v)
		if yerr != nil {
			return err
		}
	}
	rv, err := ydb.ValSliceAppend(sv, vv.Interface())
	if err != nil {
		return err
	}
	typedValue, err := ygot.EncodeTypedValue(rv.Interface(), gnmipb.Encoding_JSON_IETF)
	if err != nil {
		return err
	}

	err = ytypes.SetNode(schema, base, gpath, typedValue, &ytypes.InitMissingElements{})
	if err != nil {
		return fmt.Errorf("%s", status.FromError(err).Message)
	}
	return nil
}

// writeValue - Write the value to the model instance
func writeValue(schema *yang.Entry, base interface{}, gpath *gnmipb.Path, value string) error {
	var err error
	var curSchema *yang.Entry
	var curValue interface{}
	var p *gnmipb.Path
	curSchema = schema
	curValue = base
	for i := range gpath.GetElem() {
		switch {
		case curSchema.IsLeafList():
			return writeLeafList(schema, base, p, reflect.TypeOf(curValue), gpath.Elem[i].Name)
		case !curSchema.IsDir():
			return fmt.Errorf("invalid path: %s", xpath.ToXPath(gpath))
		}

		p = &gnmipb.Path{
			Elem: []*gnmipb.PathElem{
				gpath.Elem[i],
			},
		}
		curValue, curSchema, err = ytypes.GetOrCreateNode(curSchema, curValue, p)
		if err != nil {
			return fmt.Errorf("%s", status.FromError(err).Message)
		}
		switch {
		case curSchema.IsDir():
			schema = curSchema
			base = curValue
		case curSchema.IsLeafList():
			// do nothing
		default:
			return writeLeaf(schema, base.(ygot.GoStruct), p, reflect.TypeOf(curValue), value)
		}
	}
	return nil
}

// deleteValue - Delete the value from the model instance
func deleteValue(schema *yang.Entry, base ygot.GoStruct, gpath *gnmipb.Path) error {
	if err := ytypes.DeleteNode(schema, base, gpath); err != nil {
		return err
	}
	return nil
}

func main() {
	s, err := gostruct.Schema()
	if err != nil {
		panic(err)
	}
	gs := (*device)(s)

	db, close := ydb.OpenWithSync("running", gs)
	defer close()
	r, err := os.Open("../model/data/sample.yaml")
	defer r.Close()
	if err != nil {
		panic(err)
	}
	dec := db.NewDecoder(r)
	dec.Decode()
	gdump.PrintInDepth(5, gs.Root)
}
