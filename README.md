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

## Why does YDB come out?

Embedded system 에서 하나의 application을 개발하다보면, 매번 똑같은 일을 반복하게 됩니다. 우선, 사용할 data의 type과 data structure를 정의하고, data의 life cycle에 따라 할당/해제하고, data의 type과 data structure를 관리할 인터페이스 함수를 만들고, 이를 시스템의 다른 기능에 연결하고, 정보를 조회할 수 있도록 show나 configuration 파일을 생성합니다. 이러한 과정은 매번 번거럽고 반복적인 작업이 아닐 수 없습니다. 이러한 반복되는 작업에서 최소화 할 수 있을까? YDB는 이 물음에 대한 해법의 하나로 시작했습니다.

### Less consideration for the data type

우리는 매번 내부적으로 사용되는 data type을 정의하고, data 관리 인터페이스를 구현합니다만, 사실 내부적으로 사용되는 data의 저장 형태 (type)는 크게 중요하지 않습니다. 대신 입력된 data를 읽는 시점에서 값의 의미를 해석하고 적절한 동작을 수행하는 것이 중요합니다. 가령, 아래 예제와 같은 fan control 기능의 admin 정보는 boolean (true/false), integer (0/1) 또는 string ("enable"/"disable") 과 같은 어려가지 형태로 표현할 수 있지만, 개발자가 integer (0/1) 이라고 admin 정보를 입력했다면, 읽는 시점에서도 동일하게 integer (0/1) 인지를 확인하고, 해당 값에 맞춰 fan을 동작 시키는 것이 admin 기능의 핵심일 것입니다.

YDB는 내부적으로 모든 data를 string으로 저장하고, 꺼내오는 시점에 사용자의 요구에 맞춰 형변환 (such as `scanf`)을 수행하도록 설계되었습니다. 만약 입력시 사용된 변수가 integer라면, 읽어올 때도 동일하게 integer 변수로 읽어오면 됩니다. YDB는 개발자가 내부적으로 사용되는 data type에 대한 고려보다도 수행해야 하는 동작에 초점을 맞출 수 있도록 도와줍니다.

```c
// read the current status of the fan from hardware
int fan_current_admin = read_current_fan_state(); // 1: enabled, 0: disabled
// store the current status to the current fan admin 
// for fan-control initialization.
ydb_write(datablock,
    "system:\n"
    "  fan-control:\n"
    "   admin: %d\n",
    fan_current_admin);
```

```c
// read the current status of the fan admin
int fan_config_admin = 1;
int fan_current_admin = 0;
ydb_read(datablock,
    "system:\n"
    "  fan-control:\n"
    "   admin: %d\n",
    &fan_current_admin);
if (!fan_current_admin && fan_config_admin)
{
    // enable the target fan
}
else if (...) // ...
```

### Less consideration for the data structure

우리는 일반적으로 사용할 data structure를 아래와 같이 type으로 정의하고, 이를 이미 구현되거나 또는 구현할 다른 data structure 안에 포함할 것입니다.


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

만약, 이러한 정의없이 단순히 YAML로 fan 정보를 간단히 표현한다면, 다음과 같을 겁니다.

```yaml
system:
  fan-control:
    1:
      admin: true
      config_speed: 100
      current_speed: 50
    2:
      admin: true
      config_speed: 200
      current_speed: 100
```

입력은 이렇게

조회는 이렇게 ...


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
