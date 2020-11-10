package main

import (
	"github.com/kr/pretty"
	"github.com/neoul/libydb/go/ydb"
)

// Default Updater interface example

type userData struct {
	System struct {
		Cpu         string `path:"cpu"`
		Motherboard string
		Memory      int `unit:"GB"`
		Power       string
		InputDevice map[string]interface{} `path:"input"`
	} `path:"system"`
}

func main() {
	// Create a user-defined data structure
	userdb := &userData{}

	// Enable log
	// ydb.SetLogLevel(logrus.DebugLevel)
	// ydb.SetInternalLog(ydb.LogDebug)

	// Open an YDB instance
	db, close := ydb.Open("hello")
	defer close()

	// Write YAML data to the YDB instance
	db.Write([]byte(`
system:
  cpu: Pantium
  motherboard: Asus XXX
  memory: 16
  input:
    keyboard: Logitech
    mouse: ab
  power: 750W
`))
	// Read all data to the user-defined data structure.
	db.Convert(userdb)
	pretty.Println(userdb)
}
