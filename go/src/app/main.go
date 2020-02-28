package main

/*
#cgo LDFLAGS: -lydb -lyaml
#include <stdio.h>
#include <ylog.h>
#include <ydb.h>
extern int sum(int a, int b);

static inline void CExample() {
	int r = sum(1, 2);
	printf("%d\n", r);
}

extern void callbackInGo(void *p, int n);
static inline void callbackExample(void *p) {
	callbackInGo(p, 100);
}

*/
import "C"
import (
	"log"
	"unsafe"
	"ydb"
)

//export sum
func sum(a, b C.int) C.int {
	return a + b
}

//export callbackInGo
func callbackInGo(p unsafe.Pointer, n C.int) {
	f := *(*func(C.int))(p)
	f(n)
}

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

	datablock.Serve(1000)

	err = datablock.Disconnect("uss://test")
	if err != nil {
		log.Println(err)
	}

	// var datablock *C.ydb
	// name := C.CString("hello")
	// datablock = C.ydb_open(name)
	// C.CExample()
	// C.ydb_close(datablock)
	// f := func(n C.int) {
	// 	fmt.Println(n)
	// }
	// C.callbackExample(unsafe.Pointer(&f))
}
