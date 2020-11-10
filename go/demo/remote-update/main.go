package main

import (
	"flag"
	"log"

	"github.com/neoul/libydb/go/ydb"
	"github.com/sirupsen/logrus"
)

// An example receiving updates from remote process.

var (
	address = flag.String("address", "uss://test", "Address to connect")
)

func main() {
	flag.Parse()
	done := ydb.SetSignalFilter()
	ydb.SetLogLevel(logrus.DebugLevel)
	datastore := map[string]interface{}{}
	db, close := ydb.OpenWithSync("host", &datastore)
	defer close()
	err := db.Connect(*address, "pub")
	if err != nil {
		log.Println(err)
	}
	db.Serve()
	<-done
	err = db.Disconnect(*address)
	if err != nil {
		log.Println(err)
	}
	for num, err := range db.Errors {
		log.Println(num, err)
	}
	// for i := 0; i < 100; i++ {
	// 	<-time.After(time.Second * 5)
	// 	// node := db.ConvertToYNode(ydb.ConvertedDepth(2))
	// 	// log.Println(node)
	// 	// log.Println(node.GetChildren())
	// 	// for _, node := range node.GetChildren() {
	// 	// 	log.Println(node)
	// 	// 	for _, child := range node.GetChildren() {
	// 	// 		log.Println(child)
	// 	// 	}
	// 	// }
	// }
}
