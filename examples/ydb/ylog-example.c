#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "ylog.h"

extern FILE *ylog_fp;
int main(int argc, char *argv[])
{
    ylog_level = YLOG_DEBUG;

    ylog_debug("logging for debug\n");
    ylog_info("logging for info\n");
    ylog_warn("logging for warn\n");
    ylog_error("logging for error\n");

    if (argc > 1)
    {
        ylog_file_open(argv[1]);
    }
    
    ylog_in();
    ylog_debug("logging for debug\n");
    ylog_info("logging for info\n");
    ylog_warn("logging for warn\n");
    ylog_error("logging for error\n");
    ylog_out();

    fclose(ylog_fp);
    
    ylog_in();
    ylog_debug("logging for debug\n");
    ylog_info("logging for info\n");
    ylog_warn("logging for warn\n");
    ylog_error("logging for error\n");
    ylog_out();

    if (argc > 1)
        ylog_file_close();
}
