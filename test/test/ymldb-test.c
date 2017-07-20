#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <time.h>

#include "ymldb.h"

int interface_callback(void *usr_data, int deleted)
{
    printf("\n\n%s\n\n", (char *)usr_data);
    return 0;
}

int ymldb_test()
{
    // write a file for a ymldb stream input.
    FILE *f = fopen("ymldb-interface.yml", "w");
    if (!f)
    {
        fprintf(stderr, "%s", strerror(errno));
        return -1;
    }
    fputs(
        "%TAG !seq! ymldb:seq:11\n"
        "%TAG !merge! ymldb:op:merge\n"
        "---\n"
        "interface:\n"
        "  ge1:\n"
        "    mtu: 1500\n"
        "    operstatus: false\n"
        "    rx-octet: 10022\n"
        "    tx-octet: 2222\n"
        "    ip-addr: [192.168.44.1, 192.168.0.77]\n"
        "  ge2:\n"
        "    operstatus: false\n"
        "    rx-octet: 10022\n"
        "    tx-octet: 2222\n"
        "    mtu: 2000\n"
        "    desc: ge2 interface is used for WAN\n"
        "...\n"
        "%TAG !seq! ymldb:seq:12\n"
        "%TAG !merge! ymldb:op:merge\n"
        "---\n"
        "interface:\n"
        "  ge3:\n"
        "...\n"
        ,
        f);
    fclose(f);

    clock_t start, end;
    double cpu_time_used;
    start = clock();

    ymldb_log_set(YMLDB_LOG_LOG, NULL);
    
    // create ymldb for interface.
    int res = ymldb_create("interface", YMLDB_FLAG_NONE);
    if (res < 0)
    {
        fprintf(stderr, "ymldb create failed.\n");
        return -1;
    }

    // ymldb_callback_register(interface_callback, "abc", "interface", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13");
    ymldb_callback_register(interface_callback, "abc", "interface", "ge3");
    ymldb_dump_all(stdout);

    // read ymldb from a file.
    int infd = open("ymldb-interface.yml", O_RDONLY, 0644);
    ymldb_run_with_fd("interface", infd, 0);
    close(infd);

    ymldb_dump_all(stdout);

    // get data from ymldb.
    char *value = ymldb_read("interface", "ge1", "operstatus");
    fprintf(stdout, "ymldb_read(ge1 operstatus=%s)\n", value);
    char *keys[10] = {"interface", "ge1", "mtu" };
    value = ymldb_read2(3, keys);
    fprintf(stdout, "ymldb_read2(ge1 operstatus=%s)\n", value);

    // get data from ymldb using ymldb_pull.
    int mtu = 0;
    char operstatus_str[32] = {0};
    ymldb_pull("interface",
               "interface:\n"
               "  ge2:\n"
               "    operstatus: %s\n"
               "    mtu: %d\n",
               operstatus_str, &mtu);
    fprintf(stdout, "ge2 mtu=%d\n", mtu);
    fprintf(stdout, "ge2 operstatus=%s\n", operstatus_str);

    // read ymldb data (yaml format string) to OUTPUT stream.
    ymldb_get(stdout, "interface", "ge2");

    res = ymldb_create("system", YMLDB_FLAG_NONE);
    if (res < 0)
    {
        fprintf(stderr, "ymldb create failed.\n");
        return -1;
    }

    res = ymldb_push("system",
                         "system:\n"
                         "  product: %s\n"
                         "  serial-number: %s\n"
                         "  code: 1223\n",
                         "G.FAST-HN5124D",
                         "HN5124-S100213124");
    if (res < 0)
    {
        fprintf(stderr, "fail to push data.\n");
    }
    

    res = ymldb_write("system", "product", "HA-805");
    if (res < 0)
    {
        fprintf(stderr, "fail to write data.\n");
    }

    
    // delete an ymldb.
    ymldb_delete("interface", "ge1", "rx-octet");
    
    // it would be failed to remove unknown ymldb node.
    res = ymldb_delete("interface", "ge3");
    if (res < 0)
    {
        fprintf(stderr, "failed to delete.\n");
    }
    

    // this would return NULL.
    value = ymldb_read("system");
    fprintf(stdout, "read data = %s\n", value);

    // ymldb_read read the value of a leaf!
    value = ymldb_read("system", "product");
    fprintf(stdout, "read data = %s\n", value);

    ymldb_dump_all(stdout);
    // ymldb_callback_unregister("interface", "ge1");


    ymldb_destroy_all();
    // ymldb_destroy("interface");
    // ymldb_destroy("system");
    
    ymldb_dump_all(stdout);

    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    fprintf(stdout, "time elapsed: %f\n", cpu_time_used);
    return 0;
}

int main(int argc, char *argv[])
{
    return ymldb_test();
}
