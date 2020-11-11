package main

import (
	"fmt"
	"os"

	"github.com/neoul/gdump"
	"github.com/neoul/libydb/go/demo/ygot/model/gostruct"
	"github.com/neoul/libydb/go/ydb"
)

// ygot/example1 is an example to update ygot struct using the default YDB Updater interface.
// ygot/example2 is an example to update ygot struct using the user-defined YDB Updater interface.

func main() {
	gs := gostruct.Device{}
	db, close := ydb.OpenWithSync("running", &gs)
	defer close()
	r, err := os.Open("../model/data/example.yaml")
	defer r.Close()
	if err != nil {
		panic(err)
	}
	dec := db.NewDecoder(r)
	dec.Decode()
	gdump.Print(gs)
	fmt.Println("")
	fmt.Println(*gs.Country["United Kingdom"].Name, *gs.Country["United Kingdom"].CountryCode, *gs.Country["United Kingdom"].DialCode)
	fmt.Println(*&gs.Company.Address, gs.Company.Enumval)
	fmt.Println("")
}
