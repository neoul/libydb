// Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define UTF8_ACCEPT 0
#define UTF8_REJECT 12

static const uint8_t utf8d[] = {
  // The first part of the table maps bytes to character classes that
  // to reduce the size of the transition table and create bitmasks.
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
   7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
   8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,

  // The second part is a transition table that maps a combination
  // of a state of the automaton and a character class to a state.
   0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
  12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
  12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
  12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
  12,36,12,12,12,12,12,12,12,12,12,12, 
};

static inline uint32_t
decode(uint32_t* state, uint32_t* codep, uint32_t byte) {
  uint32_t type = utf8d[byte];

  *codep = (*state != UTF8_ACCEPT) ?
    (byte & 0x3fu) | (*codep << 6) :
    (0xff >> type) & (byte);

  *state = utf8d[256 + *state + type];
  return *state;
}

int isUTF8(uint8_t *s)
{
  uint32_t codepoint, state = 0;
  while (*s)
    decode(&state, &codepoint, *s++);
  return state == UTF8_ACCEPT;
}

// Chapter 5. Characters
// 5.1. Character Set
// To ensure readability, YAML streams use only the printable subset of the Unicode character set. The allowed character range explicitly excludes the C0 control block #x0-#x1F (except for TAB #x9, LF #xA, and CR #xD which are allowed), DEL #x7F, the C1 control block #x80-#x9F (except for NEL #x85 which is allowed), the surrogate block #xD800-#xDFFF, #xFFFE, and #xFFFF.
// On input, a YAML processor must accept all Unicode characters except those explicitly excluded above.
// On output, a YAML processor must only produce acceptable characters. Any excluded characters must be presented using escape sequences. In addition, any allowed characters known to be non-printable should also be escaped. This isn’t mandatory since a full implementation would require extensive character property tables.
// [1]	c-printable	::=	  #x9 | #xA | #xD | [#x20-#x7E]          /* 8 bit */
//                      | #x85 | [#xA0-#xD7FF] | [#xE000-#xFFFD] /* 16 bit */
//                      | [#x10000-#x10FFFF]                     /* 32 bit */
// To ensure JSON compatibility, YAML processors must allow all non-control characters inside quoted scalars. To ensure readability, non-printable characters should be escaped on output, even inside such scalars. Note that JSON quoted scalars cannot span multiple lines or contain tabs, but YAML quoted scalars can.
// [2]	nb-json	::=	#x9 | [#x20-#x10FFFF]

int checkUTF8(uint8_t *s)
{
  uint32_t codepoint;
  uint32_t state = 0;
  int not_printable = 0;

  for (; *s; ++s)
  {
    printf("STR %s\n", s);
    if (!decode(&state, &codepoint, *s))
    {
      printf("  codepoint: U+%04X\n", codepoint);
      if (!((codepoint >= 0x20 && codepoint <= 0x7E) ||
            (codepoint >= 0xA0 && codepoint <= 0xD7FF) ||
            (codepoint >= 0xE000 && codepoint <= 0xFFFD) ||
            (codepoint >= 0x10000 && codepoint <= 0x10FFFF) ||
            (codepoint == 0x9) ||
            (codepoint == 0xA) ||
            (codepoint == 0xD) ||
            (codepoint == 0x85)))
      {
        not_printable++;
      }
    }
    if (state == UTF8_REJECT)
      return -1;
  }
  if (not_printable)
    return not_printable;
  return 0;
}

void printCodePoints(uint8_t *s)
{
  uint32_t codepoint;
  uint32_t state = 0;

  for (; *s; ++s)
    if (!decode(&state, &codepoint, *s))
      printf("U+%04X\n", codepoint);
  if (state != UTF8_ACCEPT)
    printf("The string is not well-formed\n");
}

#if 0
int main(int argc, char *argv[])
{
  char *example[]
  = {
    "abc ",
    "한글",
    "\xff\x34",
    "\x7F",
    "willing 준"
  };
  int i = 0;
  for (; i < (sizeof(example)/sizeof(char *)); i++)
  {
    int state = checkUTF8(example[i]);
    if (state < 0)
      printf("- not UTF8 format\n");
    else if (state == 0)
      printf("- printable UTF8 format\n");
    else
      printf("- not printable UTF8 format inside the string.\n");
  }
  return 0;
}
#endif