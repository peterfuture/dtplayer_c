// HTTP STREAM USING CURL

#include "dtstream.h"
#include "dt_error.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define TAG "STREAM-CURL"

typedef struct {
    int fd;
    int64_t file_size;
} curl_ctx_t;

static int stream_curl_open(stream_wrapper_t * wrapper, char *stream_name)
{
    return DTERROR_NONE;
}

static int stream_curl_read(stream_wrapper_t * wrapper, uint8_t *buf, int len)
{
    return 0;
}

static int stream_curl_seek(stream_wrapper_t * wrapper, int64_t pos, int whence)
{
    return DTERROR_NONE;
}

static int stream_curl_close(stream_wrapper_t * wrapper)
{
    return 0;
}

stream_wrapper_t stream_curl = {
    .name = "CURL STREAMMING - HTTP",
    .id = STREAM_CURL,
    .open = stream_curl_open,
    .read = stream_curl_read,
    .seek = stream_curl_seek,
    .close = stream_curl_close,
};
