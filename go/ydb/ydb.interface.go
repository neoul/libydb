package ydb

// Updater - An interface to handle a Go struct defined by user
type Updater interface {
	Create(keys []string, key string, tag string, value string) error
	Replace(keys []string, key string, tag string, value string) error
	Delete(keys []string, key string) error
}

// UpdaterStartEnd indicates the start and end of the data update.
// They will be called before or after the Updater (Create, Replace and Delete) execution.
type UpdaterStartEnd interface {
	UpdateStart()
	UpdateEnd()
}

// UpdaterSyncResponse - An interface to update the target
// data node on which a request (DataUpdateSyncRequester) signaled.
type UpdaterSyncResponse interface {
	// SyncResponse is a callback for target data node synchronization.
	// It must return YAML bytes to be updated.
	SyncResponse(keys []string, key string) []byte
}

// EmptyGoStruct - Empty Go Struct for empty Updater interface
type EmptyGoStruct struct{}

// Create - Updater function to create an entity on !!map object
func (emptyStruct *EmptyGoStruct) Create(keys []string, key string, tag string, value string) error {
	log.Debugf("emptyStruct.Create(%s, %s, %s, %s)", keys, key, tag, value)
	return nil
}

// Replace - Updater function to replace the entity on !!map object
func (emptyStruct *EmptyGoStruct) Replace(keys []string, key string, tag string, value string) error {
	log.Debugf("emptyStruct.Replace(%s, %s, %s, %s)", keys, key, tag, value)
	return nil
}

// Delete - Updater function to delete the entity from !!map object
func (emptyStruct *EmptyGoStruct) Delete(keys []string, key string) error {
	log.Debugf("emptyStruct.Delete(%s, %s)", keys, key)
	return nil
}

// UpdateStart - indicates the start of the YDB update. It will be called ahead of Updater interface
func (emptyStruct *EmptyGoStruct) UpdateStart() {
	log.Debugf("emptyStruct.UpdateStart")
}

// UpdateEnd - indicates the end of the YDB update. It will be called after Updater interface
func (emptyStruct *EmptyGoStruct) UpdateEnd() {
	log.Debugf("emptyStruct.UpdateEnd")
}

// SyncResponse - Interface to update the target (pointed by the keys and key) data node upon sync request.
func (emptyStruct *EmptyGoStruct) SyncResponse(keys []string, key string) []byte {
	log.Debugf("emptyStruct.SyncResponse(%s, %s)", keys, key)
	return nil
}

// DataUpdate interface (= Updater with different arguments) to handle a Go struct
type DataUpdate interface {
	UpdateCreate(path string, value string) error
	UpdateReplace(path string, value string) error
	UpdateDelete(path string) error
}

// DataUpdateStartEnd indicates the start and end of the data update.
// They will be called before or after the DataUpdate (Create, Replace and Delete) execution.
type DataUpdateStartEnd interface {
	UpdaterStartEnd
}

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
func (db *YDB) UpdateStart() {
	db.UpdateStart()
}

// UpdateEnd function of the DataUpdateStartEnd interface for *YDB
func (db *YDB) UpdateEnd() {
	db.UpdateEnd()
}
