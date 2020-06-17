package ydb

import (
	"io/ioutil"
	"os"

	nested "github.com/antonfisher/nested-logrus-formatter"
	"github.com/sirupsen/logrus"
)

var logger *logrus.Logger
var log *logrus.Entry

// DisableLog - disable the log facilities.
func DisableLog(component string) {
	logrus.SetOutput(ioutil.Discard)
	logrus.SetLevel(logrus.ErrorLevel)
}

// GetLogger - get new logger for logging.
func GetLogger(component string) *logrus.Entry {
	// A common pattern is to re-use fields between logging statements by re-using
	// the logrus.Entry returned from WithFields()
	return logger.WithFields(logrus.Fields{
		"component": component,
	})
}

func init() {
	// Log as JSON instead of the default ASCII formatter.
	// log.SetFormatter(&log.JSONFormatter{})

	// Output to stdout instead of the default stderr
	// Can be any io.Writer, see below for File example
	// logrus.SetOutput(os.Stdout)

	// Only log the warning severity or above.
	// logrus.SetLevel(logrus.DebugLevel)
	logrus.SetOutput(ioutil.Discard)
	logrus.SetLevel(logrus.ErrorLevel)

	// logrus.WithFields(logrus.Fields{
	// 	"omg":    true,
	// 	"number": 122,
	// }).Warn("The group's number increased tremendously!")

	logger = logrus.New()
	logger.SetOutput(os.Stdout)
	logger.SetLevel(logrus.DebugLevel)
	logger.SetFormatter(&nested.Formatter{
		HideKeys:    true,
		FieldsOrder: []string{"component", "category"},
		// ShowFullLevel: true,
	})

	// A common pattern is to re-use fields between logging statements by re-using
	// the logrus.Entry returned from WithFields()
	log = logger.WithFields(logrus.Fields{
		"component": "ydb.go",
	})
	// log.Info("just info message")
	// Output: Jan _2 15:04:05.000 [INFO] just info message

	// log.WithField("component", "rest").Warn("warn message")
	// Output: Jan _2 15:04:05.000 [WARN] [rest] warn message
}
