// ydb-ex-map.c

#include <stdio.h>
#include <string.h>
#include <ylog.h>
#include <ydb.h>

// yaml block mapping format
const char *yaml_map1 =
    "system:\n"
    " interfaces:\n"
    "  %s:\n"
    "   ipaddr:\n"
    "    inet: %s\n"
    "    netmask: %s\n";

const char *yaml_map2 =
    "system:\n"
    " interfaces:\n"
    "  %s:\n"
    "   ipaddr:\n";

int main(int argc, char *argv[])
{
    ydb *datablock;
    datablock = ydb_open("demo");

    fprintf(stdout, "\n");
    fprintf(stdout, "YDB example for YAML mapping (list)\n");
    fprintf(stdout, "=============================\n");

    // ylog_severity = YLOG_DEBUG;
    fprintf(stdout, "\n[ydb_write]\n");

    ydb_write(datablock, yaml_map1, "eth0", "172.17.0.1", "255.255.0.0");
    ydb_write(datablock, yaml_map1, "eth1", "172.18.0.1", "255.255.0.0");
    ydb_dump(datablock, stdout);

    fprintf(stdout, "\n[ydb_write] update eth0\n");
    ydb_write(datablock, yaml_map1, "eth1", "192.168.0.10", "255.255.0.0");

    // print all the node of the target yaml data.
    ydb_fprintf(stdout, datablock,
                "system:\n"
                " interfaces:\n"
                "  %s:\n",
                "eth1");

    fprintf(stdout, "\n[ydb_read] read the inet of eth1\n");
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

    fprintf(stdout, "\n[ydb_delete] delete ipaddr of eth1\n");
    ydb_delete(datablock, yaml_map2, "eth1");
    ydb_dump(datablock, stdout);

    fprintf(stdout, "\n[ydb_delete] add eth1 data again using path\n");
    ydb_path_write(datablock, "/system/interfaces/eth1/ipaddr/inet=192.168.1.111");
    ydb_path_write(datablock, "/system/interfaces/eth1/ipaddr/netmask=255.255.0.0");

    fprintf(stdout, "\n[ydb_path_read] read ipaddr of eth1 using path\n");
    const char *data;
    data = ydb_path_read(datablock, "/system/interfaces/eth0/ipaddr/inet");
    fprintf(stdout, "/system/interfaces/eth0/ipaddr/inet=%s\n", data);

    fprintf(stdout, "\n[ydb_path_read] delete ipaddr of eth1 using path\n");
    ydb_path_delete(datablock, "/system/interfaces/eth1/ipaddr");

    ydb_dump(datablock, stdout);
    ydb_close(datablock);
    return 0;
}