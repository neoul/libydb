#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "ymap.h"

char *exstr[] = {
"interface",
"interface/name",
"interface/description",
"interface/type",
"interface/enabled",
"ietf-ip/ipv4",
"ietf-ip/ipv4/enabled",
"ietf-ip/ipv4/forwarding",
"ietf-ip/ipv4/mtu",
"ietf-ip/ipv4/address",
"ietf-ip/ipv4/address/ip",
"ietf-ip/ipv4/address/subnet",
"ietf-ip/ipv4/address/subnet/prefix-length",
"ietf-ip/ipv4/address/subnet/prefix-length/prefix-length",
"ietf-ip/ipv4/address/subnet/netmask",
"ietf-ip/ipv4/neighbor",
"ietf-ip/ipv4/neighbor/ip",
"ietf-ip/ipv4/neighbor/link-layer-address",
"ietf-ip/ipv6",
"ietf-ip/ipv6/enabled",
"ietf-ip/ipv6/forwarding",
"ietf-ip/ipv6/mtu",
"ietf-ip/ipv6/address",
"ietf-ip/ipv6/address/ip",
"ietf-ip/ipv6/address/prefix-length",
"ietf-ip/ipv6/neighbor",
"ietf-ip/ipv6/neighbor/ip",
"ietf-ip/ipv6/neighbor/link-layer-address",
"ietf-ip/ipv6/dup-addr-detect-transmits",
"ietf-ip/ipv6/autoconf",
"ietf-ip/ipv6/autoconf/create-global-addresses",
"ethernet",
"ethernet/auto-negotiation",
"ethernet/auto-negotiation/enable",
"ethernet/duplex",
"ethernet/speed",
"ethernet/flow-control",
"ethernet/flow-control/pause",
"ethernet/flow-control/pause/direction",
"ethernet/flow-control/force-flow-control",
};

static int callbackn;
int callback(void *key, void *data, void *addition)
{
    printf("%d:%s:%s\n", callbackn, (char *)addition, (char *)data);
    callbackn++;
    return 0;
}

int main()
{
    int n;
    ymap *map;
    printf("\n[ymap_create]\n");
    map = ymap_create((ytree_cmp)strcmp, NULL);
    
    printf("\n[ymap_insert_front]\n");
    for (n=0; n < (sizeof(exstr)/sizeof(char *)); n++)
    {
        char *item = strdup(exstr[n]);
        item = ymap_insert_front(map, item, item);
        if (item)
            free(item);
    }
    printf("\n[ymap_size]\n");
    printf("size=%d\n", ymap_size(map));
    
    printf("\n[ymap_traverse (unordered)]\n");
    ymap_traverse(map, callback, "TRV");

    printf("\n[ymap_search_nearby]\n");
    char *key, *nearkey;
    char *data;
    key = "ietf-ip/ipv6/";
    nearkey = NULL;
    data = ymap_search_nearby(map, key, (void **)nearkey, 0);
    printf("key=%s, data=%s\n", key, data);

    callbackn =0;
    printf("\n[ymap_traverse (ordered)]\n");
    ymap_traverse_order(map, callback, "TRV");

    printf("\n[ymap pop]\n");
    char *popstr = ymap_pop_front(map, NULL, NULL);
    printf("pop (front) str=%s\n", popstr);
    free(popstr);
    popstr = ymap_pop_tail(map, NULL, NULL);
    printf("pop (tail) str=%s\n", popstr);
    free(popstr);

    printf("\n[ymap loop]\n");
    
    ymap_iter *iter;
    n = 0;
    iter = ymap_first(map);
    for (; !ymap_done(map, iter); iter = ymap_next(map, iter))
    {
        (char *)ymap_data(iter);
        (char *)ymap_key(iter);
        printf("%d:%s:%s\n", n, "loop", (char *)ymap_data(iter));
        n++;
    }

    printf("\n[ymap_insert_back]\n");
    // move the head to the tail of the ymap.
    char *newstr = strdup(exstr[n-1]);
    char *oldstr = ymap_insert_back(map, newstr, newstr);
    assert(oldstr);
    free(oldstr);
    iter = ymap_last(map);
    printf("%d:%s:%s\n", n, "last", (char *)ymap_data(iter));

    printf("\n[ymap_delete]\n");

    char *delstr = ymap_delete(map, "ethernet/speed");
    assert(delstr);
    free(delstr);

    printf("\n[ymap_size]\n");
    printf("size=%d\n", ymap_size(map));

    printf("\n[ymap_search]\n");

    char *searchstr = ymap_search(map, "ethernet/speed");
    assert(!searchstr);

    searchstr = ymap_search(map, "ietf-ip/ipv6/autoconf");
    assert(searchstr);

    printf("\n[ymap_remove]\n");

    n = 0;
    iter = ymap_first(map);
    for (; !ymap_done(map, iter); iter = ymap_next(map, iter))
    {
        if (strncmp(ymap_key(iter), "ethernet/flow-control", strlen("ethernet/flow-control")) == 0 ||
            strncmp(ymap_key(iter), "ietf-ip/ipv6", strlen("ietf-ip/ipv6")) == 0)
        {
            printf("%d:%s:%s\n", n, "del", (char *)ymap_key(iter));
            iter = ymap_remove(map, iter, free);
            n++;
        }
    }

    printf("\n[ymap_size]\n");
    printf("size=%d\n", ymap_size(map));

    printf("\n[ymap_insert (success)]\n");
    iter = ymap_index(map, 3);
    char *insert_item = strdup("new inserted data");
    iter = ymap_insert(map, iter, insert_item, insert_item);
    assert(iter);

    printf("\n[ymap_insert (failure)]\n");
    insert_item = strdup("new inserted data");
    iter = ymap_insert(map, iter, insert_item, insert_item);
    assert(!iter);
    free(insert_item);

    printf("\n[ymap_traverse]\n");
    callbackn = 0;
    ymap_traverse_order(map, callback, "TRV-ORDER");

    ymap_destroy_custom(map, free);
    return 0;
}

