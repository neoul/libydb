#ifndef __YSTR__
#define __YSTR__
#include <stdlib.h>

// ystr: YDB string pool (ystr_pool)

typedef struct ystr ystr;

// ystrndump --
// dump src up to srclen.
const char *ystrndup(char *src, int srclen);

// ystrndump --
// dump src.
const char *ystrdup(char *src);

// ystrnew --
// Build a new string formatted.
const char *ystrnew(const char *format, ...);

// ystrbody --
// Get ystr structure from src.
ystr *ystrbody(const char *src);

// ydatasearch --
// Return ystr matched with src and srclen
// If srclen <= 0, search only yptr_pool
struct ystr *ystrsearch(const char *src);


// ydatadup --
// dump src
const void *ydatadup(void *src, int srclen);

// ydatabody --
// Get ystr structure from src
ystr *ydatabody(const void *src);

// ydatasearch --
// Return ystr matched with src and srclen
// If srclen <= 0, search only yptr_pool
struct ystr *ydatasearch(const void *src);

// ystrdata --
// Return the data of ystr
const void *ystrdata(struct ystr *str);

// ystrsize --
// Return the size of ystr data/string
int ystrsize(ystr *str);

// ystrref --
// Return the reference count of ystr
int ystrref(ystr *str);

// yfree --
// Free allocated ystr
void yfree(const void *src);

// yfree_all --
// Free all ystr
void yfree_all();

#endif // __YSTR__
