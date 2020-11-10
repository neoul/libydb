package ydb

// Updater interface to handle a Go struct defined by user
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

// SyncUpdater - Interface to update the target (pointed by the keys and key) data node on which a request signaled.
type SyncUpdater interface {
	// SyncUpdate is a callback for target data node synchronization.
	// It must return YAML bytes to be updated.
	SyncUpdate(keys []string, key string) []byte
}

// EmptyGoStruct - Empty Go Struct for empty Updater interface
type EmptyGoStruct struct{}

// Create - Interface to create an entity on !!map object
func (emptyStruct *EmptyGoStruct) Create(keys []string, key string, tag string, value string) error {
	log.Debugf("emptyStruct.Create(%s, %s, %s, %s)", keys, key, tag, value)
	return nil
}

// Replace - Interface to replace the entity on !!map object
func (emptyStruct *EmptyGoStruct) Replace(keys []string, key string, tag string, value string) error {
	log.Debugf("emptyStruct.Replace(%s, %s, %s, %s)", keys, key, tag, value)
	return nil
}

// Delete - Interface to delete the entity from !!map object
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

// SyncUpdate - Interface to update the target (pointed by the keys and key) data node upon sync request.
func (emptyStruct *EmptyGoStruct) SyncUpdate(keys []string, key string) []byte {
	log.Debugf("emptyStruct.SyncUpdate(%s, %s)", keys, key)
	return nil
}

// DataUpdate interface (= Updater with different arguments) to handle a Go struct
type DataUpdate interface {
	UpdateCreate(path string, value string) error
	UpdateReplace(path string, value string) error
	UpdateDelete(path string) error
}

// UpdateStartEnd indicates the start and end of the data update.
// They will be called before or after the DataUpdate (Create, Replace and Delete) execution.
type UpdateStartEnd interface {
	UpdaterStartEnd
}

// DataSync (= SyncUpdater)
type DataSync interface {
	UpdateSync(path string) error
}
