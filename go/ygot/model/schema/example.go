/*
Package schema is a generated package which contains definitions
of structs which represent a YANG schema. The generated schema can be
compressed by a series of transformations (compression was false
in this case).

This package was generated by /home/neoul/go/src/github.com/openconfig/ygot/genutil/names.go
using the following YANG input files:
	- yang/example.yang
Imported modules were sourced from:
	- yang/...
*/
package schema

import (
	"encoding/json"
	"fmt"
	"reflect"

	"github.com/openconfig/ygot/ygot"
	"github.com/openconfig/goyang/pkg/yang"
	"github.com/openconfig/ygot/ytypes"
)

// Binary is a type that is used for fields that have a YANG type of
// binary. It is used such that binary fields can be distinguished from
// leaf-lists of uint8s (which are mapped to []uint8, equivalent to
// []byte in reflection).
type Binary []byte

// YANGEmpty is a type that is used for fields that have a YANG type of
// empty. It is used such that empty fields can be distinguished from boolean fields
// in the generated code.
type YANGEmpty bool

var (
	SchemaTree map[string]*yang.Entry
)

func init() {
	var err error
	if SchemaTree, err = UnzipSchema(); err != nil {
		panic("schema error: " +  err.Error())
	}
}

// Schema returns the details of the generated schema.
func Schema() (*ytypes.Schema, error) {
	uzp, err := UnzipSchema()
	if err != nil {
		return nil, fmt.Errorf("cannot unzip schema, %v", err)
	}

	return &ytypes.Schema{
		Root: &Example{},
		SchemaTree: uzp,
		Unmarshal: Unmarshal,
	}, nil
}

// UnzipSchema unzips the zipped schema and returns a map of yang.Entry nodes,
// keyed by the name of the struct that the yang.Entry describes the schema for.
func UnzipSchema() (map[string]*yang.Entry, error) {
	var schemaTree map[string]*yang.Entry
	var err error
	if schemaTree, err = ygot.GzipToSchema(ySchema); err != nil {
		return nil, fmt.Errorf("could not unzip the schema; %v", err)
	}
	return schemaTree, nil
}

// Unmarshal unmarshals data, which must be RFC7951 JSON format, into
// destStruct, which must be non-nil and the correct GoStruct type. It returns
// an error if the destStruct is not found in the schema or the data cannot be
// unmarshaled. The supplied options (opts) are used to control the behaviour
// of the unmarshal function - for example, determining whether errors are
// thrown for unknown fields in the input JSON.
func Unmarshal(data []byte, destStruct ygot.GoStruct, opts ...ytypes.UnmarshalOpt) error {
	tn := reflect.TypeOf(destStruct).Elem().Name()
	schema, ok := SchemaTree[tn]
	if !ok {
		return fmt.Errorf("could not find schema for type %s", tn )
	}
	var jsonTree interface{}
	if err := json.Unmarshal([]byte(data), &jsonTree); err != nil {
		return err
	}
	return ytypes.Unmarshal(schema, destStruct, jsonTree, opts...)
}

// Example represents the /example YANG schema element.
type Example struct {
	Company	*Network_Company	`path:"company" module:"network"`
	Country	map[string]*Network_Country	`path:"country" module:"network"`
	Married	YANGEmpty	`path:"married" module:"network"`
	Operator	map[uint32]*Network_Operator	`path:"operator" module:"network"`
	Person	*string	`path:"person" module:"network"`
}

// IsYANGGoStruct ensures that Example implements the yang.GoStruct
// interface. This allows functions that need to handle this struct to
// identify it as being generated by ygen.
func (*Example) IsYANGGoStruct() {}

// NewCountry creates a new entry in the Country list of the
// Example struct. The keys of the list are populated from the input
// arguments.
func (t *Example) NewCountry(Name string) (*Network_Country, error){

	// Initialise the list within the receiver struct if it has not already been
	// created.
	if t.Country == nil {
		t.Country = make(map[string]*Network_Country)
	}

	key := Name

	// Ensure that this key has not already been used in the
	// list. Keyed YANG lists do not allow duplicate keys to
	// be created.
	if _, ok := t.Country[key]; ok {
		return nil, fmt.Errorf("duplicate key %v for list Country", key)
	}

	t.Country[key] = &Network_Country{
		Name: &Name,
	}

	return t.Country[key], nil
}

// NewOperator creates a new entry in the Operator list of the
// Example struct. The keys of the list are populated from the input
// arguments.
func (t *Example) NewOperator(Asn uint32) (*Network_Operator, error){

	// Initialise the list within the receiver struct if it has not already been
	// created.
	if t.Operator == nil {
		t.Operator = make(map[uint32]*Network_Operator)
	}

	key := Asn

	// Ensure that this key has not already been used in the
	// list. Keyed YANG lists do not allow duplicate keys to
	// be created.
	if _, ok := t.Operator[key]; ok {
		return nil, fmt.Errorf("duplicate key %v for list Operator", key)
	}

	t.Operator[key] = &Network_Operator{
		Asn: &Asn,
	}

	return t.Operator[key], nil
}

