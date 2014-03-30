#include "dtstream.h"

#include <unistd.h>

#define TAG "STREAM"

#define REGISTER_STREAM(X,x) \
    if(ENABLE_STREAM_##X)    \
    {                         \
        extern stream_wrapper_t stream_##x; \
        register_stream(&stream_##x);     \
    }
static stream_wrapper_t *g_stream = NULL;

static void register_stream (stream_wrapper_t * wrapper)
{
    stream_wrapper_t **p;
    p = &g_stream;
    while (*p != NULL)
        p = &((*p)->next);
    *p = wrapper;
    dt_info (TAG, "[%s:%d] register stream, name:%s fmt:%d \n", __FUNCTION__, __LINE__, (*p)->name, (*p)->id);
    wrapper->next = NULL;
}

void stream_register_all ()
{
    REGISTER_STREAM (FILE, file);
#ifdef ENABLE_STREAM_FFMPEG
    REGISTER_STREAM (FFMPEG, ffmpeg);
#endif
}

static int get_stream_id(char *name)
{
    int ret = access(name,0);
    if(ret ==0)
        return STREAM_FILE;
    return STREAM_FFMPEG; // default 
}

static int stream_select (dtstream_context_t * stm_ctx)
{
    if (!g_stream)
        return -1;
    int id = get_stream_id(stm_ctx->stream_name);
    dt_info(TAG,"get stream id:%d \n",id);
    stream_wrapper_t *entry = g_stream;
    while(entry)
    {
        if(id == entry->id || STREAM_FFMPEG == entry->id)
            break;
        entry = entry->next;
    }
    if(!entry)
        return -1;
    stm_ctx->stream = entry;
    dt_info (TAG, "[%s:%d] select stream, name:%s id:%d \n", __FUNCTION__, __LINE__, entry->name, entry->id);
    return 0;
}

int stream_open (dtstream_context_t * stm_ctx)
{
    int ret = 0;
    if (stream_select (stm_ctx) == -1)
    {
        dt_error (TAG, "select stream failed \n");
        return -1;
    }
    stream_wrapper_t *wrapper = stm_ctx->stream;
    memset(&wrapper->info,0,sizeof(stream_ctrl_t));
    dt_info (TAG, "select stream:%s\n", wrapper->name);
    ret = wrapper->open (wrapper, stm_ctx->stream_name);
    if (ret < 0)
    {
        dt_error (TAG, "stream open failed\n");
        return -1;
    }
    dt_info (TAG, "stream open ok\n");
    return 0;
}

int stream_eof (dtstream_context_t *stm_ctx)
{
    stream_wrapper_t *wrapper = stm_ctx->stream;
    stream_ctrl_t *info = &wrapper->info;
    return info->eof_flag;
}

int64_t stream_tell (dtstream_context_t *stm_ctx)
{
    stream_wrapper_t *wrapper = stm_ctx->stream;
    stream_ctrl_t *info = &wrapper->info;
    return info->cur_pos;
}

int64_t stream_get_size (dtstream_context_t *stm_ctx)
{
    stream_wrapper_t *wrapper = stm_ctx->stream;
    stream_ctrl_t *info = &wrapper->info;
    return info->stream_size;
}

int stream_read (dtstream_context_t *stm_ctx, uint8_t *buf,int len)
{
    stream_wrapper_t *wrapper = stm_ctx->stream;
    return wrapper->read(wrapper,buf,len);
}

int stream_seek (dtstream_context_t *stm_ctx, int64_t pos,int whence)
{
    stream_wrapper_t *wrapper = stm_ctx->stream;
    return wrapper->seek(wrapper,pos,whence);
}

int stream_close (dtstream_context_t *stm_ctx)
{
    stream_wrapper_t *wrapper = stm_ctx->stream;
    wrapper->close(wrapper);
    return 0;
}
