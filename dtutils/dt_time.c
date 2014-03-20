#include <sys/time.h>
#include <stdint.h>
#include <unistd.h>
#include <stddef.h>

uint64_t dt_gettime (void)
{
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (uint64_t) (tv.tv_sec * 1000000 + tv.tv_usec);
}

int dt_usleep (unsigned usec)
{
    return usleep (usec);
}
