/*
 * =====================================================================================
 *
 *    Filename   :  dt_mem.c
 *    Description:
 *    Version    :  1.0
 *    Created    :  2015年11月19日 14时35分27秒
 *    Revision   :  none
 *    Compiler   :  gcc
 *    Author     :  peter-s
 *    Email      :  peter_future@outlook.com
 *    Company    :  dt
 *
 * =====================================================================================
 */

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "dt_mem.h"

static size_t max_alloc_size = INT_MAX;

void dt_max_alloc(size_t max)
{
    max_alloc_size = max;
}

void *dt_malloc(size_t size)
{
    void *ptr = NULL;
#if CONFIG_MEMALIGN_HACK
    long diff;
#endif

    /* let's disallow possibly ambiguous cases */
    if (size > (max_alloc_size - 32)) {
        return NULL;
    }

#if CONFIG_MEMALIGN_HACK
    ptr = malloc(size + ALIGN);
    if (!ptr) {
        return ptr;
    }
    diff              = ((~(long)ptr) & (ALIGN - 1)) + 1;
    ptr               = (char *)ptr + diff;
    ((char *)ptr)[-1] = diff;
#elif HAVE_POSIX_MEMALIGN
    if (size) //OS X on SDK 10.6 has a broken posix_memalign implementation
        if (posix_memalign(&ptr, ALIGN, size)) {
            ptr = NULL;
        }
#elif HAVE_ALIGNED_MALLOC
    ptr = _aligned_malloc(size, ALIGN);
#elif HAVE_MEMALIGN
#ifndef __DJGPP__
    ptr = memalign(ALIGN, size);
#else
    ptr = memalign(size, ALIGN);
#endif
    /* Why 64?
     * Indeed, we should align it:
     *   on  4 for 386
     *   on 16 for 486
     *   on 32 for 586, PPro - K6-III
     *   on 64 for K7 (maybe for P3 too).
     * Because L1 and L2 caches are aligned on those values.
     * But I don't want to code such logic here!
     */
    /* Why 32?
     * For AVX ASM. SSE / NEON needs only 16.
     * Why not larger? Because I did not see a difference in benchmarks ...
     */
    /* benchmarks with P3
     * memalign(64) + 1          3071, 3051, 3032
     * memalign(64) + 2          3051, 3032, 3041
     * memalign(64) + 4          2911, 2896, 2915
     * memalign(64) + 8          2545, 2554, 2550
     * memalign(64) + 16         2543, 2572, 2563
     * memalign(64) + 32         2546, 2545, 2571
     * memalign(64) + 64         2570, 2533, 2558
     *
     * BTW, malloc seems to do 8-byte alignment by default here.
     */
#else
    ptr = malloc(size);
#endif
    if (!ptr && !size) {
        size = 1;
        ptr = dt_malloc(1);
    }
#if CONFIG_MEMORY_POISONING
    if (ptr) {
        memset(ptr, FF_MEMORY_POISON, size);
    }
#endif
    return ptr;
}

void *dt_realloc(void *ptr, size_t size)
{
#if CONFIG_MEMALIGN_HACK
    int diff;
#endif

    /* let's disallow possibly ambiguous cases */
    if (size > (max_alloc_size - 32)) {
        return NULL;
    }

#if CONFIG_MEMALIGN_HACK
    //FIXME this isn't aligned correctly, though it probably isn't needed
    if (!ptr) {
        return dt_malloc(size);
    }
    diff = ((char *)ptr)[-1];
    dt_assert0(diff > 0 && diff <= ALIGN);
    ptr = realloc((char *)ptr - diff, size + diff);
    if (ptr) {
        ptr = (char *)ptr + diff;
    }
    return ptr;
#elif HAVE_ALIGNED_MALLOC
    return _aligned_realloc(ptr, size + !size, ALIGN);
#else
    return realloc(ptr, size + !size);
#endif
}

void *dt_realloc_f(void *ptr, size_t nelem, size_t elsize)
{
    size_t size;
    void *r;

    if (dt_size_mult(elsize, nelem, &size)) {
        dt_free(ptr);
        return NULL;
    }
    r = dt_realloc(ptr, size);
    if (!r && size) {
        dt_free(ptr);
    }
    return r;
}

int dt_reallocp(void *ptr, size_t size)
{
    void *val;

    if (!size) {
        dt_freep(ptr);
        return 0;
    }

    memcpy(&val, ptr, sizeof(val));
    val = dt_realloc(val, size);

    if (!val) {
        dt_freep(ptr);
        return DTERROR(ENOMEM);
    }

    memcpy(ptr, &val, sizeof(val));
    return 0;
}

void *dt_realloc_array(void *ptr, size_t nmemb, size_t size)
{
    if (!size || nmemb >= INT_MAX / size) {
        return NULL;
    }
    return dt_realloc(ptr, nmemb * size);
}

int dt_reallocp_array(void *ptr, size_t nmemb, size_t size)
{
    void *val;

    memcpy(&val, ptr, sizeof(val));
    val = dt_realloc_f(val, nmemb, size);
    memcpy(ptr, &val, sizeof(val));
    if (!val && nmemb && size) {
        return DTERROR(ENOMEM);
    }

    return 0;
}

void dt_free(void *ptr)
{
#if CONFIG_MEMALIGN_HACK
    if (ptr) {
        int v = ((char *)ptr)[-1];
        dt_assert0(v > 0 && v <= ALIGN);
        free((char *)ptr - v);
    }
#elif HAVE_ALIGNED_MALLOC
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

void dt_freep(void *arg)
{
    void *val;

    memcpy(&val, arg, sizeof(val));
    memcpy(arg, &(void *) {
        NULL
    }, sizeof(val));
    dt_free(val);
}

void *dt_mallocz(size_t size)
{
    void *ptr = dt_malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void *dt_calloc(size_t nmemb, size_t size)
{
    if (size <= 0 || nmemb >= INT_MAX / size) {
        return NULL;
    }
    return dt_mallocz(nmemb * size);
}

char *dt_strdup(const char *s)
{
    char *ptr = NULL;
    if (s) {
        size_t len = strlen(s) + 1;
        ptr = dt_realloc(NULL, len);
        if (ptr) {
            memcpy(ptr, s, len);
        }
    }
    return ptr;
}

char *dt_strndup(const char *s, size_t len)
{
    char *ret = NULL, *end;

    if (!s) {
        return NULL;
    }

    end = memchr(s, 0, len);
    if (end) {
        len = end - s;
    }

    ret = dt_realloc(NULL, len + 1);
    if (!ret) {
        return NULL;
    }

    memcpy(ret, s, len);
    ret[len] = 0;
    return ret;
}

void *dt_memdup(const void *p, size_t size)
{
    void *ptr = NULL;
    if (p) {
        ptr = dt_malloc(size);
        if (ptr) {
            memcpy(ptr, p, size);
        }
    }
    return ptr;
}


