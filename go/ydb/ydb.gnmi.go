package ydb

import (
	"bytes"
	"fmt"
	"strings"
	"time"
)

// github.com/neoul/gnxi/gnmi/model interface

// UpdateCreate function of the DataUpdate interface for *YDB
func (db *YDB) UpdateCreate(path string, value string) error {
	if !db.atomicDataUpdate {
		return db.WriteTo(path, value)
	}
	entry := &dataUpdateEntry{
		isDelete: false,
		path:     path,
		value:    value,
	}
	db.dataUpdate = append(db.dataUpdate, entry)
	return nil
}

// UpdateReplace function of the DataUpdate interface for *YDB
func (db *YDB) UpdateReplace(path string, value string) error {
	if !db.atomicDataUpdate {
		return db.WriteTo(path, value)
	}
	entry := &dataUpdateEntry{
		isDelete: false,
		path:     path,
		value:    value,
	}
	db.dataUpdate = append(db.dataUpdate, entry)
	return nil
}

// UpdateDelete function of the DataUpdate interface for *YDB
func (db *YDB) UpdateDelete(path string) error {
	if !db.atomicDataUpdate {
		return db.DeleteFrom(path)
	}
	entry := &dataUpdateEntry{
		isDelete: true,
		path:     path,
	}
	db.dataUpdate = append(db.dataUpdate, entry)
	return nil
}

// UpdateStart function of the DataUpdateStartEnd interface for *YDB
func (db *YDB) UpdateStart() error {
	if !db.atomicDataUpdate {
		return nil
	}
	db.Lock()
	db.dataUpdate = make([]*dataUpdateEntry, 0, 12)
	return nil
}

// ToYAML converts the path and value as YAML bytes and then write them to bytes.Buffer.
func ToYAML(path *string, value *string, isDel bool) ([]byte, error) {
	if path == nil {
		return nil, fmt.Errorf("empty path")
	}
	keylist, err := ToSliceKeys(*path)
	if err != nil {
		return nil, err
	}
	indent := 0
	var b bytes.Buffer
	for _, key := range keylist {
		b.WriteString(fmt.Sprintf("\n%s%s:", strings.Repeat(" ", indent), key))
		indent++
	}
	if isDel {
		b.WriteString(" !ydb!delete\n")
		return b.Bytes(), nil
	}
	if value == nil || *value == "" {
		b.WriteString("\n")
	} else {
		b.WriteString(fmt.Sprintf(" \"%s\"\n", *value))
	}
	return b.Bytes(), nil
}

// UpdateEnd function of the DataUpdateStartEnd interface for *YDB
func (db *YDB) UpdateEnd() error {
	if !db.atomicDataUpdate {
		return nil
	}
	defer db.Unlock()
	var b bytes.Buffer
	for _, entry := range db.dataUpdate {
		ybytes, err := ToYAML(&entry.path, &entry.value, entry.isDelete)
		if err != nil {
			db.dataUpdate = nil
			return err
		}
		b.Write(ybytes)
	}
	err := db.parse(b.Bytes(), true)
	db.dataUpdate = nil
	return err
}

// UpdateSync requests the update to remote YDB instances in order to refresh the data nodes.
func (db *YDB) UpdateSync(path ...string) error {
	return db.EnhansedSyncTo(time.Second*1, true, path...)
}

// UpdateSyncPath requests the update to remote YDB instances in order to refresh the data nodes.
func (db *YDB) UpdateSyncPath() []string {
	return []string{}
}
