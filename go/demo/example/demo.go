package main // import "github.com/neoul/libydb/go/demo/example"

import (
	"log"

	"github.com/neoul/libydb/go/ydb"
)

func main() {
	done := ydb.SetSignalFilter()
	// db, close := ydb.Open("hello")
	datastore := map[string]interface{}{}
	db, close := ydb.OpenWithTargetStruct("hello", &datastore)
	// db, close := ydb.OpenWithTargetStruct("hello", &ydb.EmptyGoStruct{})
	defer close()
	err := db.Connect("uss://test", "sub")
	if err != nil {
		log.Println(err)
	}
	db.Serve()
	<-done
	err = db.Disconnect("uss://test")
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
