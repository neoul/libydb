#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "ymldb.h"

int main(int argc, char *argv[])
{
    int help = 0;
    int infile = 0;
    int outfile = 0;
    int k;
    char *infilename = "";
    char *outfilename = "";
    int infd = 0;
    int outfd = 0;
    int res = 0;

    /* Analyze command line options. */
    for (k = 1; k < argc; k++)
    {
        // fprintf(stdout, "argv[%d]=%s\n", k, argv[k]);
        if (strcmp(argv[k], "-h") == 0 || strcmp(argv[k], "--help") == 0)
        {
            help = 1;
        }
        else if (strcmp(argv[k], "-i") == 0 || strcmp(argv[k], "--infile") == 0)
        {
            infile = k;
        }
        else if (strcmp(argv[k], "-o") == 0 || strcmp(argv[k], "--outfile") == 0)
        {
            outfile = k;
        }
        else
        {
            if (infile + 1 == k)
            {
                infilename = argv[k];
                fprintf(stderr, "infilename %s\n", infilename);
            }
            else if (outfile + 1 == k)
            {
                outfilename = argv[k];
                fprintf(stderr, "outfilename %s\n", outfilename);
            }
            else
            {
                fprintf(stderr, "Unrecognized option: %s\n"
                                "Try `%s --help` for more information.\n",
                        argv[k], argv[0]);
                return 1;
            }
        }
    }

    /* Display the help string. */
    if (help)
    {
        _log_printf("%s <input\n"
               "or\n%s -h | --help\nDeconstruct a YAML in\n\nOptions:\n"
               "-h, --help\t\tdisplay this help and exit\n"
               "-i, --infile\t\tinput ymldb file to read. (*.yml)\n"
               "-o, --outfile\t\toutput ymldb file to read. (*.yml)\n",
               argv[0], argv[0]);
        return 0;
    }

    infd = STDIN_FILENO;
    if (strlen(infilename) > 0)
    {
        infd = open(infilename, O_RDONLY, 0644);
        if (infd < 0)
        {
            fprintf(stderr, "file open error. %s\n", strerror(errno));
            return 1;
        }
    }

    outfd = STDOUT_FILENO;
    if (strlen(outfilename) > 0)
    {
        outfd = open(outfilename, O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (outfd < 0)
        {
            fprintf(stderr, "outfilename file open error. %s\n", strerror(errno));
            return 1;
        }
    }
    
    struct ymldb_cb *cb;
    cb = ymldb_create("interface", YMLDB_FLAG_NONE);
    if (!cb)
        return -1;
    ymldb_run(cb, infd, outfd);

    

    struct ymldb_cb *cb2;
    cb2 = ymldb_create("system", YMLDB_FLAG_NONE);

    _log_printf("PUSH\n");
    res = ymldb_push(cb2,
                     "system:\n"
                     "  product: %s\n"
                     "  serial-number: %s\n"
                     "  code: 1223\n",
                     "G.FAST-HN5124D",
                     "HN5124-S100213124");
    if (res < 0)
    {
        _log_printf("ERROR in ymldb_push\n");
    }

    ymldb_dump_all(stdout);

    char ip1[64];
    char ip2[32];
    ymldb_pull(cb,
               "interface:\n"
               "  ge1:\n"
               "    ip-addr:\n"
               "      - %s\n"
               "      - %s\n",
               ip1, ip2);
    printf("ip1= %s\n", ip1);
    printf("ip2= %s\n", ip1);

    _log_printf("WRITE\n");
    res = ymldb_write(cb2, "system", "product", "abc");
    if (res < 0)
    { // key must be the same
        _log_printf("ERROR in ymldb_write\n");
    }

    ymldb_dump_all(stdout);
    _log_printf("PULL\n");
    char serial_number[32];
    char productstr[32];
    int code;
    ymldb_pull(cb2,
               "system:\n"
               "  serial-number: %s\n"
               "  code: %d\n"
               "  product: %s\n",
               serial_number,
               &code,
               productstr);
    _log_printf("serial_number=%s\n", serial_number);
    _log_printf("code=%d\n", code);
    _log_printf("product=%s\n", productstr);

    _log_printf("READ\n");
    
    char *data = ymldb_read(cb2, "system", "code", "123"); // 123 is the value... so, ignored.
    _log_printf("read data = %s\n", data);

    data = ymldb_read(cb2, "system", "code"); // retrieve the code.
    _log_printf("read data = %s\n", data);

    data = ymldb_read(cb2, "system");
    _log_printf("read data = %s\n", data);

    data = ymldb_read(cb2, "system", "unknown");
    _log_printf("read data = %s\n", data);

    data = ymldb_read(cb, "interface", "ge1", "ip-addr");
    _log_printf("read data = %s\n", data);
    
    ymldb_destroy(cb);
    ymldb_destroy(cb2);

    ymldb_dump_all(stdout);

    if (infd)
        close(infd);
    if (outfd)
        close(outfd);
    return 0;
}
