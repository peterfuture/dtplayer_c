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

static void stream_unregister_all ()
{
    g_stream = NULL;
    return;
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

int64_t stream_tell (dtstream_context_t *stm_ctx)
{
    stream_wrapper_t *wrapper = stm_ctx->stream;
    return wrapper->tell(wrapper);
}

int stream_read (dtstream_context_t *stm_ctx, char *buf,int len)
{
    stream_wrapper_t *wrapper = stm_ctx->stream;
    return wrapper->read(wrapper,buf,len);
}

int stream_seek (dtstream_context_t *stm_ctx, int64_t pos)
{
    stream_wrapper_t *wrapper = stm_ctx->stream;
    return wrapper->seek(wrapper,pos);
}

int stream_close (dtstream_context_t *stm_ctx)
{
    stream_wrapper_t *wrapper = stm_ctx->stream;
    wrapper->close(wrapper);
    stream_unregister_all();
    return 0;
}
