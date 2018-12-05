#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>

#include "ylog.h"
#include "ydb.h"

static int done;
void HANDLER_SIGINT(int param)
{
    done = 1;
}

char *get_stdin_to_buf(size_t *buflen)
{
    int ret;
    struct timeval tv;
    fd_set read_set;

    tv.tv_sec = 0;
    tv.tv_usec = 10;
    FD_ZERO(&read_set);
    FD_SET(STDIN_FILENO, &read_set);
    ret = select(STDIN_FILENO + 1, &read_set, NULL, NULL, &tv);
    if (ret < 0)
        return NULL;
    if (FD_ISSET(STDIN_FILENO, &read_set))
    {
        int n;
        FILE *fp;
        char *buf;
        char temp[512];
        buf = NULL;
        *buflen = 0;
        fp = open_memstream(&buf, buflen);
        if (!fp)
            return NULL;
        while ((n = read(STDIN_FILENO, temp, sizeof(temp))) > 0)
        {
            fwrite(temp, n, 1, fp);
        }
        fclose(fp);
        return buf;
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    ydb_res res;
    ydb *datablock;
    size_t buflen = 0;
    char *buf = NULL;
    char dest[256];

    if (argc < 4)
    {
        fprintf(stdout, " [usage]\n");
        fprintf(stdout, "  (INPUT) | %s (ADDR) (SRC) (DEST)\n\n", argv[0]);
        fprintf(stdout, " [example]\n");
        fprintf(stdout, "  cat examples/yaml/yaml-input-1.yaml | %s uss://top 1 2\n\n", argv[0]);
        return 0;
    }

    // ylog_severity = YLOG_DEBUG;
    datablock = ydb_open("top");
    if (!datablock)
    {
        fprintf(stderr, "ydb_open failed.\n");
        goto failed;
    }

    buf = get_stdin_to_buf(&buflen);

    if (!buf || buflen <= 0)
    {
        fprintf(stderr, "no input\n");
        goto failed;
    }

    // ignore SIGPIPE.
    signal(SIGPIPE, SIG_IGN);
    // add a signal handler to quit this program.
    signal(SIGINT, HANDLER_SIGINT);

    // update the src (mine) id
    ydb_write(datablock, "+id: {%s: }\n", argv[2]);

    // connect to the channel
    res = ydb_connect(datablock, argv[1], "pub");
    if (res)
    {
        fprintf(stderr, "ydb_connect failed. ADDR=%s\n", argv[1]);
        goto failed;
    }

    // set the whispering target.
    snprintf(dest, sizeof(dest), "/+id/%s", argv[3]);

    do
    {
        static int whispered;
        // check the whispering target exists and whisper my data once.
        if (!whispered)
        {
            ylog_severity = YLOG_CRITICAL;
            res = ydb_whisper_merge(datablock, dest, "%s", buf);
            ylog_severity = YLOG_ERROR;
            if (!res)
            {
                whispered = 1;
                fprintf(stdout, "ydb_whisper_merge from %s to %s\n", argv[2], argv[3]);
            }
        }
        res = ydb_serve(datablock, 1000);
        if (YDB_FAILED(res))
            break;
    } while (!done);

    if (buf)
        free(buf);
    ydb_dump(datablock, stdout);
    ydb_close(datablock);
    return 0;

failed:
    if (buf)
        free(buf);
    ydb_close(datablock);
    return 1;
}
