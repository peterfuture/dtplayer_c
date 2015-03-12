#include <sys/time.h>
#include <stdint.h>
#include <unistd.h>
#include <stddef.h>

int64_t dt_gettime(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((int64_t)(tv.tv_sec) * 1000000 + (int64_t)(tv.tv_usec));
}

int dt_usleep(unsigned usec)
{
    return usleep(usec);
}
