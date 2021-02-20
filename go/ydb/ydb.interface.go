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
	UpdaterStart() error
	UpdaterEnd() error
}

// UpdaterSyncResponse - An interface to update the target
// data node on which a request (DataUpdateSyncRequester) signaled.
type UpdaterSyncResponse interface {
	// SyncResponse is a callback for target data node synchronization.
	// It must return YAML bytes to be updated.
	SyncResponse(keys []string, key string) []byte
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
	UpdateStart() error
	UpdateEnd() error
}

// DataUpdateSyncResponse - An interface to update the target
// data node on which a request (DataUpdateSyncRequester) signaled.
type DataUpdateSyncResponse interface {
	// SyncResponse is a callback for target data node synchronization.
	// It must return YAML bytes to be updated.
	SyncResponse(path string) []byte
}
