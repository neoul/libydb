# YDB (YAML DataBlock)

**YAML DataBlock (YDB)** is a library to manage the hierarchical configuration and statistical data simply and clearly using YAML format input/output.

The following example shows you what YDB is briefly.


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

In this example, you will create an YDB datablock (`ydb_open`) and then write some data into the datablock using YAML format (`ydb_write`). After written, each data can be read from the datablock (`ydb_read`) like as `scanf` C function.

To compile the example, these libraries should be installed into your system.

- YAML library (http://pyyaml.org/wiki/LibYAML): C Fast YAML 1.1
- YDB library (This library)

```shell
neoul@neoul-dev:~/projects/study-c$ gcc ydb-example1.c -lydb -lyaml
neoul@neoul-dev:~/projects/study-c$ ./a.out 
fan_speed 100, fan_enabled false
neoul@neoul-dev:~/projects/study-c$
```


## YDB Library Installation

YDB library uses libyaml (http://pyyaml.org/wiki/LibYAML) to parse YAML format data stream into the datablock. So, libyaml should be installed before the YDB library installation. 

```
$ wget http://pyyaml.org/download/libyaml/yaml-0.2.1.tar.gz
$ tar xvfz yaml-0.2.1.tar.gz
$ cd yaml-0.2.1
$ ./configure
$ make
$ make install
```

```shell
$ sudo apt-get install autoconf libtool
$ sudo apt-get install -y libyaml-dev
```


```shell
$ autoreconf -i -f
$ ./configure or $ ./configure CFLAGS="-g -Wall"
$ make
$ make install
```

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
