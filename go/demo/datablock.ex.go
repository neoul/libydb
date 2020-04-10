package main

import (
	"log"
	"time"

	"github.com/neoul/libydb/go/datablock"
)

func main() {
	db, close := datablock.Open("hello")
	defer close()
	// datablock.SetLog(datablock.LogDebug)
	err := db.Connect("uss://test", "pub")
	if err != nil {
		log.Println(err)
	}
	go db.Receive()
	<-time.After(time.Second * 1)
	node := db.Retrieve(datablock.RetrieveDepth(2))
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
