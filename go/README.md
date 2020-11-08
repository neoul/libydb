# YDB (YAML DataBlock) Go Interface

**YDB Go Interface** is designed to support YDB in Go. The **YDB Go Interface** can automatically create and manipulate a tree-structured data instance such as Go structure. To use this **YDB Go Interface**, you need to install `libydb` and `Go` compiler before starting.

## Example 1 - Updating Go map with hierarchy

The example show you how to use the YDB Go Interface API. An YDB instance is created and updated by YAML seriailzed data and then converted to your Go data structure.

```go
package main

import (
    "fmt"

    "github.com/kr/pretty"
    "github.com/neoul/libydb/go/ydb"
)

func main() {
    // Create a user-defined data structure
    userdb := map[string]interface{}{}

    // Enable log
    // ydb.SetLogLevel(logrus.DebugLevel)
    // ydb.SetInternalLog(ydb.LogDebug)

    // Open an YDB instance
    db, close := ydb.Open("hello")
    defer close()

    // Write data to the YDB instance
    db.WriteTo("/system/cpu", "pentium")

    // Write YAML data to the YDB instance
    db.Write([]byte(`
system:
  motherboard: Asus XXX
  memory: 16GB
  power: 750W
`))
    // Read all data to the user-defined data structure.
    db.Convert(ydb.RetrieveAll(), ydb.RetrieveStruct(userdb))
    pretty.Println(userdb)

    // Read the data from the leaf data. (Branch node doesn't have data.)
    r := db.ReadFrom("/system/cpu")
    fmt.Println(r)
}

```

```bash
$ go run main.go
map[string]interface {}{
    "system": map[string]interface {}{
        "cpu":         "pentium",
        "memory":      "16GB",
        "motherboard": "Asus XXX",
        "power":       "750W",
    },
}
pentium
```

## Example 2 - Updating user-defined Go structure

The **YDB Go Interface** also supports the conversion to the user-defined Go structure. Each name of the struct fields is used as a key for the updated data. If `path` tag is defined in your Go structure, it is also used as another key of the updated data.

```go
package main

import (
    "github.com/kr/pretty"
    "github.com/neoul/libydb/go/ydb"
)

type userData struct {
    System struct {
        Cpu         string `path:"cpu"`
        Motherboard string
        Memory      int
        Power       string
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

    // Write data to the YDB instance
    db.WriteTo("/system/cpu", "pentium")

    // Write YAML data to the YDB instance
    db.Write([]byte(`
system:
  Motherboard: Asus XXX
  Memory: 16
  Power: 750W
`))
    // Read all data to the user-defined data structure.
    db.Convert(ydb.RetrieveAll(), ydb.RetrieveStruct(userdb))
    pretty.Println(userdb)
}
```

Another tag name can be used for the key. In the following example, The `path` tag is changed to `json`. This supports the mixed use with JSON marshalling and unmarshalling.

```go
// Do not lookup another key name if EnableTagLookup is false.
ydb.EnableTagLookup = true
ydb.TagLookupKey = "json"
```

## Synchronized data update

### YDB Interface for data update

YDB Go Interface defines two kinds of interfaces to send the updated data to the user. Both `Updater` interface (`Updater`, `UpdaterStartEnd`, `SyncUpdater`) and `DataUpdate` interface (`DataUpdate`, `UpdaterStartEnd`, `DataSync`) are used to receive the same updated data and its operation (`Create`, `Replace` and `Delete`) with the different format.

#### Updater interface

The `Updater` interface is a list of functions to be implemented to the user data structure.

```go
// Updater interface to manipulate user structure
type Updater interface {
    Create(keys []string, key string, tag string, value string) error
    Replace(keys []string, key string, tag string, value string) error
    Delete(keys []string, key string) error
}

// UpdaterStartEnd - indicates the start and end of the YDB Update.
// They will be called before or after the Updater (Create, Replace and Delete) execution.
type UpdaterStartEnd interface {
    UpdateStart()
    UpdateEnd()
}

// SyncUpdater - Interface to update the target (pointed by the keys and key) data node upon sync request.
type SyncUpdater interface {
    SyncUpdate(keys []string, key string) []byte
}
```

#### DataUpdate interface

```go
// UpdateStartEnd - indicates the start and end of the YDB Update.
// They will be called before or after the DataUpdate (Create, Replace and Delete) execution.
type UpdateStartEnd interface {
    UpdaterStartEnd
}

// DataUpdate (= Updater with different arguments) for Modeled Data Update
type DataUpdate interface {
    UpdateCreate(path string, value string) error
    UpdateReplace(path string, value string) error
    UpdateDelete(path string) error
}

// DataSync (= SyncUpdater) for Modeled Data Sync
type DataSync interface {
    UpdateSync(path string) error
}
```


## Updating data from the remote

## Testing

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

## Running demo

```bash
cd go/demo/example
go run demo.go &
./demo.sh # Need to check the commands in demo.sh to use.
```
