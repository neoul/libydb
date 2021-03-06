package main

import (
	"fmt"
	"os"

	"github.com/neoul/libydb/go/ydb"
)

// An example for synchronized data update

func main() {

	datastore := map[string]interface{}{}
	db, close := ydb.OpenWithSync("hello", &datastore)
	defer close()
	r, err := os.Open("../../../examples/yaml/ydb-input.yaml")
	defer r.Close()
	if err != nil {
		panic(err)
	}
	dec := db.NewDecoder(r)
	dec.Decode()

	fmt.Println(datastore)
}
