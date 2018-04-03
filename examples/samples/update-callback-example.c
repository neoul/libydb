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

void ymldb_update_callback(void *usr_data, struct ymldb_callback_data *cdata)
{
    int i;
    static int index = 0;
    static int mtu[] = {256, 512, 1024, 1518};
    static int rx_octet[] = {1000, 2000, 3000, 4000};
    static int tx_octet[] = {5000, 6000, 7000, 8000};

    printf("\n");
    printf("\t- KEYS(1):");
    for(i=0; i<cdata->keys_num; i++) {
        printf(" %s", cdata->keys[i]);
    }
    if(cdata->value) {
        printf(" = %s", cdata->value);
    }
    printf("\n");

    printf("\t- KEYS(2):");
    for(i=cdata->keys_level; i< cdata->keys_num; i++) {
        printf(" %s", cdata->keys[i]);
    }
    if(cdata->value) {
        printf(" = %s", cdata->value);
    }
    printf("\n");

    printf("\t- keys_num=%d, keys_level=%d\n\n",cdata->keys_num, cdata->keys_level);

    ymldb_push("interfaces",
        "interface:\n"
        "  ge3:\n"
        "    rx-octet: %d\n"
        "    tx-octet: %d\n"
        "    mtu: %d\n"
        ,
        rx_octet[index],
        tx_octet[index],
        mtu[index]
    );
    index = (index + 1)%4;
}

int ymldb_test()
{
    // write a file for a ymldb stream input.
    FILE *f = fopen("interfaces.yml", "w");
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
    int res = ymldb_create("interfaces", YMLDB_FLAG_NONE);
    if (res < 0)
    {
        fprintf(stderr, "ymldb create failed.\n");
        return -1;
    }

    // read ymldb from a file.
    int infd = open("interfaces.yml", O_RDONLY, 0644);
    ymldb_run_with_fd("interfaces", infd, 0);
    close(infd);

    ymldb_update_callback_register(ymldb_update_callback, "USR-DATA", "interfaces", "interface", "ge3");
    ymldb_dump(stdout, NULL);

    // read ymldb data (yaml format string) to OUTPUT stream.
    ymldb_get(stdout, "interfaces", "interface");
    ymldb_get(stdout, "interfaces", "interface", "ge3");
    ymldb_get(stdout, "interfaces", "interface", "ge3", "mtu");

    ymldb_callback_unregister("interfaces", "interface", "ge3");
    ymldb_destroy_all();
    // ymldb_destroy("interface");
    // ymldb_destroy("system");
    
    ymldb_dump(stdout, NULL);

    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    fprintf(stdout, "time elapsed: %f\n", cpu_time_used);
    return 0;
}

int main(int argc, char *argv[])
{
    return ymldb_test();
}
