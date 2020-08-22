# YDB (YAML DataBlock) Go Interface

## Testing

```bash
cd ydb
go test
# go test -run=TestValNewStruct
```

## Cross-compile

```bash
cd demo/example
CC=aarch64-hfr-linux-gnu-gcc GOOS=linux GOARCH=arm64 CGO_ENABLED=1 go build demo.go
```
