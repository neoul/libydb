#include <stdlib.h>
#include <iostream>
#include <fstream>
#include "ylog.h"
#include "ynode.h"
#include "ydbcpp.h"

using namespace std;

Ydb::Ydb(char *name)
{
    db = ydb_open((char *) name);
}

Ydb::~Ydb()
{
    ydb_close(db);
}

ydb_res Ydb::push(char *yaml)
{
    return ydb_write(db, "%s", yaml);
}
ydb_res Ydb::pop(char *yaml)
{
    return ydb_delete(db, "%s", yaml);
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

ydb_res Ydb::connect(char * addr, char * flags)
{
    return ydb_connect(db, addr, flags);
}

ydb_res Ydb::disconnect(char * addr)
{
    return ydb_disconnect(db, addr);
}

ydb_res Ydb::serve(int timeout)
{
    return ydb_serve(db, timeout);
}



// ydb_res Ydb::write(std::string yaml)
// {
//     return ydb_write(db, "%s", yaml.c_str());
// }

// int Ydb::print()
// {
//     return ydb_path_fprintf(stdout, db, "/");
// }

// int Ydb::print(std::string filter="")
// {
//     return ydb_fprintf(stdout, db, "%s", filter.c_str());
// }

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

std::ostream &operator <<(std::ostream &c, Ydb &Y)
{
    c << Y.to_string();
    return c;
}

std::istream &operator >>(std::istream &c, Ydb &Y)
{
    std::string input;
    char sbuf[1024];
    do {
        c.getline(sbuf, sizeof(sbuf));
        if (c.gcount() > 0)
            input = input + sbuf + '\n';
        // cout << c.good() << c.eof() << c.fail() << c.bad() <<  "|";
    } while (c.good());
    Y.push((char *) input.c_str());
    return c;
}
