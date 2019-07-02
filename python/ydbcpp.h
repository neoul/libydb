#ifndef __YDB_HPP__
#define __YDB_HPP__

#include "ydb.h"
// g++ -g -v -Wall ydb.cpp -lydb -lyaml

// class Ynode
// {
//     bool Ynode::operator!() const;
// private:
//     ynode *node;
// public:
//     Ynode up();
//     Ynode down();
//     Ynode left();
//     Ynode right();
//     std::string to_string();
// };

class Ydb
{
    friend std::ostream &operator<<(std::ostream &c, Ydb &Y);
    friend std::istream &operator>>(std::istream &is, Ydb &Y);

private:
    ydb *db;

public:
    Ydb(char *name);
    ~Ydb();

    // write --
    // write the YAML data
    ydb_res write(char *yaml);

    // remove --
    // delete the YAML data
    ydb_res remove(char *yaml);

    // The return value of get() must be free.
    char *get(char *filter);
    char *get();

    // path_write --
    // write the data using /path/to/data=value
    ydb_res path_write(char *path);

    // path_remove --
    // delete the data using /path/to/data
    ydb_res path_remove(char *path);

    // path_get --
    // get the data (value only) using /path/to/data
    const char *path_get(char *path);

    std::string to_string();

    ydb_res connect(char* addr, char* flags);
    ydb_res disconnect(char* addr);

    // ydb_is_connected --
    // Check the YDB IPC channel connected or not.
    int is_connected(char *addr);

    // fd --
    // Return the fd (file descriptor) opened for YDB IPC channel.
    int fd();

    // serve --
    // Run serve() in the main loop if YDB IPC channel is used.
    // serve() updates the local YDB instance using the received YAML data from remotes.
    ydb_res serve(int timeout);


    // return the path of the node. (the path must be free.)
    char *path(ynode *node);
    // return the path of the node. (the path must be free.)
    char *path_and_value(ynode *node);
    // return the node in the path (/path/to/data).
    ynode *search(char *path);
    // return the top node of the yaml data block.
    ynode *top();
    // return 1 if the node has no child.
    int empty(ynode *node);
    // Return the found node by the path
    ynode *find(ynode *base, char *path);
    // return the parent node of the node.
    ynode *up(ynode *node);
    // return the first child node of the node.
    ynode *down(ynode *node);
    // return the previous sibling node of the node.
    ynode *prev(ynode *node);
    // return the next sibling node of the node.
    ynode *next(ynode *node);
    // return the first sibling node of the node.
    ynode *first(ynode *node);
    // return the last sibling node of the node.
    ynode *last(ynode *node);
    // return node tag
    const char *tag(ynode *node);
    // Return node value if that is a value node.
    const char *value(ynode *node);
    // Return the key of the node when the parent is a map (hasp).
    const char *key(ynode *node);
    // Return the index of the node when the parent is a seq (list).
    int index(ynode *node);

    // Return the level of two nodes.
    int level(ynode *base, ynode *node);

    // Return the list of path/to/data
    char *path_list(int depth, char *path);
};

#endif
