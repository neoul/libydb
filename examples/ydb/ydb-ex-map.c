// ydb-ex-list.c

#include <stdio.h>
#include <string.h>
#include <ylog.h>
#include <ydb.h>

char *yaml_map1 =
"interfaces:\n"
"  interface:\n"
"    ge1:\n"
"      statistics:\n"
"        discontinuity-time: 2018-12-13T11:03:52+09:00\n"
"        in-broadcast-pkts: 13462\n"
"        in-discards: 13462\n"
"        in-errors: 0\n"
"        in-multicast-pkts: 0\n"
"        in-octets: 81433798\n"
"        in-unicast-pkts: 350463\n"
"        in-unknown-protos: 0\n"
"        out-broadcast-pkts: 40378\n"
"        out-discards: 0\n"
"        out-errors: 0\n"
"        out-multicast-pkts: 0\n"
"        out-octets: 47815244\n"
"        out-unicast-pkts: 350459\n"
"      description: ge1 interface!\n"
"      enabled: false\n"
"      ipv4:\n"
"        address:\n"
"          192.168.1.10:\n"
"            prefix-length: 24\n"
"            secondary: false\n"
"      name: ge1\n";

char *yaml_map2 =
"interfaces:\n"
"  interface:\n"
"    ge2:\n"
"      statistics:\n"
"      description: ge2 interface!\n"
"      enabled: false\n"
"      ipv4:\n"
"        address:\n"
"          192.168.2.10:\n"
"            prefix-length: 24\n"
"            secondary: false\n"
"      name: ge2\n";

char *yaml_map3 =
"interfaces:\n"
"  interface:\n"
"    ge2:\n"
"      statistics:\n"
"      description: ge2 interface!\n"
"      enabled: false\n"
"      ipv4:\n"
"      name: ge2\n";

int main(int argc, char *argv[])
{
    ydb *datablock;
    datablock = ydb_open("demo");
    
    ylog_severity = YLOG_DEBUG;
    fprintf(stdout, "[merge - ydb_parses]\n");
    ydb_parses(datablock, yaml_map1, strlen(yaml_map1));
    fprintf(stdout, "[merge - ydb_write]\n");
    ydb_write(datablock, yaml_map2);
    
    fprintf(stdout, "[merge - ydb_delete]\n");
    ydb_delete(datablock, yaml_map3);
    
    ydb_dump(datablock, stdout);

    ydb_close(datablock);
    return 0;
}