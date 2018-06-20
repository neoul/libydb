#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>


void ylogging (const char *format, ...)
{
  FILE *fp;
  va_list args;
  char buf[1024];

  fp = fopen("/tmp/ycomm.log", "a");
  if(fp == NULL) return;

  va_start (args, format);
  vsnprintf(buf, sizeof(buf), format, args);

  fprintf (fp, "%s", buf);

  fclose(fp);
  va_end (args);
}