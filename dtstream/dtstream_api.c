#include "dtstream_api.h"
#include "dtstream.h"

#define TAG "STREAM_API"

int dtstream_open (void **priv, dtstream_para_t * para, void *parent)
{
    dtstream_context_t *ctx = (dtstream_context_t *)malloc(sizeof(dtstream_context_t));
    if(!ctx)
    {
        dt_error(TAG,"STREAM CTX MALLOC FAILED \n");
        return -1;
    }
    memset (ctx, 0, sizeof (dtstream_context_t));
    ctx->stream_name = para->stream_name;
    if(stream_open(ctx) == -1)
    {
        dt_error(TAG,"STREAM CONTEXT OPEN FAILED \n");
        free(ctx);
        *priv = NULL;
        return -1;
    }
    *priv = (void *)ctx;
    ctx->parent = parent;
    dt_info(TAG,"STREAM CTX OPEN SUCCESS\n");
    return 0;
}

int64_t dtstream_get_size(void *priv)
{
    dtstream_context_t *stm_ctx = (dtstream_context_t *) priv;
    return stream_get_size(stm_ctx);
}

int dtstream_eof (void *priv)
{
    dtstream_context_t *stm_ctx = (dtstream_context_t *) priv;
    return stream_eof(stm_ctx);
}

int64_t dtstream_tell (void *priv)
{
    dtstream_context_t *stm_ctx = (dtstream_context_t *) priv;
    return stream_tell(stm_ctx);
}

/*
 * skip size byte 
 * maybe negitive , then seek forward
 *
 * */
int dtstream_skip (void *priv, int64_t size)
{
    dtstream_context_t *stm_ctx = (dtstream_context_t *) priv;
    stream_seek(stm_ctx,size,SEEK_CUR);
    return 1;
}

int dtstream_read (void *priv, uint8_t *buf,int len)
{
    dtstream_context_t *stm_ctx = (dtstream_context_t *) priv;
    return stream_read(stm_ctx,buf,len);
}

int dtstream_seek (void *priv, int64_t pos ,int whence)
{
    dtstream_context_t *stm_ctx = (dtstream_context_t *) priv;
    return stream_seek(stm_ctx,pos,whence);
}

int dtstream_close (void *priv)
{
    dtstream_context_t *stm_ctx = (dtstream_context_t *) priv;
    if(stm_ctx)
    {
        stream_close(stm_ctx);
        free(stm_ctx);
    }
    priv = NULL;
    return 0;
}
