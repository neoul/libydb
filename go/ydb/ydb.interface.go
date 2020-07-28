package ydb

// Updater interface to manipulate user structure
type Updater interface {
	Create(keys []string, key string, tag string, value string) error
	Replace(keys []string, key string, tag string, value string) error
	Delete(keys []string, key string) error
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

// UpdaterStartEnd - indicates the start and end of the YDB Update.
// They will be called before or after the Updater (Create, Replace and Delete) execution.
type UpdaterStartEnd interface {
	UpdateStart()
	UpdateEnd()
}

// UpdateStart - indicates the start of the YDB update. It will be called ahead of Updater interface
func (emptyStruct *EmptyGoStruct) UpdateStart() {
	log.Debugf("emptyStruct.UpdateStart")
}

// UpdateEnd - indicates the end of the YDB update. It will be called after Updater interface
func (emptyStruct *EmptyGoStruct) UpdateEnd() {
	log.Debugf("emptyStruct.UpdateEnd")
}
