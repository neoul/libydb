package main

import (
	"log"
	"time"

	"github.com/neoul/libydb/go/datablock"
)

func main() {
	data, close := datablock.Open("hello")
	defer close()
	err := data.Connect("uss://test", "pub")
	if err != nil {
		log.Println(err)
	}

	go data.Receive()
	<-time.After(time.Second * 3)

	node := data.Top()
	if node != nil {
		node = node.Down()
	}
	log.Println(node.Key())
	log.Println(node.Retrieve(datablock.RetrieveDepth(1)))
	log.Println(data.Retrieve(datablock.RetrieveKeys([]string{"c"}), datablock.RetrieveDepth(2)))

	err = data.Disconnect("uss://test")
	if err != nil {
		log.Println(err)
	}
}
