package main

// DataUpdate interface Example

import (
	"os"

	"github.com/kr/pretty"
	"github.com/neoul/gdump"
	"github.com/neoul/libydb/go/ydb"
	"github.com/neoul/trie"
)

// DataUpdate interface example
// Implement DataUpdate interface for the user Go struct.

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

func (u *userdata) FuzzyFind(path string) error {
	keys := u.FuzzySearch(path)
	gdump.Print(keys)
	return nil
}

func main() {
	// The DataUpdate interface is implemented to ud (user data).
	ud := newTrie()
	db, close := ydb.OpenWithSync("test", ud)
	defer close()
	// # YAML data
	// system:
	// 	fan:
	// 	fan[1]:
	// 		config_speed: 100
	// 		current_speed: 100
	// 	fan[2]:
	// 		config_speed: 200
	// 		current_speed: 200
	// 	ram: 4G
	// 	cpu: pentium
	// 	mainboard: gigabyte
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
	ud.FuzzyFind("/system/fan/fan[2")
	// Remove fan[1]
	db.DeleteFrom("/system/fan/fan[1]")

	// The update lock is not required because there is no multiple thread.
	// db.Lock()
	// defer db.Unlock()
	for i, k := range ud.Keys() {
		n, ok := ud.Find(k)
		if ok {
			pretty.Println(i, k, n.Meta())
		} else {
			pretty.Println(i, k)
		}
	}
}
