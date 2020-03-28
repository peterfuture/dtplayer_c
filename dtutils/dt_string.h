#ifndef DTUTIL_STRING_H
#define DTUTIL_STRING_H

#include <stddef.h>
#include <stdint.h>

int dt_strstart(const char *str, const char *pfx, const char **ptr);
size_t dt_strlcpy(char *dst, const char *src, size_t size);

static inline int dt_toupper(int c)
{
    if (c >= 'a' && c <= 'z')
        c ^= 0x20;
    return c;
}

static inline int dt_tolower(int c)
{
    if (c >= 'A' && c <= 'Z')
        c ^= 0x20;
    return c;
}

#endif /* DTUTIL_STRING_H */
