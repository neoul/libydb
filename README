# ymldb - YAML database

ymldb is designed for NETCONF state and statistics data.

A database constructed by pairs of key and value data with data hierarchical form (as tree).

All data is automatically synchronized with data owner.

All data is serialized to an YAML document for data transmission.

For synchronization, changed data is just re-transmitted.

Publisher (Data owner) and Subscriber fashion.


# Required libraries

## CPROPS (c prototyping tools)

### Website
http://cprops.sourceforge.net/

### Download
https://svwh.dl.sourceforge.net/project/cprops/cprops/cprops-0.1.12/libcprops-0.1.12.tar.gz

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