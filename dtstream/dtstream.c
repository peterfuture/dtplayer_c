#include "dtstream.h"

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
    wrapper->next = NULL;
}

static void stream_register_all ()
{
    REGISTER_STREAM (FILE, file);
}

static int stream_select (dtstream_context_t * stm_ctx)
{
    if (!g_stream)
        return -1;
    stm_ctx->stream = g_stream; // select the only one
    return 0;
}

int stream_open (dtstream_context_t * stm_ctx)
{
    int ret = 0;
    /*register streamer */
    stream_register_all ();
    if (stream_select (stm_ctx) == -1)
    {
        dt_error (TAG, "select stream failed \n");
        return -1;
    }
    stream_wrapper_t *wrapper = stm_ctx->stream;
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
