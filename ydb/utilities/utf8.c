// Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>

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
decode(uint32_t *state, uint32_t *codep, uint32_t byte)
{
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

char *yaml_multiline_str(const char *src, int srclen, int newline, int indent)
{
    uint32_t codepoint;
    uint32_t state = 0;
    uint8_t *s;
    char *newstr;
    char *base;
    int len;
    // printf("src=%d, indent=%d, newline=%d \n", srclen, indent, newline);
    indent += 1;
    newstr = malloc(srclen + ((newline + 1) * indent)  + 2 + 2);
    if (!newstr)
    {
        return NULL;
    }
    len = 0;
    base = (char *)src;
    s = (uint8_t *)src;

    len = sprintf(newstr, "|\n");
    memset(&newstr[len], ' ', indent);
    len += indent;

    for (; *s; ++s)
    {
        if (!decode(&state, &codepoint, *s))
        {
            // printf("  codepoint: U+%04X\n", codepoint);
            if (codepoint <= 0x7F)
            {
                newstr[len] = codepoint;
                len++;
                if (codepoint == '\n')
                {
                    memset(&newstr[len], ' ', indent);
                    len += indent;
                }
            }
            else if (codepoint <= 0x07FF)
            {
                memcpy(&newstr[len], base, 2);
                len += 2;
            }
            else if (codepoint < 0xFFFF)
            {
                memcpy(&newstr[len], base, 3);
                len += 3;
            }
            else
            {
                memcpy(&newstr[len], base, 4);
                len += 4;
            }
            base = (char *)(s + 1);
        }
    }
    newstr[len] = 0;
    return newstr;
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

// YAML’s double-quoted style uses familiar C-style escape sequences. This enables ASCII encoding of non-printable or 8-bit (ISO 8859-1) characters such as “\x3B”. Non-printable 16-bit Unicode and 32-bit (ISO/IEC 10646) characters are supported with escape sequences such as “\u003B” and “\U0000003B”.

char *yaml_string(const char *src, int indent, int *is_new)
{
    uint32_t codepoint;
    uint32_t state = 0;
    uint8_t *s = (uint8_t *)src;
    int non_printable8 = 0;
    int non_printable16 = 0;
    int non_printable32 = 0;
    int quotes_required = 0;
    int backslash = 0;
    int d_quotes = 0;
    int newline = 0;
    char *newstr;
    char *base;
    int len;

    if (is_new)
        *is_new = 0;
    if (!src)
        return "(null)";
    char c = *src;
    if (ispunct(c) || c == ' ')
    {
        quotes_required++;
    }
    for (; *s; ++s)
    {
        if (!decode(&state, &codepoint, *s))
        {
            if (state == UTF8_REJECT)
            {
                return "(non-UTF8)";
            }
            // printf("  codepoint: U+%04X\n", codepoint);
            if (codepoint <= 0x7F)
            {
                if (isprint(codepoint)) // isgraph() + ' '
                {
                    if (codepoint == '"' || codepoint == '\\')
                        d_quotes++;
                }
                // if (isalnum(codepoint) || codepoint == ' ')
                // {
                //     ;
                // }
                // else if (ispunct(codepoint))
                // {
                //     if (codepoint == '"' || codepoint == '\\')
                //         d_quotes++;
                //     quotes_required++;
                // }
                else if (codepoint == 0xA)
                    newline++;
                else if (codepoint == 0x9 || codepoint == 0xD)
                    backslash++;
                else
                    non_printable8++;
            }
            else if (codepoint <= 0xFFFF)
            {
                if ((codepoint >= 0xA0 && codepoint <= 0xD7FF) ||
                      (codepoint >= 0xE000 && codepoint <= 0xFFFD) ||
                      codepoint == 0x85)
                {
                    if (codepoint == 0xA0 || codepoint == 0x0085 || codepoint == 0x2028 || codepoint == 0x2029)
                    {
                        backslash++;
                        quotes_required++;
                    }
                }
                else
                    non_printable16++;
            }
            else
            {
                if (!(codepoint >= 0x10000 && codepoint <= 0x10FFFF))
                    non_printable32++;
            }
        }
    }

    len = (char *)s - src;
    if (quotes_required == 0 && non_printable8 == 0 && non_printable16 == 0 && non_printable32 == 0)
    {
        // printf("src(%d)=%s, newline=%d, backslash=%d, d_quotes=%d, no8=%d, no16=%d, no32=%d, malloc=%d\n", len, src,
        //    newline, backslash, d_quotes, non_printable8, non_printable16, non_printable32, len);
        if (backslash == 0 && newline == 0 && d_quotes == 0)
            return (char *)src;
        else
        {
            if (indent >= 0 && len > 64)
            {
                newstr = yaml_multiline_str(src, len, newline, indent);
                if (newstr)
                {
                    if (is_new)
                        *is_new = 1;
                    return newstr;
                }
            }
        }
    }

    len = (len + backslash + newline + d_quotes +
           (non_printable8 * 4) + (non_printable16 * 6) +
           (non_printable32 * 10) + 4);
    // printf("src(%d)=%s, newline=%d, backslash=%d, d_quotes=%d, no8=%d, no16=%d, no32=%d, malloc=%d\n", len, src,
    //        newline, backslash, d_quotes, non_printable8, non_printable16, non_printable32, len);
    
    newstr = malloc(len);
    if (!newstr)
    {
        return "(null)";
    }
    len = 0;
    base = (char *)src;
    s = (uint8_t *)src;
    newstr[len] = '"';
    len++;
    for (; *s; ++s)
    {
        if (!decode(&state, &codepoint, *s))
        {
            // printf("  codepoint: U+%04X\n", codepoint);
            if (codepoint <= 0x7F)
            {
                if (isprint(codepoint)) // isgraph() + ' '
                {
                    if (codepoint == '"' || codepoint == '\\')
                    {
                        newstr[len] = '\\';
                        newstr[len + 1] = codepoint;
                        len += 2;
                    }
                    else
                    {
                        newstr[len] = codepoint;
                        len++;
                    }
                }
                else if (codepoint == 0x9 || codepoint == 0xA || codepoint == 0xD)
                {
                    newstr[len] = '\\';
                    newstr[len + 1] = (codepoint == 0x9) ? 't' : (codepoint == 0xA) ? 'n' : 'r';
                    len += 2;
                }
                else
                {
                    int n = sprintf(newstr + len, "\\x%02X", codepoint);
                    assert(n == 4);
                    len = len + n;
                }
            }
            else if (codepoint <= 0x07FF)
            {
                if (codepoint == 0xA0 || codepoint == 0x0085)
                {
                    newstr[len] = '\\';
                    newstr[len + 1] = (codepoint == 0xA0) ? '_' : 'N';
                    len += 2;
                }
                else if ((codepoint >= 0xA0 && codepoint <= 0xD7FF) || codepoint == 0x85)
                {
                    memcpy(&newstr[len], base, 2);
                    len += 2;
                }
                else
                {
                    int n = sprintf(newstr + len, "\\u%04X", codepoint);
                    assert(n == 6);
                    len = len + n;
                }
            }
            else if (codepoint < 0xFFFF)
            {
                if (codepoint == 0x2028 || codepoint == 0x2029)
                {
                    newstr[len] = '\\';
                    newstr[len + 1] = (codepoint == 0x2028) ? 'L' : 'P';
                    len += 2;
                }
                else if ((codepoint >= 0xA0 && codepoint <= 0xD7FF) ||
                    (codepoint >= 0xE000 && codepoint <= 0xFFFD))
                {
                    memcpy(&newstr[len], base, 3);
                    len += 3;
                }
                else
                {
                    int n = sprintf(newstr + len, "\\u%04X", codepoint);
                    assert(n == 6);
                    len = len + n;
                }
            }
            else
            {
                if (codepoint <= 0x10FFFF)
                {
                    memcpy(&newstr[len], base, 4);
                    len += 4;
                }
                else
                {
                    int n = sprintf(newstr + len, "\\U%08X", codepoint);
                    assert(n == 10);
                    len = len + n;
                }
            }
            base = (char *)(s + 1);
        }
    }
    newstr[len] = '"';
    len++;
    newstr[len] = 0;
    if (is_new)
        *is_new = 1;
    return newstr;
}
