package main // import "github.com/neoul/libydb/go/demo"

import (
	"log"
	"time"

	"github.com/neoul/libydb/go/ydb"
)

func main() {
	// db, close := ydb.OpenWithUpdater("hello", &ydb.DefaultStruct{})
	// db, close := ydb.OpenWithUpdater("hello", &ydb.EmptyGoStruct{})
	db, close := ydb.OpenWithAutoUpdater("hello", &ydb.EmptyGoStruct{})
	defer close()
	// ydb.SetLog(ydb.LogDebug)
	err := db.Connect("uss://test", "pub")
	if err != nil {
		log.Println(err)
	}
	db.Serve()
	
	for i := 0; i < 100; i++ {
		<-time.After(time.Second * 5)
		for num, err := range db.Errors {
			log.Println(num, err)
		}
		// node := db.Retrieve(ydb.RetrieveDepth(2))
		// log.Println(node)
		// log.Println(node.GetChildren())
		// for _, node := range node.GetChildren() {
		// 	log.Println(node)
		// 	for _, child := range node.GetChildren() {
		// 		log.Println(child)
		// 	}
		// }
	}
	
	

	err = db.Disconnect("uss://test")
	if err != nil {
		log.Println(err)
	}
}
