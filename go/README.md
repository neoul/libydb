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