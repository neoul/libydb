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

#define VARIABLE_NAME(NAME)  #NAME

int ymldb_file_rw_test()
{
    int res = 0;
    char *yfilename = "ymldb-file.yml";

    // ymldb_log_set(YMLDB_LOG_LOG, NULL);
    res = ymldb_file_push(yfilename,
        "daemon:\n"
        "  zebos:\n"
        "    nsm: %d\n"
        "    imish: %d\n"
        "  netconf: %d\n"
        "  snmp: %d\n"
        "  g.fast: %d\n",
        100,
        200,
        300,
        400,
        500
    );
    if(res < 0) {
        fprintf(stderr, "fail %d\n", res);
        return -1;
    }

    int nsm = 0;
    int imish = 0;
    int netconf = 0;
    int snmp = 0;
    int gfast = 0;

    // success case
    res = ymldb_file_pull(yfilename,
        "daemon:\n"
        "  zebos:\n"
        "    nsm: %d\n"
        "    imish: %d\n"
        "  netconf: %d\n"
        "  snmp: %d\n"
        "  g.fast: %d\n",
        &nsm,
        &imish,
        &netconf,
        &snmp,
        &gfast
    );

    fprintf(stdout, "ymldb_file_pull %s res %d\n", VARIABLE_NAME(yfilename), res);
    fprintf(stdout, "%s %d\n",VARIABLE_NAME(nsm), nsm);
    fprintf(stdout, "%s %d\n",VARIABLE_NAME(imish), imish);
    fprintf(stdout, "%s %d\n",VARIABLE_NAME(netconf), netconf);
    fprintf(stdout, "%s %d\n",VARIABLE_NAME(snmp), snmp);
    fprintf(stdout, "%s %d\n",VARIABLE_NAME(gfast), gfast);

    // failure case
    int hello = 0;
    char *unknown = "unknown.yml";
    res = ymldb_file_pull(unknown, "hello : %d\n", &hello);
    fprintf(stdout, "ymldb_file_pull %s res %d\n", VARIABLE_NAME(unknown), res);
    fprintf(stdout, "%s %d\n",VARIABLE_NAME(hello), hello);
    return 0;
}

int main(int argc, char *argv[])
{
    return ymldb_file_rw_test();
}
