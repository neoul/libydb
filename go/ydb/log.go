package ydb

import (
	"os"
	"time"

	nested "github.com/antonfisher/nested-logrus-formatter"
	"github.com/sirupsen/logrus"
)

var logger *logrus.Logger
var log *logrus.Entry

// NewLogEntry - Create new logger for logging.
func NewLogEntry(component string) *logrus.Entry {
	// A common pattern is to re-use fields between logging statements by re-using
	// the logrus.Entry returned from WithFields()
	return logger.WithFields(logrus.Fields{
		"component": component,
	})
}

func init() {
	logger = logrus.New()
	logger.SetOutput(os.Stderr)
	// logger.SetLevel(logrus.ErrorLevel)
	logger.SetLevel(logrus.DebugLevel)
	logger.SetFormatter(&nested.Formatter{
		HideKeys: true,
		// FieldsOrder:     []string{"component", "category"},
		FieldsOrder:     []string{"component"},
		TimestampFormat: time.RFC3339,
		// ShowFullLevel: true,
	})

	// A common pattern is to re-use fields between logging statements by re-using
	// the logrus.Entry returned from WithFields()
	log = logger.WithFields(logrus.Fields{
		"component": "ydb",
	})
	// log.Info("just info message")
	// Output: Jan _2 15:04:05.000 [INFO] just info message

	// log.WithField("component", "rest").Warn("warn message")
	// Output: Jan _2 15:04:05.000 [WARN] [rest] warn message
}

// SetLogLevel - Set the level of the YDB library
func SetLogLevel(level logrus.Level) {
	log.Logger.SetLevel(level)
}
