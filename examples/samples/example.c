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

void ymldb_notify_callback(void *usr_data, struct ymldb_callback_data *cdata)
{
    int i;
    printf("\n");
    if(cdata->deleted || cdata->unregistered)
        printf(" [callback for%s%s]\n", 
            cdata->deleted?" del":"",
            cdata->unregistered?" unreg":"");

    if(!cdata->unregistered && !cdata->deleted)
        printf(" [callback for merge]\n");
    
    printf("\t- %s(%s)\n", __FUNCTION__, (char *) (usr_data?usr_data:""));
    
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

    // ymldb iterator example in ymldb notify callback
    struct ymldb_iterator *iter = 
        ymldb_iterator_alloc2(cdata->keys_num, cdata->keys);
    const char *key = ymldb_iterator_get_key(iter);
    printf(" ###### ITER=%s\n", key);
    ymldb_iterator_free(iter);

    if(!cdata->unregistered && !cdata->deleted) {
        ymldb_notify_callback_register2(ymldb_notify_callback, "SUB", cdata->keys_num, cdata->keys );
    }
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

    // ymldb_log_set(YMLDB_LOG_LOG, NULL);
    
    printf("\n\n[ymldb_create(interfaces)]\n");
    // create ymldb for interface.
    int res = ymldb_create("interfaces", YMLDB_FLAG_NONE);
    if (res < 0)
    {
        fprintf(stderr, "ymldb create failed.\n");
        return -1;
    }

    // ymldb_notify_callback_register(ymldb_notify_callback, "interfaces-cb", "interfaces");
    // ymldb_notify_callback_register(ymldb_notify_callback, "interface-cb", "interfaces", "interface");
    // ymldb_notify_callback_register(ymldb_notify_callback, "ge1-cb", "interfaces", "interface", "ge1");

    // read ymldb from a file.
    printf("\n\n[ymldb_run_with_fd]\n");
    int infd = open("interfaces.yml", O_RDONLY, 0644);
    ymldb_run_with_fd("interfaces", infd, 0);
    close(infd);

    printf("\n\n[ymldb_dump]\n");
    ymldb_dump(stdout, NULL);

    // use ymldb iterator
    printf("\n\n[ymldb_iterator]\n");
    const char *key;
    struct ymldb_iterator *iter = 
        ymldb_iterator_alloc("interfaces", "interface", "ge1", "mtu");
    key = ymldb_iterator_up(iter);
    do
    {
        printf(" key=%s\n", key);
    } while((key = ymldb_iterator_next(iter)) != NULL);

    printf("\n\n[ymldb_iter_lookup]\n");
    key = ymldb_iterator_lookup(iter, "ge2");
    printf("ymldb_iterator_lookup key=%s\n", key);

    ymldb_iterator_free(iter);
    iter = NULL;

    // get data from ymldb.
    printf("\n\n[ymldb_read]\n");
    char *value = ymldb_read("interfaces", "interface", "ge1", "operstatus");
    fprintf(stdout, "ymldb_read(ge1 operstatus=%s)\n", value);
    
    char *keys[10] = {"interfaces", "interface", "ge1", "mtu" };
    value = ymldb_read2(4, keys);

    fprintf(stdout, "ymldb_read2(ge1 operstatus=%s)\n", value);

    // get data from ymldb using ymldb_pull.
    printf("\n\n[ymldb_pull]\n");
    int mtu = 0;
    char operstatus_str[32] = {0};
    ymldb_pull("interfaces",
               "interface:\n"
               "  ge2:\n"
               "    operstatus: %s\n"
               "    mtu: %d\n",
               operstatus_str, &mtu);
    fprintf(stdout, "ge2 mtu=%d\n", mtu);
    fprintf(stdout, "ge2 operstatus=%s\n", operstatus_str);

    printf("\n\n[ymldb_write]\n");
    ymldb_write("interfaces", "interface", "ge2", "speed", "1G");

    printf("\n\n[ymldb_get]\n");
    // read ymldb data (yaml format string) to OUTPUT stream.
    ymldb_get(stdout, "interfaces", "interface", "ge2");

    printf("\n\n[ymldb_create(system)]\n");
    res = ymldb_create("system", YMLDB_FLAG_NONE);
    if (res < 0)
    {
        fprintf(stderr, "ymldb create failed.\n");
        return -1;
    }

    printf("\n\n[ymldb_push(system)]\n");
    res = ymldb_push("system",
                         "product: %s\n"
                         "serial-number: %s\n"
                         "code: 1223\n",
                         "G.FAST-HN5124D",
                         "HN5124-S100213124");
    if (res < 0)
    {
        fprintf(stderr, "fail to push data.\n");
    }
    
    printf("\n\n[ymldb_write(system)]\n");
    res = ymldb_write("system", "product", "HA-805");
    if (res < 0)
    {
        fprintf(stderr, "fail to write data.\n");
    }

    printf("\n\n[ymldb_get(system)]\n");
    ymldb_get(stdout, "system");
    
    // delete an ymldb.
    printf("\n\n[ymldb_delete(rx-octet)]\n");
    ymldb_delete("interfaces", "interface", "ge1", "rx-octet");
    
    // printf("\n\n[ymldb_delete(interfaces)]\n");
    // // it would be failed to remove unknown ymldb node.
    // res = ymldb_delete("interfaces", "interface", "ge1");
    // if (res < 0)
    // {
    //     fprintf(stderr, "failed to delete.\n");
    // }
    

    // this would return NULL.
    printf("\n\n[ymldb_read(system)]\n");
    value = ymldb_read("system");
    fprintf(stdout, "read data = %s\n", value);

    // ymldb_read read the value of a leaf!
    printf("\n\n[ymldb_read(system)]\n");
    value = ymldb_read("system", "product");
    fprintf(stdout, "read data = %s\n", value);

    printf("\n\n[ymldb_dump]\n");
    ymldb_dump(stdout, NULL);
    
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
