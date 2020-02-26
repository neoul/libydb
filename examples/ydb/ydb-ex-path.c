// ydb-ex-path.c

#include <stdio.h>
#include <ylog.h>
#include <ydb.h>

int main(int argc, char *argv[])
{
    ydb *datablock;
    FILE *input;
    datablock = ydb_open("demo");
    input = fopen("examples/yaml/yaml-demo.yaml", "r");
    ydb_parse(datablock, input);
    ydb_dump(datablock, stdout);

    ydb_write(datablock, 
        "omap:\n"
        " Bestiary: !!omap\n"
        "  anaconda: abc\n");

    ydb_path_fprintf(stdout, datablock, "/omap/Bestiary");

    ydb_path_write(datablock, "/omap/Bestiary/anaconda=update!");
    ydb_path_fprintf(stdout, datablock, "/omap/Bestiary");
    
    ylog_level = YLOG_DEBUG;
    ydb_path_write(datablock, "/merge/0/x=1000");
    ydb_path_fprintf(stdout, datablock, "/merge");

    ylog_level = YLOG_ERROR;
    ydb_close(datablock);
    return 0;
}