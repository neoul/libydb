
# Run

```bash
source env.sh
go build
./demo.sh
```

# Cross compile with CGO

```bash
CC=aarch64-hfr-linux-gnu-gcc GOOS=linux GOARCH=arm64 CGO_ENABLED=1 go build demo.go
```