// Validate validates s against the YANG schema corresponding to its type.
func (t *Example) Validate(opts ...ygot.ValidationOption) error {
	if err := ytypes.Validate(SchemaTree["Example"], t, opts...); err != nil {
		return err
	}
	return nil
}

// ΛEnumTypeMap returns a map, keyed by YANG schema path, of the enumerated types
// that are included in the generated code.
func (t *Example) ΛEnumTypeMap() map[string][]reflect.Type { return ΛEnumTypes }


// Network_Company represents the /network/company YANG schema element.
type Network_Company struct {
	Address	[]string	`path:"address" module:"network"`
	Name	*string	`path:"name" module:"network"`
}

// IsYANGGoStruct ensures that Network_Company implements the yang.GoStruct
// interface. This allows functions that need to handle this struct to
// identify it as being generated by ygen.
func (*Network_Company) IsYANGGoStruct() {}

// Validate validates s against the YANG schema corresponding to its type.
func (t *Network_Company) Validate(opts ...ygot.ValidationOption) error {
	if err := ytypes.Validate(SchemaTree["Network_Company"], t, opts...); err != nil {
		return err
	}
	return nil
}

// ΛEnumTypeMap returns a map, keyed by YANG schema path, of the enumerated types
// that are included in the generated code.
func (t *Network_Company) ΛEnumTypeMap() map[string][]reflect.Type { return ΛEnumTypes }


// Network_Country represents the /network/country YANG schema element.
type Network_Country struct {
	CountryCode	*string	`path:"country-code" module:"network"`
	DialCode	*uint32	`path:"dial-code" module:"network"`
	Name	*string	`path:"name" module:"network"`
}

// IsYANGGoStruct ensures that Network_Country implements the yang.GoStruct
// interface. This allows functions that need to handle this struct to
// identify it as being generated by ygen.
func (*Network_Country) IsYANGGoStruct() {}

// ΛListKeyMap returns the keys of the Network_Country struct, which is a YANG list entry.
func (t *Network_Country) ΛListKeyMap() (map[string]interface{}, error) {
	if t.Name == nil {
		return nil, fmt.Errorf("nil value for key Name")
	}

	return map[string]interface{}{
		"name": *t.Name,
	}, nil
}

// Validate validates s against the YANG schema corresponding to its type.
func (t *Network_Country) Validate(opts ...ygot.ValidationOption) error {
	if err := ytypes.Validate(SchemaTree["Network_Country"], t, opts...); err != nil {
		return err
	}
	return nil
}

// ΛEnumTypeMap returns a map, keyed by YANG schema path, of the enumerated types
// that are included in the generated code.
func (t *Network_Country) ΛEnumTypeMap() map[string][]reflect.Type { return ΛEnumTypes }


// Network_Operator represents the /network/operator YANG schema element.
type Network_Operator struct {
	Asn	*uint32	`path:"asn" module:"network"`
	Name	*string	`path:"name" module:"network"`
}

// IsYANGGoStruct ensures that Network_Operator implements the yang.GoStruct
// interface. This allows functions that need to handle this struct to
// identify it as being generated by ygen.
func (*Network_Operator) IsYANGGoStruct() {}

// ΛListKeyMap returns the keys of the Network_Operator struct, which is a YANG list entry.
func (t *Network_Operator) ΛListKeyMap() (map[string]interface{}, error) {
	if t.Asn == nil {
		return nil, fmt.Errorf("nil value for key Asn")
	}

	return map[string]interface{}{
		"asn": *t.Asn,
	}, nil
}

// Validate validates s against the YANG schema corresponding to its type.
func (t *Network_Operator) Validate(opts ...ygot.ValidationOption) error {
	if err := ytypes.Validate(SchemaTree["Network_Operator"], t, opts...); err != nil {
		return err
	}
	return nil
}

// ΛEnumTypeMap returns a map, keyed by YANG schema path, of the enumerated types
// that are included in the generated code.
func (t *Network_Operator) ΛEnumTypeMap() map[string][]reflect.Type { return ΛEnumTypes }



