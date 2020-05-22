package main

import (
	"os"

	"github.com/neoul/libydb/go/ydb"
)


func main() {
	db, close := ydb.Open("mydb")
	defer close()
	// ydb.SetLog(ydb.LogDebug)

	r, err := os.Open("../../../examples/yaml/yaml-demo.yaml")
	defer r.Close()
	if err != nil {
		panic(err)
	}
	dec := db.NewDecoder(r)
	dec.Decode()

	w, err := os.Create("yaml-demo.yaml")
	defer w.Close()
	if err != nil {
		panic(err)
	}
	enc := db.NewEncoder(w)
	enc.Encode()
}
