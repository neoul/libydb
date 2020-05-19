package ydb

import (
	"io"
	"os"

	"github.com/op/go-logging"
)

var log *logging.Logger

// SetLog - Set the log facilities.
func SetLog(module string, out io.Writer, level logging.Level, formatstr string) *logging.Logger {
	if formatstr == "" {
		formatstr = `%{color}%{time} %{program}.%{module}.%{shortfunc:.12s} %{level:.5s} ▶%{color:reset} %{message}`
	}
	ilog := logging.MustGetLogger(module)
	// Example format string. Everything except the message has a custom color
	// which is dependent on the log level. Many fields have a custom output
	// formatting too, eg. the time returns the hour down to the milli second.
	var format = logging.MustStringFormatter(formatstr)
	logBackend := logging.NewLogBackend(out, "", 0)
	newLogBackend := logging.NewBackendFormatter(logBackend, format)
	leveledBackend := logging.AddModuleLevel(newLogBackend)
	leveledBackend.SetLevel(level, module)
	logging.SetBackend(leveledBackend)
	return ilog
}

func init() {
	log = SetLog("ydb", os.Stdout, logging.DEBUG, "%{message}")
}
