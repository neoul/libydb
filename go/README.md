# YDB (YAML DataBlock) Go Interface

**YDB Go Interface** is designed to support YDB facilities in Go. The **YDB Go Interface** is able to be used for creating and manipulating tree-structured data instance such as Go `struct`, `slice` or `map` automatically. To use this **YDB Go Interface**, you need to install `libydb` and `Go` compiler before starting.

- Supports to convert YAML list to Go slice.
- Supports to convert YAML map (key, value pair) to Go map.
- Supports to convert YAML map to Go struct fields and its data.
  - YDB finds and updates anonymous fields of the Go struct.

## Example

The example show you how to use the YDB Go Interface API. An YDB instance is created and updated with YAML seriailzed data and then it is converted to your Go data struct.

```go
package main

import (
    "github.com/kr/pretty"
    "github.com/neoul/libydb/go/ydb"
)

// Default Updater interface example

type userData struct {
    System struct {
        Cpu         string `path:"cpu"`
        Motherboard string
        Memory      int `unit:"GB"`
        Power       string
        InputDevice map[string]interface{} `path:"input"`
    } `path:"system"`
}

func main() {
    // Create a user-defined data structure
    userdb := &userData{}

    // Enable log
    // ydb.SetLogLevel(logrus.DebugLevel)
    // ydb.SetInternalLog(ydb.LogDebug)

    // Open an YDB instance
    db, close := ydb.Open("hello")
    defer close()

    // Write YAML data to the YDB instance
    db.Write([]byte(`
system:
  cpu: Pantium
  motherboard: Asus XXX
  memory: 16
  input:
    keyboard: Logitech
    mouse: ab
  power: 750W
`))
    // Read all data to the user-defined data structure.
    db.Convert(userdb)
    pretty.Println(userdb)
}
```

```bash
$ go run main.go
&main.userData{
    System: struct { Cpu string "path:\"cpu\""; Motherboard string; Memory int "unit:\"GB\""; Power string; InputDevice map[string]interface {} "path:\"input\"" }{
        Cpu:         "Pantium",
        Motherboard: "Asus XXX",
        Memory:      16,
        Power:       "750W",
        InputDevice: {
            "keyboard": "Logitech",
            "mouse":    "ab",
        },
    },
}
```

Each name of the Go struct fields is used as a case-insensitive key for the updated data by default. (e.g. The struct field `Motherboard` is matched to `motherboard` data in the YDB instance.)
If `path` tag is defined in your Go struct, it is also used as another key of the updated data. (e.g. `InputDevice`)

Another tag name can be used for the key. The `path` tag in the example can be changed to `json` if you want to support the mixed use of YDB with JSON marshalling and unmarshalling.

The following global options can be configured to control the YDB Go Interface operation according to your requirement.

```go
import "github.com/neoul/libydb/go/ydb"

// EnableTagLookup enables the tag lookup (e.g. "path") of struct fields for searching data
ydb.EnableTagLookup bool = true
// CaseInsensitiveFieldLookup enables the case-insensitive struct field name lookup.
ydb.CaseInsensitiveFieldLookup bool = true
// TagLookupKey is the tag name of the struct field for searching data
ydb.TagLookupKey string = "path"
// InitChildenOnSet initalizes all struct fields on Set.
ydb.InitChildenOnSet bool = false
```

## Synchronized data update

The example above show you a data update of the decoupled YDB and Go struct. In this example, unlike the example above, the YDB and Go struct will update together. When some data is updated to the YDB instance, it is updated to your Go struct, too. This synchronized data update requires the data update lock of your Go struct before access if there are multiple go routines (threads) running.

```go
    // Create a user-defined data structure
    userdb := &userData{}
    // Open an YDB instance
    db, close := ydb.OpenWithSync("hello", userdb)
    defer close()

    // Write YAML data to the YDB instance
    db.Write([]byte(`
system:
  cpu: Pantium
  motherboard: Asus XXX
  memory: 16
  input:
    keyboard: Logitech
    mouse: ab
  power: 750W
`))
    db.Lock()
    pretty.Println(userdb)
    db.Unlock()
```

## Updater & DataUpdate Interfaces

The default YDB data update operation (Default YDB Updater) can be customized and optimized by using `Updater` or `DataUpdate` interfaces defined in the `YDB Go Interface`. These two interfaces have the same operational meaning, but have the different function names and arguments.

- **`Updater`** (`Updater`, `UpdaterStartEnd`)
- **`DataUpdate`** (`DataUpdate`, `DataUpdateStartEnd`)

### Updater interface

```go
// Updater - An interface to handle a Go struct defined by user
type Updater interface {
    Create(keys []string, key string, tag string, value string) error
    Replace(keys []string, key string, tag string, value string) error
    Delete(keys []string, key string) error
}

// UpdaterStartEnd indicates the start and end of the data update.
// They will be called before or after the Updater (Create, Replace and Delete) execution.
type UpdaterStartEnd interface {
    UpdateStart()
    UpdateEnd()
}
```

### DataUpdate interface

```go
// DataUpdate interface (= Updater with different arguments) to handle a Go struct
type DataUpdate interface {
    UpdateCreate(path string, value string) error
    UpdateReplace(path string, value string) error
    UpdateDelete(path string) error
}

// DataUpdateStartEnd indicates the start and end of the data update.
// They will be called before or after the DataUpdate (Create, Replace and Delete) execution.
type DataUpdateStartEnd interface {
    UpdaterStartEnd
}
```

This is a customized DataUpdate interface.

```go

import "github.com/neoul/trie"

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


func main() {
    // The DataUpdate interface is implemented to ud (user data).
    ud := newTrie()
    db, close := ydb.OpenWithSync("test", ud)
    defer close()
    // # Update
    // system:
    //     fan:
    //     fan[1]:
    //         config_speed: 100
    //         current_speed: 100
    //     fan[2]:
    //         config_speed: 200
    //         current_speed: 200
    //     ram: 4G
    //     cpu: pentium
    //     mainboard: gigabyte
    r, err := os.Open("../../../examples/yaml/ydb-input.yaml")
    defer r.Close()
    if err != nil {
        panic(err)
    }
    dec := db.NewDecoder(r)
    dec.Decode()

    // Remove fan[1]
    db.DeleteFrom("/system/fan/fan[1]")

    // The data update lock is not required because there is no multiple thread.
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

```

## 

## Remote data update

[Remote data update example](demo/sync-update/main.go)

```bash
# Run the remote data update example.
cd demo/remote-update
go run main.go

# Open another terminal and then execute shell to push data.
./push.sh
```

## YDB package testing

```bash
cd ydb
go test
# go test -run=TestValNewStruct
```

## Cross compilation

```bash
cd demo/example
CC=aarch64-hfr-linux-gnu-gcc GOOS=linux GOARCH=arm64 CGO_ENABLED=1 go build demo.go
```