var (
	// ySchema is a byte slice contain a gzip compressed representation of the
	// YANG schema from which the Go code was generated. When uncompressed the
	// contents of the byte slice is a JSON document containing an object, keyed
	// on the name of the generated struct, and containing the JSON marshalled
	// contents of a goyang yang.Entry struct, which defines the schema for the
	// fields within the struct.
	ySchema = []byte{
		0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0xec, 0x5a, 0xcb, 0x6e, 0xdb, 0x3a,
		0x10, 0xdd, 0xeb, 0x2b, 0x04, 0xae, 0x7d, 0x11, 0x5f, 0x27, 0x6d, 0x6a, 0xef, 0xdc, 0x3c, 0x50,
		0x20, 0x4d, 0x52, 0xa4, 0x45, 0x37, 0x45, 0x51, 0x10, 0xd2, 0xc4, 0x21, 0x22, 0x91, 0xc2, 0x90,
		0x42, 0x6c, 0x14, 0xfe, 0xf7, 0x42, 0xa6, 0xa4, 0x58, 0x16, 0x49, 0xd3, 0xad, 0x83, 0x3e, 0xcc,
		0x65, 0x38, 0x87, 0x9e, 0xc7, 0x19, 0x72, 0x0e, 0x85, 0x7c, 0x8f, 0xe2, 0x38, 0x8e, 0xc9, 0x0d,
		0xcd, 0x81, 0x4c, 0x62, 0x02, 0x73, 0x9a, 0x17, 0x19, 0x90, 0x81, 0x5e, 0xbe, 0x62, 0x3c, 0x25,
		0x93, 0xf8, 0xff, 0xfa, 0xcf, 0x33, 0xc1, 0xef, 0xd9, 0x8c, 0x4c, 0xe2, 0x61, 0xbd, 0x70, 0xce,
		0x90, 0x4c, 0x62, 0xfd, 0x1b, 0xab, 0x85, 0x44, 0xe4, 0x05, 0xe5, 0x8b, 0xce, 0x62, 0xc7, 0x41,
		0x03, 0x18, 0x74, 0xcd, 0x5d, 0x47, 0xed, 0xf2, 0xa6, 0xc3, 0xd6, 0xf0, 0x01, 0xe1, 0x9e, 0xcd,
		0x7b, 0x6e, 0x3a, 0xae, 0x38, 0xa8, 0x0d, 0x37, 0x2b, 0xf3, 0x47, 0x51, 0x62, 0x02, 0xc6, 0xad,
		0x3a, 0x14, 0x58, 0x3c, 0x09, 0xac, 0xa2, 0x21, 0x85, 0xf6, 0x32, 0x30, 0x03, 0xdf, 0x51, 0x39,
		0xc5, 0x59, 0x99, 0x03, 0x57, 0x64, 0x12, 0x2b, 0x2c, 0xc1, 0x02, 0x5c, 0x43, 0xad, 0x82, 0xea,
		0xa1, 0x96, 0x9d, 0x95, 0xe5, 0x46, 0xae, 0x9b, 0x45, 0x6e, 0x0d, 0x34, 0x4d, 0x11, 0xa4, 0xb4,
		0xa7, 0xd2, 0x54, 0xa2, 0x01, 0x5a, 0xe2, 0xab, 0x8b, 0x3f, 0xb4, 0x98, 0x6d, 0x24, 0xf8, 0x90,
		0xe1, 0x49, 0x8a, 0x2f, 0x39, 0x3b, 0x93, 0xb4, 0x33, 0x59, 0xfe, 0xa4, 0x99, 0xc9, 0xb3, 0x90,
		0xd8, 0xfe, 0xec, 0xa7, 0x45, 0x01, 0x7e, 0x95, 0x92, 0x0a, 0x19, 0x9f, 0xb9, 0x8a, 0xd5, 0x1c,
		0x9a, 0x37, 0x3b, 0x45, 0xf0, 0x9e, 0x49, 0x35, 0x55, 0x0a, 0xdd, 0x51, 0x5c, 0x33, 0x7e, 0x91,
		0x41, 0x55, 0x80, 0xaa, 0xbf, 0x78, 0x99, 0x65, 0x8e, 0x40, 0xae, 0xe9, 0xdc, 0x1f, 0x7c, 0x8b,
		0x29, 0x20, 0xa4, 0x6f, 0x17, 0x35, 0x34, 0xf2, 0x2b, 0xaa, 0x21, 0x1d, 0xc2, 0x75, 0xa9, 0xb6,
		0x74, 0xff, 0x0a, 0x15, 0x5a, 0xff, 0x90, 0x5a, 0x7f, 0xc7, 0x1b, 0x76, 0xca, 0xb9, 0x50, 0x54,
		0x31, 0xc1, 0xcd, 0x17, 0xad, 0x4c, 0x1e, 0x20, 0xa7, 0x05, 0x55, 0x0f, 0x55, 0x74, 0x47, 0x1c,
		0xd4, 0x93, 0xc0, 0xc7, 0x23, 0xf3, 0x2c, 0xd3, 0x3b, 0x14, 0x96, 0x89, 0xaa, 0xfb, 0x93, 0xdc,
		0xe8, 0x0d, 0xdf, 0xce, 0xea, 0x0d, 0x91, 0x39, 0xb4, 0xb5, 0xb0, 0x48, 0x22, 0x4a, 0xae, 0xd0,
		0x39, 0x48, 0x35, 0x20, 0x0c, 0xd2, 0x5f, 0x1f, 0xa4, 0x75, 0x2d, 0xff, 0x4b, 0x44, 0xea, 0x71,
		0x9f, 0x74, 0xd0, 0xe1, 0x5e, 0x39, 0xec, 0x7b, 0xc5, 0x70, 0x2e, 0x52, 0x46, 0x33, 0xcf, 0x56,
		0x7a, 0x86, 0x86, 0x3e, 0xfa, 0x8b, 0xfa, 0xa8, 0x64, 0x5c, 0x1d, 0x8f, 0x3c, 0xfa, 0xe8, 0xd4,
		0x01, 0xb9, 0xa3, 0x7c, 0x56, 0xfd, 0xda, 0x17, 0x67, 0xb2, 0xee, 0x62, 0x37, 0x52, 0x6d, 0x2b,
		0x2b, 0x9e, 0x0d, 0xd5, 0x83, 0x7f, 0xa6, 0x59, 0x09, 0xfd, 0x79, 0x62, 0xc5, 0x5f, 0x22, 0x4d,
		0xaa, 0x39, 0x7a, 0xce, 0x66, 0x6c, 0x25, 0x06, 0x87, 0x5b, 0xf7, 0x2d, 0x07, 0x1e, 0x29, 0xd2,
		0xf9, 0x8b, 0xa7, 0x78, 0x32, 0x1e, 0xbd, 0x60, 0x92, 0xd1, 0xcf, 0x59, 0xbf, 0x06, 0x75, 0x1c,
		0x6e, 0x9f, 0xdf, 0xac, 0x8e, 0xaf, 0x60, 0x61, 0x69, 0x14, 0xf7, 0x5b, 0xd2, 0xeb, 0x0d, 0xe9,
		0xf5, 0x76, 0x74, 0xbf, 0x19, 0xf7, 0xa6, 0xe6, 0x4d, 0x82, 0xda, 0xa9, 0xe6, 0xf5, 0x06, 0x0f,
		0x35, 0x9f, 0x53, 0x44, 0x06, 0xa9, 0x5d, 0xcd, 0x37, 0x00, 0xb3, 0x9a, 0x1f, 0x06, 0x35, 0xdf,
		0xa7, 0xd9, 0x7a, 0x60, 0x9e, 0x3f, 0x66, 0xe6, 0x85, 0x32, 0xf2, 0xd9, 0x9c, 0x8f, 0x63, 0x0f,
		0xea, 0x44, 0x01, 0x48, 0x95, 0x40, 0x3b, 0x77, 0x2d, 0x22, 0x3c, 0xc5, 0xf6, 0xf0, 0x4d, 0x53,
		0x72, 0x8f, 0xef, 0x99, 0x92, 0x87, 0x91, 0x15, 0x04, 0xf3, 0x9f, 0x2c, 0x98, 0x87, 0x07, 0x20,
		0x98, 0x47, 0xe3, 0x93, 0xf1, 0xeb, 0xd3, 0xd1, 0xf8, 0x55, 0xd0, 0xcd, 0xe1, 0x12, 0x0a, 0xba,
		0xd9, 0xa2, 0x9b, 0xfb, 0xc3, 0xea, 0x9f, 0x92, 0xcd, 0x16, 0xf5, 0xe3, 0xd0, 0xcd, 0xb7, 0xcd,
		0x0e, 0x0f, 0xf5, 0x55, 0x00, 0x4a, 0x43, 0x40, 0x2d, 0xa7, 0xb5, 0x3d, 0xc8, 0xe6, 0x3d, 0xca,
		0x66, 0xeb, 0x39, 0xb1, 0x9c, 0x8f, 0x35, 0xe6, 0xa2, 0x35, 0x8f, 0xb6, 0x86, 0x22, 0x4c, 0x5e,
		0xd2, 0x47, 0xb8, 0x13, 0xa2, 0x9f, 0xe8, 0x66, 0x93, 0x91, 0x75, 0x53, 0xa7, 0x99, 0x2e, 0xea,
		0x7f, 0x54, 0xd0, 0x1e, 0xa3, 0xe5, 0x0f, 0x00, 0x00, 0x00, 0xff, 0xff, 0x01, 0x00, 0x00, 0xff,
		0xff, 0xa9, 0x7d, 0xa1, 0xfc, 0xc8, 0x20, 0x00, 0x00,
	}
)


// ΛEnumTypes is a map, keyed by a YANG schema path, of the enumerated types that
// correspond with the leaf. The type is represented as a reflect.Type. The naming
// of the map ensures that there are no clashes with valid YANG identifiers.
var ΛEnumTypes = map[string][]reflect.Type{
}

