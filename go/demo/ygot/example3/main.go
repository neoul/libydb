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
	err := writeValue(d.RootSchema(), d.Root, path, value)
	if err != nil {
		fmt.Println(path, ":::", err)
	}
	return err
}

func (d *device) UpdateReplace(path string, value string) error {
	err := writeValue(d.RootSchema(), d.Root, path, value)
	if err != nil {
		fmt.Println(path, ":::", err)
	}
	return err
}

func (d *device) UpdateDelete(path string) error {
	return deleteValue(d.RootSchema(), d.Root, path)
}

func writeScalarValue(schema *yang.Entry, base ygot.GoStruct, gpath *gnmipb.Path, value string) error {
	target, tSchema, err := ytypes.GetOrCreateNode(schema, base, gpath)
	if err != nil {
		fmt.Println("XXXX ", err)
		return fmt.Errorf("%s", status.FromError(err).Message)
	}

	var typedValue *gnmipb.TypedValue
	vt := reflect.TypeOf(target)
	if vt.Kind() == reflect.Ptr {
		vt = vt.Elem()
	}
	vv, err := ytypes.StringToType(vt, value)
	if err != nil {
		switch tSchema.Type.Kind {
		case yang.Yenum, yang.Yidentityref:
			return err
		default:
			var yerr error
			vv, yerr = ydb.ValScalarNew(vt, value)
			if yerr != nil {
				return err
			}
		}
	}

	switch tSchema.Type.Kind {
	case yang.Yempty:

	}

	typedValue, err = ygot.EncodeTypedValue(vv.Interface(), gnmipb.Encoding_JSON_IETF)
	if err != nil {
		return err
	}
	// switch tSchema.Type.Kind {
	// case yang.Yempty:
	// 	typedValue = &gnmipb.TypedValue{
	// 		Value: &gnmipb.,
	// 	}
	// }
	fmt.Println(typedValue)

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

func writeListValue(schema *yang.Entry, base interface{}, gpath *gnmipb.Path, t reflect.Type, v string) error {
	gdump.PrintInDepth(2, base, v)
	vv, err := ytypes.StringToType(t.Elem(), v)
	sv, err := ydb.ValSliceNew(t)
	rv, err := ydb.ValSliceAppend(sv, vv.Interface())
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

func writeDirectoryValue(schema *yang.Entry, base ygot.GoStruct, gpath *gnmipb.Path) error {
	// callCreateDirectory(schema, base, fmt.Sprint("New%s", base.))
	_, _, err := ytypes.GetOrCreateNode(schema, base, gpath)
	if err != nil {
		return fmt.Errorf("%s", status.FromError(err).Message)
	}
	return nil
}

// writeValue - Write the value to the model instance
func writeValue(schema *yang.Entry, base interface{}, path string, value string) error {
	var err error
	var gpath *gnmipb.Path
	gpath, err = xpath.ToGNMIPath(path)
	if err != nil {
		return err
	}
	var lpath *gnmipb.Path
	n := len(gpath.GetElem())
	if n == 0 {
		return fmt.Errorf("empty path inserted for write")
	}
	lpath = &gnmipb.Path{
		Elem: []*gnmipb.PathElem{
			gpath.Elem[n-1],
		},
	}
	// pschema := schema
	// parent := base

	// for i := range gpath.GetElem() {
	// 	pschema = schema
	// 	parent = base
	// 	p := &gnmipb.Path{
	// 		Elem: []*gnmipb.PathElem{
	// 			gpath.Elem[i],
	// 		},
	// 	}
	// 	base, schema, err = ytypes.GetOrCreateNode(schema, base, p)
	// 	if err != nil {
	// 		return fmt.Errorf("%s", status.FromError(err).Message)
	// 	}
	// }
	gpath.Elem = gpath.Elem[:n-1]
	branch, bschema, err := ytypes.GetOrCreateNode(schema, base, gpath)
	if err != nil {
		return fmt.Errorf("%s", status.FromError(err).Message)
	}
	if bschema.IsLeafList() {
		fmt.Println("*Branch leaf-list", lpath)
		return writeListValue(schema, base, gpath, reflect.TypeOf(branch), lpath.Elem[0].Name)
	}
	tschema := FindSchema(bschema, lpath)
	if tschema == nil {
		return fmt.Errorf("schema not found for %v", path)
	}
	if tschema.IsDir() {
		return writeDirectoryValue(bschema, branch.(ygot.GoStruct), lpath)
	}
	if tschema.IsLeafList() {
		return nil
	}
	return writeScalarValue(bschema, branch.(ygot.GoStruct), lpath, value)
}

// deleteValue - Delete the value from the model instance
func deleteValue(schema *yang.Entry, base ygot.GoStruct, path string) error {
	gpath, err := xpath.ToGNMIPath(path)
	if err != nil {
		return err
	}
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
