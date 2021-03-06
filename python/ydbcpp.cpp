#include <stdlib.h>
#include <iostream>
#include <fstream>
#include "ylog.h"
#include "ynode.h"
#include "ydbcpp.h"

using namespace std;

Ydb::Ydb(char *name)
{
    db = ydb_open((char *)name);
}

Ydb::~Ydb()
{
    ydb_close(db);
}

ydb_res Ydb::write(char *yaml)
{
    return ydb_write(db, "%s", yaml);
}

ydb_res Ydb::remove(char *yaml)
{
    return ydb_delete(db, "%s", yaml);
}

ydb_res Ydb::path_write(char *path)
{
    return ydb_path_write(db, "%s", path);
}

ydb_res Ydb::path_remove(char *path)
{
    return ydb_path_delete(db, "%s", path);
}

const char *Ydb::path_get(char *path)
{
    return ydb_path_read(db, "%s", path);
}

char *Ydb::get(char *filter)
{
    FILE *fp;
    char *buf = NULL;
    size_t buflen = 0;
    fp = open_memstream(&buf, &buflen);
    if (!fp)
        return NULL;
    ydb_fprintf(fp, db, "%s", filter);
    fclose(fp);
    return buf;
}

char *Ydb::get()
{
    FILE *fp;
    char *buf = NULL;
    size_t buflen = 0;
    fp = open_memstream(&buf, &buflen);
    if (!fp)
        return NULL;
    ydb_path_fprintf(fp, db, "/");
    fclose(fp);
    return buf;
}

ydb_res Ydb::connect(char *addr, char *flags)
{
    return ydb_connect(db, addr, flags);
}

ydb_res Ydb::disconnect(char *addr)
{
    return ydb_disconnect(db, addr);
}

int Ydb::is_connected(char *addr)
{
    return ydb_is_connected(db, addr);
}

int Ydb::fd()
{
    return ydb_fd(db);
}

ydb_res Ydb::serve(int timeout)
{
    return ydb_serve(db, timeout);
}

std::string Ydb::to_string()
{

    char *buf = NULL;
    buf = get();
    if (buf)
    {
        std::string data = std::string(buf);
        free(buf);
        return data;
    }
    return std::string();
}

std::ostream &operator<<(std::ostream &c, Ydb &Y)
{
    c << Y.to_string();
    return c;
}

std::istream &operator>>(std::istream &c, Ydb &Y)
{
    std::string input;
    char sbuf[1024];
    do
    {
        c.getline(sbuf, sizeof(sbuf));
        if (c.gcount() > 0)
            input = input + sbuf + '\n';
        // cout << c.good() << c.eof() << c.fail() << c.bad() <<  "|";
    } while (c.good());
    Y.write((char *)input.c_str());
    return c;
}

// return the path of the node. (the path must be free.)
char *Ydb::path(ynode *node)
{
    return ydb_path(db, node, NULL);
}
// return the path of the node. (the path must be free.)
char *Ydb::path_and_value(ynode *node)
{
    return ydb_path_and_value(db, node, NULL);
}
// return the node in the path (/path/to/data).
ynode *Ydb::search(char *path)
{
    return ydb_search(db, "%s", path);
}
// return the top node of the yaml data block.
ynode *Ydb::top()
{
    return ydb_top(db);
}
// return 1 if the node has no child.
int Ydb::empty(ynode *node)
{
    return ydb_empty(node);
}
// Return the found node by the path
ynode *Ydb::find(ynode *base, char *path)
{
    return ydb_find(base, "%s", path);
}
// return the parent node of the node.
ynode *Ydb::up(ynode *node)
{
    return ydb_up(node);
}
// return the first child node of the node.
ynode *Ydb::down(ynode *node)
{
    return ydb_down(node);
}
// return the previous sibling node of the node.
ynode *Ydb::prev(ynode *node)
{
    return ydb_prev(node);
}
// return the next sibling node of the node.
ynode *Ydb::next(ynode *node)
{
    return ydb_next(node);
}
// return the first sibling node of the node.
ynode *Ydb::first(ynode *node)
{
    return ydb_first(node);
}
// return the last sibling node of the node.
ynode *Ydb::last(ynode *node)
{
    return ydb_last(node);
}
// return node tag
const char *Ydb::tag(ynode *node)
{
    return ydb_tag(node);
}
// Return node value if that is a value node.
const char *Ydb::value(ynode *node)
{
    return ydb_value(node);
}
// Return the key of the node when the parent is a map (hasp).
const char *Ydb::key(ynode *node)
{
    return ydb_key(node);
}
// Return the index of the node when the parent is a seq (list).
int Ydb::index(ynode *node)
{
    return ydb_index(node);
}

#include <string.h>
ydb_res ydb_trv(ydb *datablock, ynode *cur, ynode *src, FILE *fp, int printlevel)
{
    int level;
    char *path;
    if (!cur)
        return YDB_OK;
    level = ydb_level(src, cur);
    // printf("level %d, printlevel=%d\n", level, printlevel);
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
    res = ydb_traverse(datablock, src,
                       (ydb_traverse_callback)ydb_trv,
                       NULL, 3, src, fp, depth);
    if (YDB_FAILED(res))
        goto failed;
    fclose(fp);
    if (buf)
        return buf;
failed:
    if (fp)
        fclose(fp);
    return buf;
}

// Return the level of two nodes.
int Ydb::level(ynode *base, ynode *node)
{
    return ydb_level(base, node);
}

// Return the list of path/to/data
char *Ydb::path_list(int depth, char *path)
{
    return ydb_path_list(db, depth, path);
}