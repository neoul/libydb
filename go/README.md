# YDB (YAML DataBlock) Go Interface

This is YDB Go Interface to support tree-structured data construction. It supports the YAML-formed data tree creation, manipulation and deletion with access to each data node. You have to install **libydb** and **libyaml** to use this Go interface.

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

## YDB Interface for data update

There are two kinds of interfaces to receive the data update information from YDB. Both `Updater` interface (`Updater`, `UpdaterStartEnd`, `SyncUpdater`) and `DataUpdate` interface (`DataUpdate`, `UpdaterStartEnd`, `DataSync`) are used to receive the same updated data and its operation (`Create`, `Replace` and `Delete`) with the different format.

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