package main

import (
	"fmt"

	"github.com/kr/pretty"
	"github.com/neoul/libydb/go/ydb"
)

func main() {
	// Create a user-defined data structure
	userdb := map[string]interface{}{}

	// Enable log
	// ydb.SetLogLevel(logrus.DebugLevel)
	// ydb.SetInternalLog(ydb.LogDebug)

	// Open an YDB instance
	db, close := ydb.Open("hello")
	defer close()

	// Write data to the YDB instance
	db.WriteTo("/system/cpu", "pentium")

	// Write YAML data to the YDB instance
	db.Write([]byte(`
system:
  motherboard: Asus XXX
  memory: 16GB
  input:
    keyboard: Logitech
    mouse: ab
  power: 750W
`))
	// Read all data to the user-defined data structure.
	db.Convert(ydb.RetrieveAll(), ydb.RetrieveStruct(userdb))
	pretty.Println(userdb)

	// Read the data from the leaf data. (Branch node doesn't have data.)
	r := db.ReadFrom("/system/cpu")
	fmt.Println(r)
}
