#ifndef __YALLOC__
#define __YALLOC__
#include <stdlib.h>

void *yalloc(size_t s);
char *ystrdup(char *src);
void yfree(void *src);
void yalloc_destroy();
void ystrprint();

#endif // __YALLOC__
