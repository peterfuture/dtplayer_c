#if ENABLE_STREAM_FILE

#include "dtstream.h"
#include "dt_error.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define TAG "STREAM-FILE"

typedef struct{
    int fd;
    int64_t file_size;
}file_ctx_t;

static int stream_file_open (stream_wrapper_t * wrapper,char *stream_name)
{
    int fd = -1;
    file_ctx_t *ctx = malloc(sizeof(*ctx));
    stream_ctrl_t *info = &wrapper->info;
    fd = open(stream_name,O_RDONLY);
    if(fd < 0)
    {
        dt_error(TAG,"OPEN FILE FAILED \n");
        return -1;
    }
    ctx->fd = fd;
    
    struct stat state;  
    if(stat(stream_name, &state) < 0){  
        ctx->file_size = -1; 
    }
    else
        ctx->file_size = state.st_size;  
    wrapper->stream_priv = (void *)ctx;
    info->file_size = ctx->file_size;
    info->is_stream = 0;
    info->seek_support = 1;
    info->cur_pos = 0;
    return DTERROR_NONE;
}

static int stream_file_read (stream_wrapper_t * wrapper,char *buf,int len)
{
    int r;
    file_ctx_t *ctx = (file_ctx_t *)wrapper->stream_priv;
    r = read(ctx->fd,buf,len);
    return (r <= 0) ? -1 : r;
}

static int stream_file_seek (stream_wrapper_t * wrapper, int64_t pos)
{
    file_ctx_t *ctx = (file_ctx_t *)wrapper->stream_priv;
    stream_ctrl_t *info = &wrapper->info;
    if(lseek(ctx->fd,pos,SEEK_SET)<0)
        return -1;
    info->cur_pos = pos;
    return DTERROR_NONE;
}

static int stream_file_close (stream_wrapper_t * wrapper)
{
    file_ctx_t *ctx = malloc(sizeof(*ctx));
    close(ctx->fd);
    free(ctx);
    wrapper->stream_priv = NULL;
    return 0;
}

stream_wrapper_t stream_file = {
    .name = "File",
    .id = STREAM_FILE,
    .open = stream_file_open,
    .read = stream_file_read,
    .seek = stream_file_seek,
    .close = stream_file_close,
};

#endif /*ENABLE_STREAM_FILE*/
