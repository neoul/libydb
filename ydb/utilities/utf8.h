// Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

// added checkUTF8() to check yaml scalar value

#include <stdint.h>
#ifndef _UTF_H_
#define _UTF_H_

#ifdef __cplusplus
extern "C" {
#endif

int isUTF8(uint8_t *s);
int checkUTF8(uint8_t *s);
void printCodePoints(uint8_t *s);

#ifdef __cplusplus
}
#endif

#endif
