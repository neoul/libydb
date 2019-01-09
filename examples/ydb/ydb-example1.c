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