// ydb-example2.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ydb.h>
#include <ylog.h>


ydb_res ydb_trv(ydb *datablock, ynode *cur, ynode *src, FILE *fp, int printlevel)
{
    int level;
    char *path;
    level = ydb_level(src, cur);
    printf("level %d, printlevel=%d\n", level, printlevel);
    if (level <= 0 || level > printlevel)
        return YDB_OK;
    path = ydb_path_nodes(src, cur, NULL);
    if (path)
    {
        fprintf(fp, "%s\n", (char *)path);
        free(path);
    }
    return YDB_OK;
}

#include <string.h>
// flags: val-only, leaf-only, leaf-first, no-values
char *ydb_path_list(ydb *datablock, int depth, char *path)
{
    ydb_res res = YDB_OK;
    ynode *src = NULL;
    char *buf = NULL;
    size_t buflen = 0;
    FILE *fp = NULL;

    if (!datablock)
        return NULL;

    fp = open_memstream(&buf, &buflen);
    if (!fp)
        return NULL;

    src = ydb_search(datablock, "%s", path);
    if (!src)
        goto failed;
    
    char *rbuf = ydb_ynode2yaml(datablock, src, NULL);
    printf("rbuf=\n%s\n==\n", rbuf);
    if (rbuf)
        free(rbuf);
    res = ydb_traverse(datablock, src,
                       (ydb_traverse_callback)ydb_trv,
                       NULL, 3, src, fp, depth);
    if (YDB_FAILED(res))
        goto failed;
    fclose(fp);
    if (buf)
    {
        if (buflen > 0)
            return buf;
        free(buf);
    }
failed:
    if (fp)
        fclose(fp);
    if (buf)
        free(buf);
    return NULL;
}


ydb_res ydb_traverse_cb(ydb *datablock, ynode *cur, void *U1)
{
    char *path = ydb_path_and_value(datablock, cur, NULL);
    fprintf(stdout, "%s path=%s\n", (char *) U1, path);
    if (path)
        free(path);
    return YDB_OK;
}

int main(int argc, char *argv[])
{
    ylog_level = YLOG_DEBUG;
    ydb *block1;
    block1 = ydb_open("my-block");

    ydb *block2;
    ynode *found = NULL;
    block2 = ydb_get("my-block", &found);

    printf("block1: %s\n", ydb_name(block1));
    printf("block2: %s\n", ydb_name(block2));
    printf("block1=%p, block2=%p\n", block1, block2);

    char ifname[32] = "1/1";
    char ifdesc[64] = "ethernet interface";
    int ifenable = 1;
    char ifip[32] = "10.1.1.10/16";

    ydb_write(block1,
              "interfaces:\n"
              "  interface[name=%s]:\n"
              "    description: %s\n"
              "    enabled: %s\n"
              "    ip: %s\n"
              "    name: %s\n",
              ifname,
              ifdesc,
              ifenable ? "true" : "false",
              ifip,
              ifname);
    
    ydb_path_write(block1, "/interfaces/interface[name=1/1]/statistics/in-packet=%d", 100);
    const char *inpacket = ydb_path_read(block1, "/interfaces/interface[name=1/1]/statistics/in-packet");
    printf("in-packet=%s\n", inpacket);

    ydb_dump(block1, stdout);

    int n;
    memset(ifdesc, 0x0, sizeof(ifdesc));
    memset(ifip, 0x0, sizeof(ifip));

    n = ydb_read(block1,
                 "interfaces:\n"
                 "  interface[name=1/1]:\n"
                 "    ip: %s\n"
                 "    description: %s\n",
                 ifip,
                 ifdesc);
    printf("ydb_read: n=%d ip=%s, desc=%s\n", n, ifip, ifdesc);

    ynode *node;
    node = ydb_search(block1, "interfaces/interface[name=%s]", ifname);
    memset(ifdesc, 0x0, sizeof(ifdesc));
    memset(ifip, 0x0, sizeof(ifip));
    ydb_retrieve(node,
               "ip: %s\n"
               "description: %s\n",
               ifip,
               ifdesc);
    printf("ydb_retrieve: n=%d ip=%s, desc=%s\n", n, ifip, ifdesc);

    block2 = ydb_open("dump");
    FILE *fp = fopen("examples/yaml/ydb-input.yaml", "r");
    ydb_parse(block2, fp);
    fclose(fp);
    ydb_dump(block2, stdout);
    ydb_close(block2);

    const char *iftype;
    ydb_path_write(block1, "/interfaces/interface[name=1/1]/type=%s", "ethernet");
    iftype = ydb_path_read(block1, "/interfaces/interface[name=1/1]/type");
    printf("iftype=%s\n", iftype);

    node = ydb_top(block1);
    printf("ydb name (ydb_top) = %s\n", ydb_key(node));
    node = ydb_find_child(node, "interfaces");
    printf("found child (ydb_find_child) = %s\n", ydb_key(node));
    node = ydb_down(node);
    printf("first child (ydb_down) = %s\n", ydb_key(node));
    node = ydb_find(node, "enabled");
    printf("enabled=%s\n", ydb_value(node));
    node = ydb_next(node);
    printf("next neighbor (ydb_next) = %s\n", ydb_value(node));
    node = ydb_up(node);
    printf("parent (ydb_up) = %s\n", ydb_key(node));

    node = ydb_top(block1);
    ydb_traverse(block1, node, (ydb_traverse_callback) ydb_traverse_cb, "val-only", 1, "traverse");
    char *dummy = ydb_path_list(block1, 1, "/interfaces/interface[name=1/1]");
    if (dummy)
    {
        printf("%s", dummy); 
        free(dummy);
    }
    ydb_close(block1);
    return 0;
}
