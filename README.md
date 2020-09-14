# YDB (YAML DataBlock)

**YAML DataBlock (YDB)** is a library to manage the hierarchical configuration and statistical data simply and clearly using YAML input/output. YDB internally builds the hierarchical data block from the serialized input stream formed as YAML. And it supports the facilities to search, inquiry and iterate each internal data in YDB using the API. The input YAML stream is applied to the data block by the the following three types of operations properly. So, you don't need to allocate, free and manipulate each data block manually.

[Three types of operations in order to manage YDB]

- Create (`ydb_write`)
- Replace (`ydb_write`)
- Delete (`ydb_delete`)

In order to use **YAML DataBlock (YDB)** in your project, you need to have the knowledge of **YAML (YAML Ain't Markup Language)** that is used to the input and the output of YDB. Please, see the following website to get more information about YAML. 

- [https://yaml.org](https://yaml.org)
- [https://en.wikipedia.org/wiki/YAML](https://en.wikipedia.org/wiki/YAML)
- [https://learn.getgrav.org/advanced/yaml](https://learn.getgrav.org/advanced/yaml)

## other documents

- [https://docs.google.com/presentation/d/1oe3d0tzsOmsCKxgIlv_95MUBvxtF7xGGrqOLcLLH0vc/edit?usp=sharing](https://docs.google.com/presentation/d/1oe3d0tzsOmsCKxgIlv_95MUBvxtF7xGGrqOLcLLH0vc/edit?usp=sharing)

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
sudo apt-get install autoconf libtool build-essential
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
  fan:
    fan[INDEX]:
      admin: true
      config_speed: 100
      current_speed: 100
  # ...
```

Just write the fan data into your data block like as you use `printf`. You don't need to allocate memories and handle them.

```c
ydb_write(datablock,
    "system:\n"
    " fan:\n"
    "  fan[%d]:\n"
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
    " fan:\n"
    "  fan[1]:\n"
    "   admin: %s\n"
    "   config_speed: %d\n"
    "   current_speed: %d\n",
    fan_admin,
    &fan_speed,
    &fan_cur_speed);
```

Just remove the data if you don't need it.

```c
ydb_delete(datablock,
    "system:\n"
    "  fan:\n");
```

As the example, YDB can make you be free from the data type definition, structuring and manipulation. It will save your development time you spent for them.

## YDB input/output format (YAML)

YDB input/output is the stream of an YAML document. the YAML document represents a number of YAML nodes that have content of one of three kinds: scalar, sequence, or mapping.

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

[examples/ydb/ydb-ex-seq](examples/ydb/ydb-ex-seq.c)

[examples/ydb/ydb-ex-seq result](examples/ydb/ydb-ex-seq.txt)

### YAML Mapping node (hash)

The content of a YAML mapping node is an unordered set of `{key:value}` node pairs, with the restriction that each of the keys is unique. YAML places no further restrictions on the nodes. In particular, keys may be arbitrary nodes, the same node may be used as the value of several `{key:value}` pairs, and a mapping could even contain itself as a key or a value (directly or indirectly). In YDB, the YAML manpping nodes are used to categorize and construct the tree structure of the hierarchical configuration and statistical data. here is an sample of the mapping nodes.

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

or you can access the inet data using the simple path string as the following example.

```c
const char *data;
data = ydb_path_read(datablock, "/system/interfaces/eth0/ipaddr/inet");
fprintf(stdout, "/system/interfaces/eth0/ipaddr/inet=%s\n", data);
```

> **YDB path** is a string that list the keys to access the target data with '`/`' delimiter. The equal sign (`=`) is used to identify the value inserted to the target data node.

Please, see the following example if you want to check how to manage the mapping data in YDB.

> [examples/ydb/ydb-ex-map.c](examples/ydb/ydb-ex-map.c)

### YAML ordered mapping node (ordered hash)

YAML ordered mapping is a ordered sequence node of `{key:value}` pairs without duplicates. YDB allows the ordered mapping node is used for the YDB input stream. YDB keeps the sequence of `{key:value}` data insertions and deletes the duplicated data. you have to give the `!!omap` tag explicitly in order to use the ordered mapping node in YDB.

```shell
$ cat examples/yaml/yaml-omap.yaml
---
# http://yaml.org/type/omap.html ----------------------------------------------#

omap:
  # Explicitly typed ordered map (dictionary).
  Bestiary: !!omap
    - aardvark: African pig-like ant eater. Ugly.
    - anteater: South-American ant eater. Two species.
    - anaconda: South-American constrictor snake. Scaly.
    # Etc.
  # Flow style
  Numbers: !!omap [ one: 1, two: 2, three : 3 ]

# Ordered maps are represented as
# A sequence of mappings, with
# each mapping having one key
...


$ ydb -f examples/yaml/yaml-omap.yaml -s --write /omap/Numbers/ten=10

# top
omap:
 Bestiary: !!omap
  - aardvark: "African pig-like ant eater. Ugly."
  - anteater: "South-American ant eater. Two species."
  - anaconda: "South-American constrictor snake. Scaly."
 Numbers: !!omap
  - one: 1
  - two: 2
  - three: 3
  - ten: 10

$
```

If you use the un-ordered mapping node instead of the ordered mapping, you will get the different sequence. The un-ordered mapping node sorts the data by the key.

```shell
$ ydb --input -s <<EOF
map:
 Numbers:
  one: 1
  two: 2
  three: 3
EOF

# top
map:
 Numbers:
  one: 1
  three: 3
  two: 2
$
```

### YAML node types

YAML nodes may be labeled with a type or tag using the exclamation point (!! or !) followed by a string. The following sample shows you how to tag the types of scalar nodes.

```yaml
a: 123                     # an integer
b: "123"                   # a string, disambiguated by quotes
c: 123.0                   # a float
d: !!float 123             # also a float via explicit data type prefixed by (!!)
e: !!str 123               # a string, disambiguated by explicit type
f: !!str Yes               # a string via explicit type
g: Yes                     # a boolean True (yaml1.1), string "Yes" (yaml1.2)
h: Yes we have No bananas  # a string, "Yes" and "No" disambiguated by context.
picture: !!binary |
  R0lGODdhDQAIAIAAAAAAANn
  Z2SwAAAAADQAIAAACF4SDGQ
  ar3xxbJ9p0qa7R0YxwzaFME
  1IAADs=
```

YDB stores the all input data and types by strings without type conversion or interpretation. So, there is no data loss or processing burden in YDB.

```shell
$ ydb -f examples/yaml/yaml-types.yaml -s

# top
a: 123
b: 123
c: 123.0
d: !!float 123
e: !!str 123
f: !!str Yes
g: Yes
h: "Yes we have No bananas"
picture: !!binary "R0lGODdhDQAIAIAAAAAAANn\nZ2SwAAAAADQAIAAACF4SDGQ\nar3xxbJ9p0qa7R0YxwzaFME\n1IAADs="
```

### YAML document manipulation

YDB supports the following custom named tag for YAML document manipulation.
These tags can be used when you write or delete data from/to an YDB instance using `ydb_write()`, `ydb_delete()`.

```yaml
%TAG !ydb! tag:github.com/neoul/libydb/
---
libydb-operations:
  delete node: !ydb!delete
...
```

```c
// corrent_sppeed will be deleted during YDB updating (ydb_write()).
ydb_write(datablock,
    "system:\n"
    " fan:\n"
    "  fan[%d]:\n"
    "   admin: %s\n"
    "   config_speed: %d\n"
    "   current_speed: !ydb!delete\n",
    fan_id,
    fan_admin,
    &fan_speed);
// ...
```

## YDB (YAML DataBlock) for IPC (Inter-Process Communication)

**YAML DataBlock (YDB)** has the facilities to deliver the internal data block to other processes via unix socket, named FIFO, TCP and etc in the manner of Publish and Subscribe fashion.

### YDB IPC (Inter-Process Communication) feature specification

- Publish & Subscribe fashion
- Data block change monitoring
- Dynamic data block update

## YDB operation test tool

YDB library supports a [YDB operation test tool](https://github.com/neoul/libydb/blob/master/tools/ydb/ydb-app.c) and [test samples](https://github.com/neoul/libydb/tree/master/test) to verify YDB and YDB IPC facilities. Please, check the scripts how YDB works in detail.

## Performance

- string pool (ystr_pool) for memory saving
  - All strings allocated in YDB are mamaged by ystr_pool.
  - ystr_pool once allocates a string if the string is not in ystr_pool.
  - ystr_pool returns the allocated string for the same string allocation with increasing the reference count.
  - The string is freed upon the reference count to be zero.
- YDB node implemented by AVL tree
  - AVL tree (Search/Insert/Delete) performance is O(log n).
  - Each branch node (list or mapping node) of YDB is constructed by this AVL tree.
  - For simple calculation, let’s assume each branch node has the same number of child nodes.
  - In this case, YDB performance is in O (log n).
    - `y = L * log (m) = log ( m ** L)`
    - where m is the number of child nodes of a branch node and
    - L is the depth of the YDB hierarchy.

## Other algorithm

To support the data manipulation, YDB is implemented with a number of internal algorithms widely used. These algorithms can be included and used to your project to reduce the job about the algorithm implemenation.

- **YLIST**
  - YLIST is a simple double linked list working as queue or stack.
- **YTREE**
  - YTREE is a AVL tree that supports O(logn) search, insertion and deletion time.
- **YTRIE**
  - YTRIE is adaptive radix tree (prefix tree) implementation to implement ystr_pool.
- **YMAP**
  - YMAP is the ordered map (hash) constructed by YLIST and YTREE to support the YAML ordered map.
- **YARRAY**
  - YARRAY is a scalable array that consists of the small arrarys linked as a list.
- **YSTR**
  - The implementation of ystr_pool. (See Performance section)

## Limitation

### Multiple scalar keys

YDB doesn't support multiple scalar keys wrapping in sequence or mapping nodes. Because, the sequence or mapping nodes are not data comparable to other scalar keys.

```yaml
multiple-key example:
    { a, b } : mapping key example
    [ 1, 2 ] : sequence key example
```

The recommended format for the multiple keys is a single string concatenated by the multiple keys as follows.

```yaml
multiple-key example:
    a-b : mapping key example
    1-2 : sequence key example
```

### YAML features not supported

- !!pairs

### YAML feature supported

- !!map
- !!seq
- !!omap
- !!set
- anchor and alias with `<<` merge key
- all other types are supported as string type.
- !imap (a map indexed by integer key)

## next work items

- type control (!!str ... etc..)
- Performance enhancement for YDB iteration
- JSON parser & emitter
