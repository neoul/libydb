package main

import (
	"log"
	"time"
	"ydb"
)

func main() {
	log.Println("RUN")
	ydb.SetLog(ydb.LogInfo)
	// datablock, _ := ydb.Open("hello")
	datablock, close := ydb.Open("hello")
	defer close()
	err := datablock.Connect("uss://test", "pub")
	if err != nil {
		log.Println(err)
	}

	err = datablock.Disconnect("uss://test")
	if err != nil {
		log.Println(err)
	}
	<-time.After(time.Second * 1)
	datablock.Close()
	log.Println("END")
}
