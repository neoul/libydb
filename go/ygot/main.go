package main // import "github.com/neoul/libydb/go/ygot"

import (
	"fmt"
	"os"

	"github.com/neoul/libydb/go/ydb"
	"github.com/neoul/libydb/go/ygot/model/gostruct"
	"github.com/sirupsen/logrus"
)

var (
	ylog *logrus.Entry
)

func init() {
	ylog = ydb.NewLogEntry("ydb2ygot")
	// fmt.Println(gostruct.SchemaTree)
	// for key, entry := range gostruct.SchemaTree {
	// 	fmt.Println("key:", key)
	// 	ydb.DebugValueString(entry, 2, func(x ...interface{}) { fmt.Print(x...) })
	// 	fmt.Println("")
	// }
}

func main() {

	// db, close := ydb.Open("mydb")
	// defer close()
	// r, err := os.Open("model/data/example.yaml")
	// defer r.Close()
	// if err != nil {
	// 	ylog.Fatal(err)
	// }
	// dec := db.NewDecoder(r)
	// dec.Decode()

	// var user interface{}
	// user, err = db.Convert(ydb.RetrieveAll())
	// ylog.Debug(user)

	// var user map[string]interface{}
	// user = map[string]interface{}{}
	// _, err = db.Convert(ydb.RetrieveAll(), ydb.RetrieveStruct(user))
	// ylog.Debug(user)

	// gs := gostruct.Device{}
	// _, err = db.Convert(ydb.RetrieveAll(), ydb.RetrieveStruct(&gs))
	// ylog.Debug(gs)
	// fmt.Println(*gs.Country["United Kingdom"].Name, *gs.Country["United Kingdom"].CountryCode, *gs.Country["United Kingdom"].DialCode)
	// fmt.Println(*&gs.Company.Address, gs.Company.Enumval)

	// gs := gostruct.Device{}
	// _, err = db.Convert(ydb.RetrieveAll(), ydb.RetrieveStruct(&gs))
	// ylog.Debug(gs)
	// fmt.Println(*gs.Country["United Kingdom"].Name, *gs.Country["United Kingdom"].CountryCode, *gs.Country["United Kingdom"].DialCode)
	// fmt.Println(*&gs.Company.Address, gs.Company.Enumval)

	ydb.InitChildenOnSet = false
	gs := gostruct.Device{}
	db, close := ydb.OpenWithTargetStruct("running", &gs)
	defer close()
	r, err := os.Open("model/data/example.yaml")
	defer r.Close()
	if err != nil {
		ylog.Fatal(err)
	}
	dec := db.NewDecoder(r)
	dec.Decode()
	ydb.DebugValueString(gs, 5, func(x ...interface{}) { fmt.Print(x...) })
	fmt.Println("")
	fmt.Println(*gs.Country["United Kingdom"].Name, *gs.Country["United Kingdom"].CountryCode, *gs.Country["United Kingdom"].DialCode)
	fmt.Println(*&gs.Company.Address, gs.Company.Enumval)
	fmt.Println("")
}
