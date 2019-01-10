// ydb-ex-list.c

#include <stdio.h>
#include <string.h>
#include <ylog.h>
#include <ydb.h>

char *yaml_seq1 =
    " - \n"
    " - entry1\n"
    " - entry2\n"
    " - entry3\n";

char *yaml_seq2 =
    " - entry4\n"
    " - entry5\n";

char *yaml_seq3 =
    " - \n"
    " - \n";

char *yaml_seq4 =
    " - %s\n"
    " - %s\n";

int main(int argc, char *argv[])
{
    ydb *datablock;
    datablock = ydb_open("demo");
    
    fprintf(stdout, "\n");
    fprintf(stdout, "ydb example for yaml sequence (list)\n");
    fprintf(stdout, "=============================\n");
    fprintf(stdout, " - yaml sequence (list) in ydb is handled by the sequence (index).\n");
    fprintf(stdout, " - ydb_write: push all list entries back to the target list.\n");
    fprintf(stdout, " - ydb_delete: pop a number of entries from the head of the target list.\n");
    fprintf(stdout, " - ydb_read: read them from the head of the target list.\n");

    // ylog_severity = YLOG_DEBUG;
    fprintf(stdout, "\n[ydb_parses]\n");
    ydb_parses(datablock, yaml_seq1, strlen(yaml_seq1));
    ydb_dump(datablock, stdout);

    fprintf(stdout, "\n[ydb_write] (push them to the tail)\n");
    ydb_write(datablock, (const char *) yaml_seq2);
    ydb_dump(datablock, stdout);

    fprintf(stdout, "\n[ydb_delete] (pop them from the head)\n");
    ydb_delete(datablock, (const char *) yaml_seq3);
    ydb_dump(datablock, stdout);

    fprintf(stdout, "\n[ydb_read] (read them from the head)\n");
    char e1[32] = {0};
    char e2[32] = {0};
    ydb_read(datablock, (const char *) yaml_seq4, e1, e2);
    printf("e1=%s, e2=%s\n", e1, e2);

    ydb_close(datablock);
    return 0;
}