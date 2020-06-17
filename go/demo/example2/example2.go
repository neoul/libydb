package main // import "github.com/neoul/libydb/go/demo/example2"

import (
	"fmt"
	"os"

	"github.com/neoul/libydb/go/ydb"
)

func main() {

	datastore := map[string]interface{}{}
	db, close := ydb.OpenWithTargetStruct("hello", &datastore)
	defer close()
	r, err := os.Open("../../../examples/yaml/ydb-input.yaml")
	defer r.Close()
	if err != nil {
		panic(err)
	}
	dec := db.NewDecoder(r)
	dec.Decode()

	fmt.Println(datastore)
	ydb.DebugValueString(datastore, 4, func(n ...interface{}) { fmt.Print(n) })
}
