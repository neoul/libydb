// ydb-ex-seq.c

#include <stdio.h>
#include <string.h>
#include <ylog.h>
#include <ydb.h>

// yaml block sequence format
char *yaml_seq1 =
    "- entry1\n"
    "- entry2\n"
    "- entry3\n";

// yaml flow sequence format
char *yaml_seq2 =
    "[entry4, entry5]\n";

char *yaml_seq3 =
    "- \n"
    "- \n";

char *yaml_seq4 =
    "- %s\n"
    "- %s\n";

// yaml block sequence format
char *yaml_seq5 =
    "- \n"
    "- \n"
    "- %s\n";

// yaml flow sequence format
char *yaml_seq6 =
    "[ , , %s]\n";

int main(int argc, char *argv[])
{
    ydb *datablock;
    datablock = ydb_open("demo");

    fprintf(stdout, "\n");
    fprintf(stdout, "YDB example for YAML sequence (list)\n");
    fprintf(stdout, "=============================\n");

    // ylog_severity = YLOG_DEBUG;
    fprintf(stdout, "\n[ydb_parses]\n");
    ydb_parses(datablock, yaml_seq1, strlen(yaml_seq1));
    ydb_dump(datablock, stdout);

    fprintf(stdout, "\n[ydb_write] (push two entries to the tail)\n");
    ydb_write(datablock, yaml_seq2);
    ydb_dump(datablock, stdout);

    fprintf(stdout, "\n[ydb_delete] (pop two entries from the head)\n");
    ydb_delete(datablock, yaml_seq3);
    ydb_dump(datablock, stdout);

    fprintf(stdout, "\n[ydb_read] (read two entries from the head)\n");
    char e1[32] = {0};
    char e2[32] = {0};
    ydb_read(datablock, yaml_seq4, e1, e2);
    printf("e1=%s, e2=%s\n", e1, e2);

    fprintf(stdout, "\n[ydb_read] (read 3th entry from the head)\n");
    char e3[32] = {0};
    ydb_read(datablock, yaml_seq5, e3);
    printf("e3=%s\n", e3);

    fprintf(stdout, "\n[ydb_read] (read 3th entry using yaml flow sequence format)\n");
    e3[0] = 0;
    ydb_read(datablock, yaml_seq6, e3);
    printf("e3=%s\n", e3);

    fprintf(stdout, "\n[ydb_path_read] (read 3th entry using ydb path)\n");
    const char *data;
    data = ydb_path_read(datablock, "/2");
    fprintf(stdout, "/2=%s\n", data);

    fprintf(stdout, "\n[ydb_path_delete] only delete the first entry. others are not allowed.\n");
    ydb_res res;

    res = ydb_path_delete(datablock, "/2");
    fprintf(stdout, "delete /2 ==> %s\n", ydb_res_str(res));

    res = ydb_path_delete(datablock, "/0");
    fprintf(stdout, "delete /0 ==> %s\n", ydb_res_str(res));

    fprintf(stdout, "\n[ydb_path_write] push an entry to the tail. \n");

    // append the data to the list regardless of the index.
    res = ydb_path_write(datablock, "/0=abc");
    fprintf(stdout, "write /0=abc ==> %s\n", ydb_res_str(res));

    ydb_dump(datablock, stdout);

    ydb_close(datablock);
    return 0;
}