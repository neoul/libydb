#ifndef __YALLOC__
#define __YALLOC__
#include <stdlib.h>

void *yalloc(size_t s);
char *ystrdup(char *src);
void yfree(void *src);
void yalloc_destroy();
void ystrprint();

// overwrite default funcs
// #define free yfree
// #define malloc yalloc
// #undef strdup
// #define strdup ystrdup

// extern char *y_space_str;

#endif // __YALLOC__
