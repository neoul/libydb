#ifndef __YSTR__
#define __YSTR__
#include <stdlib.h>

typedef struct ystr ystr;
const char *ystrndup(char *src, int srclen);
const char *ystrdup(char *src);
const void *ydatadup(void *src, int srclen);
const char *ystrnew(const char *format, ...);
struct ystr *ysearch(const void *src, int srclen);
void yfree(const void *src);
void yfree_all();

#endif // __YSTR__
