package main

import (
	"fmt"
	"os"

	"github.com/neoul/libydb/go/ydb"
)

func main() {
	s := map[string]interface{}{}
	db, close := ydb.OpenWithSync("mydb", &s)
	defer close()

	r, err := os.Open("../../../examples/yaml/yaml-value.yaml")
	defer r.Close()
	if err != nil {
		panic(err)
	}
	dec := db.NewDecoder(r)
	dec.Decode()

	fmt.Println(db.Errors)
	fmt.Println(s)

	w, err := os.Create("result.yaml")
	defer w.Close()
	if err != nil {
		panic(err)
	}
	enc := db.NewEncoder(w)
	enc.Encode()
}
