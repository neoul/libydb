package main

import (
	"github.com/neoul/gostruct-dump/dump"
	"github.com/neoul/libydb/go/ydb"
	"github.com/sirupsen/logrus"
)

type userData struct {
	System struct {
		Cpu         string `path:"cpu"`
		Motherboard string
		Memory      int
		Power       string
	} `path:"system"`
}

func main() {
	// Create a user-defined data structure
	userdb := &userData{}
	// json.Marshal(jsonTree)
	// json.Unmarshal

	// Enable log
	ydb.SetLogLevel(logrus.DebugLevel)
	// ydb.SetInternalLog(ydb.LogDebug)

	// Open an YDB instance
	db, close := ydb.Open("hello")
	defer close()

	// Write YAML data to the YDB instance
	db.Write([]byte(`
system:
  cpu: pentium
  Motherboard: Asus XXX
  memory: 16
  Power: 750W
`))
	// Read all data to the user-defined data structure.
	db.Convert(ydb.RetrieveAll(), ydb.RetrieveStruct(userdb))
	// db.Unmarshal(userdb, Option)
	dump.Print(userdb)
}
