package main

// DataUpdate interface Example

import (
	"os"

	"github.com/kr/pretty"
	"github.com/neoul/gdump"
	"github.com/neoul/gtrie"
	"github.com/neoul/libydb/go/ydb"
)

// DataUpdate interface example
// Implement DataUpdate interface for the user Go struct.

type userdata struct {
	*gtrie.Trie
}

func newTrie() *userdata {
	return &userdata{gtrie.New()}
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
	keys := u.FindByPrefix(path)
	for _, p := range keys {
		u.Remove(p)
	}
	return nil
}

func (u *userdata) FuzzyFind(path string) error {
	keys := u.FindByFuzzy(path)
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
	r, err := os.Open("../../../examples/yaml/ydb-input2.yaml")
	defer r.Close()
	if err != nil {
		panic(err)
	}
	dec := db.NewDecoder(r)
	dec.Decode()
	// pretty.Println(ud)

	ud.FuzzyFind("/system/fan/fan[2")
	// Remove fan[1]
	db.DeleteFrom("/system/fan/fan[1]")

	// The update lock is not required because there is no multiple thread.
	// db.Lock()
	// defer db.Unlock()
	pretty.Print(ud.Trie.FindAll(""))
}
