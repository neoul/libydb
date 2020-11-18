// Clone an YDB instance
package main

import (
	"os"

	"github.com/neoul/gdump"
	"github.com/neoul/libydb/go/ydb"
)

func main() {
	dest, close := ydb.Open("dest")
	defer close()
	source, close := ydb.OpenWithSync("source", dest)
	defer close()
	r, err := os.Open("../../../examples/yaml/ydb-input.yaml")
	defer r.Close()
	if err != nil {
		panic(err)
	}
	dec := source.NewDecoder(r)
	dec.Decode()

	// Remove fan[1]
	source.DeleteFrom("/system/fan/fan[1]")
	a := map[string]interface{}{}
	dest.Convert(a)
	gdump.Print(a)
}
