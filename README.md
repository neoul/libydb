# ymldb - YAML database

ymldb is designed for NETCONF state and statistics data.

A database constructed by pairs of key and value data with data hierarchical form (as tree).

All data is automatically synchronized with data owner.

All data is serialized to an YAML document for data transmission.

For synchronization, changed data is just re-transmitted.

Publisher (Data owner) and Subscriber fashion.


# Required libraries

## YAML

### Website
http://pyyaml.org/wiki/LibYAML

### Download
http://pyyaml.org/download/libyaml/yaml-0.1.7.tar.gz


# Build ymldb

Try is if you meet an error in the library build.

```
$ sudo apt-get install autoconf libtool
$ autoreconf -i -f
$ ./configure or $ ./configure CFLAGS="-g -Wall"
$ make
$ make install
```

# YDB (YAML DataBlock)

YAML DataBlock for Configuration Data Management using YAML and IPC (Inter Process Communication)

## Limitation

### YAML features not supported

- !!binary
- !!pairs

### YAML feature supported

- !!map
- !!seq
- !!omap
- !!set
- anchor and alias with `<<` merge key
- all other types are supported as string type.
- !!imap (a map indexed by integer key)
