#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "ymldb.h"

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
        "...\n"
        "#2 empty message\n"
        "---\n"
        "...\n",
        f);
    fclose(f);

    struct ymldb_cb *cb;

    // create ymldb for interface.
    cb = ymldb_create("interface", YMLDB_FLAG_NONE);
    if (!cb)
    {
        fprintf(stderr, "ymldb create failed.\n");
        return -1;
    }

    // read ymldb from a file.
    int infd = open("ymldb-interface.yml", O_RDONLY, 0644);
    ymldb_run(cb, infd, 0);
    close(infd);

    // get data from ymldb.
    char *value = ymldb_read(cb, "interface", "ge1", "operstatus");
    fprintf(stdout, "ge1 operstatus=%s\n", value);

    // get data from ymldb using ymldb_pull.
    int mtu = 0;
    char operstatus_str[32];
    ymldb_pull(cb,
               "interface:\n"
               "  ge2:\n"
               "    operstatus: %s\n"
               "    mtu: %d\n",
               operstatus_str, &mtu);
    fprintf(stdout, "ge2 mtu=%d\n", mtu);
    fprintf(stdout, "ge2 operstatus=%s\n", operstatus_str);

    // read ymldb data (yaml format string) to OUTPUT stream.
    ymldb_get(cb, stdout, "interface", "ge2");

    cb = ymldb_create("system", YMLDB_FLAG_NONE);
    int res = ymldb_push(cb,
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

    res = ymldb_write(cb, "system", "product", "HA-805");
    if (res < 0)
    {
        fprintf(stderr, "fail to write data.\n");
    }

    // change ymldb cb
    cb = ymldb_cb("interface");

    // delete an ymldb.
    ymldb_delete(cb, "interface", "ge1", "rx-octet");

    // it would be failed to remove unknown ymldb node.
    res = ymldb_delete(cb, "interface", "ge3");
    if (res < 0)
    {
        fprintf(stderr, "failed to delete.\n");
    }

    cb = ymldb_cb("system");

    // this would return NULL.
    value = ymldb_read(cb, "system");
    fprintf(stdout, "read data = %s\n", value);

    // ymldb_read read the value of a leaf!
    value = ymldb_read(cb, "system", "product");
    fprintf(stdout, "read data = %s\n", value);

    ymldb_dump_all(stdout);
    ymldb_destroy(ymldb_cb("interface"));
    ymldb_destroy(ymldb_cb("system"));
    ymldb_dump_all(stdout);
    return 0;
}

int main(int argc, char *argv[])
{
    ymldb_test();
}
