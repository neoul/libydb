# YDB (YAML DataBlock)

**YAML DataBlock (YDB)** is a library to manage the hierarchical configuration and statistical data simply and clearly using YAML input/output. YDB internally builds the hierarchical data block from the serialized input stream formed as YAML. And it supports the facilities to search, inquiry and iterate each internal data in YDB using the API. The input YAML stream is applied to the data block by the the following three types of operations properly. So, you don't need to allocate, free and manipulate each data block manually.

[Three types of operations in order to manage YDB]

- Create (`ydb_write`)
- Replace (`ydb_write`)
- Delete (`ydb_delete`)

In order to use **YAML DataBlock (YDB)** in your project, you need to have the knowledge of **YAML (YAML Ain't Markup Language)** that is used to the input and the output of YDB. Please, see the following website to get more information about YAML. 

- [https://en.wikipedia.org/wiki/YAML](https://en.wikipedia.org/wiki/YAML)
- [https://yaml.org](https://yaml.org)

## First example of YDB usage

The following example shows you how to use YDB briefly.

``` c
// ydb-example1.c

#include <stdio.h>
#include <ydb.h>
int main(int argc, char *argv[])
{
    ydb *datablock;

    datablock = ydb_open("system");

    ydb_write(datablock,
              "system:\n"
              " hostname: %s\n"
              " fan-speed: %d\n"
              " fan-enable: %s\n"
              " os: %s\n",
              "my-machine",
              100,
              "false",
              "linux");

    int fan_speed = 0;
    char fan_enabled[32] = {0};

    ydb_read(datablock,
            "system:\n"
            " fan-speed: %d\n"
            " fan-enable: %s\n",
            &fan_speed,
            fan_enabled);

    printf("fan_speed %d, fan_enabled %s\n", fan_speed, fan_enabled);

    ydb_close(datablock);
    return 0;
}
```

In this example, fan data exists in a system category. If you want to get the fan information, you just need to read the data from the data block using `ydb_read` after `ydb_write`. `ydb_read` is working such as `scanf` that we often use.

## YDB Library Installation

To compile the above example, these following libraries should be installed into your system.

- YAML library: [http://pyyaml.org/wiki/LibYAML](http://pyyaml.org/wiki/LibYAML)
- YDB (YAML DataBlock) library

> YDB library uses [libyaml](http://pyyaml.org/wiki/LibYAML) to parse YAML format data stream into the datablock. So, the libyaml should be installed before YDB library installation.

### 1. YAML library installation

#### YAML library installation from the source

```shell
wget http://pyyaml.org/download/libyaml/yaml-0.2.1.tar.gz
tar xvfz yaml-0.2.1.tar.gz
cd yaml-0.2.1
./configure
make
make install
```

#### YAML library installation using apt-get

```shell
sudo apt-get install autoconf libtool
sudo apt-get install -y libyaml-dev
```

### 2. YDB library installation

```shell
git clone https://github.com/neoul/libydb.git
cd libydb
autoreconf -i -f
./configure CFLAGS="-g -Wall"
make
make install
```

### 3. The above example compilation

```shell
$ gcc ydb-example1.c -lydb -lyaml
$ ./a.out
fan_speed 100, fan_enabled false
$
```

## Working with YDB

If you make a C application with YDB, you just need to categorize the data, where that belongs to, instead of considering the data type, structuring and data manipulation. Let's assume we make a fan control logic as follows.

> YAML listes and hashes can contain nested lists and hashes, forming a tree structure. The data categorization (in other words, constructing the tree structure of the hierarchical configuration and statistical data) can be accomplished by the YAML lists and hashes nesting.

```c
typedef struct fan_ctrl_s
{
    enum {
        FAN_DISABLED,
        FAN_ENABLED,
    } admin,
    unsigned int conf_speed;
    unsigned int current_speed;
} fan_ctrl_t;

struct system
{
    // ...
    fan_ctrl_t fan[2];
    // ...
};
```

If we re-design the data structure to an YAML input for YDB, it would be like this. The fan control data would belong to the system category.

```yaml
system:
  fan-control:
    <FAN-INDEX>:
      admin: true
      config_speed: X
      current_speed: Y
```

Just write the fan data into your data block like as you use `printf`. You don't need to allocate memories and handle them.

```c
ydb_write(datablock,
    "system:\n"
    " fan-control:\n"
    "  %d:\n"
    "   admin: %s\n"
    "   config_speed: %d\n"
    "   current_speed: %d\n",
    fan_index,
    fan_admin?"true":"false",
    fan_speed,
    fan_cur_speed);
```

Just read the fan data like as you use `scanf`. If there is no data in your data block, `ydb_read` doesn't update any variables.

```c
char fan_admin[32] = {0};
int fan_speed = 0;
int fan_cur_speed = 0;
ydb_read(datablock,
    "system:\n"
    " fan-control:\n"
    "  1:\n"
    "   admin: %s\n"
    "   config_speed: %d\n"
    "   current_speed: %d\n",
    fan_admin,
    &fan_speed,
    &fan_cur_speed);
```

Just remove the data if you don't need it.

```c
// flow style
ydb_delete(datablock, "system {fan-control: }\n");
// block style
ydb_delete(datablock,
    "system:\n"
    "  fan-control:\n");
```

As the example, YDB can make you be free from the data type definition, structuring and manipulation. It will save your development time you spent for them.

## YDB input/output format (YAML)

YDB input/output is a stream of an YAML document. the YAML document represents a number of YAML nodes that have content of one of three kinds: scalar, sequence, or mapping.

### YAML Scalar node (data)

The content of a YAML scalar node is an opaque datum that can be presented as a series of zero or more Unicode characters. In YDB, the scalar node can be the value that the user want to store and the key that is used to access or identify the value. The following sample shows an YAML mapping node (key and value pair data) that consists of two scalar nodes.

```yaml
name: willing
```

### YAML Sequence node (list)

The content of a YAML sequence node is an ordered series of zero or more nodes. In particular, a sequence may contain the same node more than once. It could even contain itself (directly or indirectly). In YDB, the sequence nodes can be only accessed by the index because, the sequence nodes don't have keys to identify them. Here is the sample of the sequence nodes. the YAML sequence nodes would be known as lists or tuples in Python and arrays in Java.

```yaml
block-style:
 - entry1
 - entry2
 - entry3
flow-style:
 [ entry1, entry2, entry3 ]
```

YDB handles these sequence nodes in the manner of FIFO. The entry insertion to the sequence is only allowed in the tail of the sequence. The entry deletion from the sequence is only allowed in the head of the sequence. The sequence entries can be identified and accessed by the index but, there is no way to represent the index of the sequence entries in a YAML document. So, YDB restricts the sequence manipulation like belows.

- `ydb_write`: Push all inserting entries back to the sequence.
- `ydb_delete`: Pop a number of entries from the head of the sequence.
- `ydb_read`: Read a number of entries from the head of the sequence.
- `ydb_path_write`: Push an entry into the tail of the sequence.
- `ydb_path_delete`: Only delete the first entry of the sequence.
- `ydb_path_read`: Read the target entry by the index.

Please, see the following code if you want to check how to control the sequence in YDB.

> [examples/ydb/ydb-ex-seq.c](examples/ydb/ydb-ex-seq.c)

```shell
# examples/ydb/ydb-ex-seq.c result

YDB example for YAML sequence (list)
=============================

[ydb_parses]
- 
- entry1
- entry2
- entry3

[ydb_write] (push two entries to the tail)
- 
- entry1
- entry2
- entry3
- entry4
- entry5

[ydb_delete] (pop two entries from the head)
- entry2
- entry3
- entry4
- entry5

[ydb_read] (read two entries from the head)
e1=entry2, e2=entry3

[ydb_read] (read 3th entry from the head)
e3=entry4

[ydb_read] (read 3th entry using yaml flow sequence format)
e3=entry4

[ydb_path_read] (read 3th entry using ydb path)
/2=entry4

[ydb_path_delete] only delete the first entry. others are not allowed.
delete /3 ==> delete not allowed
delete /0 ==> ok

[ydb_path_write] push an entry to the tail. 
write /0=abc ==> ok
- entry3
- entry4
- entry5
- abc

```

### YAML Mapping (hash)

The content of a YAML mapping node is an unordered set of `{key: value}` node pairs, with the restriction that each of the keys is unique. YAML places no further restrictions on the nodes. In particular, keys may be arbitrary nodes, the same node may be used as the value of several `{key: value}` pairs, and a mapping could even contain itself as a key or a value (directly or indirectly). In YDB, the YAML manpping nodes are used to categorize and construct the tree structure of the hierarchical configuration and statistical data. here is an sample of the mapping nodes.

```yaml
system:
 interfaces:
  eth0:
   ipaddr:
    inet: 172.17.0.1
    netmask: 255.255.0.0
  eth1:
   ipaddr:
    inet: 172.18.0.1
    netmask: 255.255.0.0
```

In this example, a number of mapping nodes are nested to categorize the data. The keys of the nested mapping nodes are used to access each value. for instance, to access the inet data of eth1 interface, you need `system`, `interfaces`, `eth1`, `ipaddr` and `inet` keys. The following example shows you how to get the inet data using `ydb_read` API.

```c
int n;
char addr[32];
n = ydb_read(datablock,
    "system:\n"
    " interfaces:\n"
    "  eth0:\n"
    "   ipaddr:\n"
    "    inet: %s\n",
    addr);

if (n != 1)
    fprintf(stderr, "no data\n");
else
    fprintf(stdout, "%s\n", addr);
// ...
```

Please, see the following code if you want to check how to control the mapping data in YDB.

> [examples/ydb/ydb-ex-map.c](examples/ydb/ydb-ex-map.c)

### YAML ordered mapping

## YDB over multiple processes

**YAML DataBlock (YDB)** has the facilities to communicate among processes.

### YDB IPC (Inter-Process Communication) feature specification

- Publish & Subscribe fashion
- Change notification
- Dynamic update

## YDB read hook for dynamic YDB update

## YDB write hook for change notification


## Performance

- string pool for memory saving
  - All strings allocated in YDB are mamaged by string_pool.
  - string_pool once allocates a string if the string is not in string_pool.
  - string_pool returns the allocated string for the same string allocation with increasing the reference count.
  - The string is freed upon the reference count to be zero.
- YDB node implemented by AVL tree
  - AVL tree (Search/Insert/Delete) performance is O(log n).
  - Each branch node of YDB is constructed by this AVL tree.
  - For simple calculation, let’s assume each branch node has the same number of child nodes.
  - In this case, YDB performance is in O (log n).
    - `y = L * log (m) = log ( m ** L)`
    - where m is the number of child nodes of a branch node and
    - L is the depth of the YDB hierarchy.

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

## next work items

- type control (!!str ... etc..)
- Performance enhancement for YDB iteration
- JSON parser & emitter
