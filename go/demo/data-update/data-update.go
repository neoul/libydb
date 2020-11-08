package main

// DataUpdate interface Example

import (
	"os"

	"github.com/kr/pretty"
	"github.com/neoul/libydb/go/ydb"
	"github.com/neoul/trie"
)

// DataUpdate interface example

type userdata struct {
	*trie.Trie
}

func newTrie() *userdata {
	return &userdata{trie.New()}
}

func (u *userdata) UpdateCreate(path string, value string) error {
	u.Add(path, value)
	return nil
}

func (u *userdata) UpdateReplace(path string, value string) error {
	u.Add(path, value)
	return nil
}

func (u *userdata) UpdateDelete(path string) error {
	keys := u.PrefixSearch(path)
	for _, p := range keys {
		u.Remove(p)
	}
	return nil
}

func main() {
	// ydb.SetInternalLog(ydb.LogDebug)
	ud := newTrie()
	db, close := ydb.OpenWithTargetStruct("test", ud)
	defer close()
	r, err := os.Open("../../../examples/yaml/ydb-input.yaml")
	defer r.Close()
	if err != nil {
		panic(err)
	}
	dec := db.NewDecoder(r)
	dec.Decode()
	// pretty.Println(ud)
	for i, k := range ud.Keys() {
		n, ok := ud.Find(k)
		if ok {
			pretty.Println(i, k, n.Meta())
		} else {
			pretty.Println(i, k)
		}
	}

	// ydb.DebugValueString(ud, 4, func(n ...interface{}) { fmt.Print(n) })
}
