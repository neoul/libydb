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

    // serve --
    // Run serve() in the main loop if YDB IPC channel is used.
    // serve() updates the local YDB instance using the received YAML data from remotes.
    ydb_res serve(int timeout);

    // // Ynode &find(std::string path);
    
    // ydb_res write(std::string yaml);
    // ydb_res read(std::string &filter);
    // int print(std::string filter);
    // int print();
    // std::string to_string();
};

#endif
