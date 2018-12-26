#ifndef __YALLOC__
#define __YALLOC__
#include <stdlib.h>

void *yalloc(size_t s);
const char *ystrdup(char *src);
const char *ystrndup(char *src, int srclen);
void yfree(const void *src);
void yalloc_destroy();
void ystrprint();

#endif // __YALLOC__
