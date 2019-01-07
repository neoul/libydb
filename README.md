# YDB (YAML DataBlock)

**YAML DataBlock (YDB)** is a library to manage the hierarchical configuration and statistical data simply and clearly using YAML input/output. YDB internally builds the hierarchical data block from the serialized input stream formed as YAML. And it supports the facilities to search, inquiry and iterate each internal data in YDB using the API. The input YAML stream is applied to the data block by the the following three types of operations properly. So, you don't need to allocate, free and manipulate each data block manually.

[Three types of operations in order to manage YDB]

- Create (ydb_write)
- Replace (ydb_write)
- Delete (ydb_delete)

In order to use **YAML DataBlock (YDB)** in your project, you need to have the knowledge of **YAML (YAML Ain't Markup Language)** that is used to the input and the output of YDB. Please, see the following website to get more information about YAML. 

- Official website: https://yaml.org/
- Wiki: https://en.wikipedia.org/wiki/YAML

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

In this example, fan information exists in the system category. If you want to get the fan information, you just need to read the data from the data block using `ydb_read` after `ydb_write`. `ydb_read` is working such as `scanf` that we frequently use.

## Working with YDB

If you develop a C application with YDB, you need to categorize the data where that belongs to instead of considering the data type, structuring and data manipulation. Let's assume we develop a fan control logic as follows.

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

If we re-design the data structure to an YAML input for YDB, it would be like this.

```yaml
system:
  fan-control:
    <FAN-INDEX>:
      admin: true
      config_speed: X
      current_speed: Y
```

Just write the fan data into your data block as follows. You don't need to allocate memories and manipulate them.

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

Just read the fan data like as you write when you need.

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

## YDB Library Installation

To compile the above example, these libraries should be installed into your system.

- YAML library (http://pyyaml.org/wiki/LibYAML): C Fast YAML 1.1
- YDB (YAML DataBlock) library

> YDB library uses libyaml (http://pyyaml.org/wiki/LibYAML) to parse YAML format data stream into the datablock. So, libyaml should be installed before the YDB.

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

## Dump YAML DataBlock

```c
ydb_dump(datablock, stdout);
```

## Read YAML data from a file





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
