package ydb

import (
	"io"
	"os"

	"github.com/op/go-logging"
)

var log *logging.Logger

// InitLog - Initialize the log facilities.
func InitLog(module string, out io.Writer) *logging.Logger {
	log = logging.MustGetLogger(module)
	// Example format string. Everything except the message has a custom color
	// which is dependent on the log level. Many fields have a custom output
	// formatting too, eg. the time returns the hour down to the milli second.
	var format = logging.MustStringFormatter(
		`%{color}%{time} %{program}.%{module}.%{shortfunc:.12s} %{level:.5s} ▶%{color:reset} %{message}`,
	)
	// For demo purposes, create two backend for out.
	backend1 := logging.NewLogBackend(out, "", 0)
	backend2 := logging.NewLogBackend(out, "", 0)

	// For messages written to backend2 we want to add some additional
	// information to the output, including the used log level and the name of
	// the function.
	backend2Formatter := logging.NewBackendFormatter(backend2, format)

	// Only errors and more severe messages should be sent to backend1
	backend1Leveled := logging.AddModuleLevel(backend1)
	backend1Leveled.SetLevel(logging.ERROR, "")

	// Set the backends to be used.
	logging.SetBackend(backend1Leveled, backend2Formatter)
	return log
}

func init() {
	log = InitLog("ydb", os.Stderr)
}
