package main

import (
	"log"
	"time"

	"ydb"
)

func main() {
	log.Println("RUN")
	ydb.SetLog(ydb.LogDebug)
	datablock, close := ydb.Open("hello")
	defer close()
	// datablock.Close()
	err := datablock.Connect("uss://test", "pub")
	if err != nil {
		log.Println(err)
	}

	go datablock.Serve()
	<-time.After(time.Second * 3)

	err = datablock.Disconnect("uss://test")
	if err != nil {
		log.Println(err)
	}
}
