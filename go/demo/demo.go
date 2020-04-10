package main // import "demo"

import (
	"log"
	"time"

	"github.com/neoul/libydb/go/ydb"
)

func main() {
	db, close := ydb.Open("hello")
	defer close()
	// ydb.SetLog(ydb.LogDebug)
	err := db.Connect("uss://test", "pub")
	if err != nil {
		log.Println(err)
	}
	go db.Receive()
	<-time.After(time.Second * 1)
	node := db.Retrieve(ydb.RetrieveDepth(2))
	log.Println(node)
	log.Println(node.GetChildren())
	for _, node := range node.GetChildren() {
		log.Println(node)
		for _, child := range node.GetChildren() {
			log.Println(child)
		}
	}

	err = db.Disconnect("uss://test")
	if err != nil {
		log.Println(err)
	}
}
