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

// int ymldb_usr_callback(void *usr_data, struct ymldb_iterator *iter, int deleted)
// {
//     const char *key = ymldb_iterator_get_key(iter);
//     const char *value = ymldb_iterator_get_value(iter);
//     printf("======================================\n");
//     printf("\n");
//     printf(" %s(%s)\n", __FUNCTION__, (char *)usr_data);
//     printf("\n");
//     printf(" - current key: %s, value %s\n", key?key:"", value?value:"");

//     // go to subtree.
//     key = ymldb_iterator_down(iter);
//     if(key) {
//         do {
//             // print all sub nodes
//             key = ymldb_iterator_get_key(iter);
//             value = ymldb_iterator_get_value(iter);
//             printf("  - current key: %s, value: %s\n", key?key:"", value?value:"");
//             struct ymldb_iterator sub_iter;
//             ymldb_iterator_copy(&sub_iter, iter);
//             key = ymldb_iterator_down(&sub_iter);
//             if(key) {
//                 do {
//                     key = ymldb_iterator_get_key(&sub_iter);
//                     value = ymldb_iterator_get_value(&sub_iter);
//                     printf("   - current key: %s, value: %s\n", key?key:"", value?value:"");
//                 } while((key = ymldb_iterator_next(&sub_iter)));
//             }
//         } while((key = ymldb_iterator_next(iter)));
//     }
    
//     printf("\n");
//     printf("======================================\n");
//     if(deleted)
//         ymldb_callback_register(ymldb_usr_callback, "my-data", "interface");
//     return 0;
// }

void ymldb_usr_callback(void *usr_data, struct ymldb_callback_data *callback_data)
{
    int i;
    printf("\n");
    if(callback_data->deleted)
        printf(" [callback for %s]\n", callback_data->deleted?"deleted":"");
    if(callback_data->unregistered)
        printf(" [callback for %s]\n", callback_data->unregistered?"unregistered":"");
    if(!callback_data->unregistered && !callback_data->deleted)
        printf(" [callback for merged]\n");
    printf("\t-");
    for(i=0; i<callback_data->keys_num; i++) {
        printf(" %s", callback_data->keys[i]);
    }
    if(callback_data->value) {
        printf(" = %s", callback_data->value);
    }
    printf("\n");
    printf("\t- %s(%s)\n\n", __FUNCTION__, (char *) usr_data);
    // if(callback_data->unregistered)
    //     ymldb_callback_register(ymldb_usr_callback, "my-data", "interfaces", "interface");
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
    int res = ymldb_create("interfaces", YMLDB_FLAG_NONE);
    if (res < 0)
    {
        fprintf(stderr, "ymldb create failed.\n");
        return -1;
    }

    // ymldb_callback_register(ymldb_usr_callback, "abc", "interface", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13");
    
    ymldb_callback_register(ymldb_usr_callback, "my-data", "interfaces", "interface");

    ymldb_dump_all(stdout, NULL);

    // read ymldb from a file.
    int infd = open("ymldb-interface.yml", O_RDONLY, 0644);
    ymldb_run_with_fd("interfaces", infd, 0);
    close(infd);

    // use ymldb iterator
    const char *key;
    struct   ymldb_iterator *iter = ymldb_iterator_alloc("interfaces", "interface", "ge1", "mtu");
    key = ymldb_iterator_up(iter);
    do
    {
        printf("key=%s\n", key);
    } while((key = ymldb_iterator_next(iter)) != NULL);

    ymldb_iterator_free(iter);
    iter = NULL;


    ymldb_dump_all(stdout, NULL);

    // get data from ymldb.
    char *value = ymldb_read("interfaces", "interface", "ge1", "operstatus");
    fprintf(stdout, "ymldb_read(ge1 operstatus=%s)\n", value);
    
    char *keys[10] = {"interfaces", "interface", "ge1", "mtu" };
    value = ymldb_read2(4, keys);

    fprintf(stdout, "ymldb_read2(ge1 operstatus=%s)\n", value);

    // get data from ymldb using ymldb_pull.
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

    // read ymldb data (yaml format string) to OUTPUT stream.
    ymldb_get(stdout, "interfaces", "interface", "ge2");

    res = ymldb_create("system", YMLDB_FLAG_NONE);
    if (res < 0)
    {
        fprintf(stderr, "ymldb create failed.\n");
        return -1;
    }

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
    

    res = ymldb_write("system", "product", "HA-805");
    if (res < 0)
    {
        fprintf(stderr, "fail to write data.\n");
    }
    
    // delete an ymldb.
    ymldb_delete("interfaces", "interface", "ge1", "rx-octet");
    
    // it would be failed to remove unknown ymldb node.
    res = ymldb_delete("interfaces", "interface", "ge3");
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

    ymldb_dump_all(stdout, NULL);
    ymldb_get(stdout, "interfaces");
    ymldb_callback_unregister("interfaces", "interface");


    ymldb_destroy_all();
    // ymldb_destroy("interface");
    // ymldb_destroy("system");
    
    ymldb_dump_all(stdout, NULL);

    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    fprintf(stdout, "time elapsed: %f\n", cpu_time_used);
    return 0;
}

int main(int argc, char *argv[])
{
    return ymldb_test();
}
