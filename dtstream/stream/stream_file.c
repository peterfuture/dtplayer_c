#include "dtstream.h"
#include "dt_error.h"

#define TAG "STREAM-FILE"

static int stream_file_open (stream_wrapper_t * wrapper)
{
    int ret = 0;
    return ret;
}

static int stream_file_read (stream_wrapper_t * wrapper,char *buf,int len)
{
    return DTERROR_NONE;
}

static int stream_file_seek (stream_wrapper_t * wrapper, int64_t pos)
{
    return DTERROR_NONE;
}

static int stream_file_close (stream_wrapper_t * wrapper)
{
    return 0;
}

stream_wrapper_t stream_file = {
    .name = "File",
    .open = stream_file_open,
    .read = stream_file_read,
    .seek = stream_file_seek,
    .close = stream_file_close,
};
