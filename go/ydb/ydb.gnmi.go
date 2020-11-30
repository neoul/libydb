package ydb

import "time"

// github.com/neoul/gnxi/gnmi/model interface

// UpdateCreate function of the DataUpdate interface for *YDB
func (db *YDB) UpdateCreate(path string, value string) error {
	return db.WriteTo(path, value)
}

// UpdateReplace function of the DataUpdate interface for *YDB
func (db *YDB) UpdateReplace(path string, value string) error {
	return db.WriteTo(path, value)
}

// UpdateDelete function of the DataUpdate interface for *YDB
func (db *YDB) UpdateDelete(path string) error {
	return db.DeleteFrom(path)
}

// UpdateStart function of the DataUpdateStartEnd interface for *YDB
func (db *YDB) UpdateStart() error {
	return nil
}

// UpdateEnd function of the DataUpdateStartEnd interface for *YDB
func (db *YDB) UpdateEnd() error {
	return nil
}

// UpdateSync requests the update to remote YDB instances in order to refresh the data nodes.
func (db *YDB) UpdateSync(path ...string) error {
	return db.SyncTo(time.Second*3, true, path...)
}

// UpdateSyncPath requests the update to remote YDB instances in order to refresh the data nodes.
func (db *YDB) UpdateSyncPath() []string {
	return []string{}
}
