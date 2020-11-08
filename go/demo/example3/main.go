package main

import (
	"flag"
	"log"

	"github.com/neoul/libydb/go/ydb"
)

var (
	address = flag.String("address", "uss://test", "Address to connect")
)

func main() {
	flag.Parse()
	done := ydb.SetSignalFilter()
	// db, close := ydb.Open("hello")
	datastore := map[string]interface{}{}
	db, close := ydb.OpenWithTargetStruct("hello", &datastore)
	// db, close := ydb.OpenWithTargetStruct("hello", &ydb.EmptyGoStruct{})
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
	// 	// node := db.Retrieve(ydb.RetrieveDepth(2))
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
