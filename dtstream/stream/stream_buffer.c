#include "dtstream.h"
#include "dt_error.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define TAG "STREAM-BUFFER"

#ifndef AVSEEK_SIZE 
#define AVSEEK_SIZE 0x10000
#endif

const int DEFAULT_STREAM_BUFSIZE = 1024*1024*32; // 32m
const int READ_PER_TIME = 10*1024; // read 10k bytes per time

typedef enum{
    ST_BUFFER_IDLE = 0,
    ST_BUFFER_PAUSED,
    ST_BUFFER_RUNNING,
    ST_BUFFER_QUIT,
}st_status_t;

const int ST_FLAG_NULL = 0;
const int ST_FLAG_PAUSE = 2;

typedef struct{
    dt_buffer_t sbuf;
    pthread_t fill_tid;
    st_status_t status;
    int flag;
    int eof;
    stream_wrapper_t *wrapper; // point to real stream
}streambuf_ctx_t;

static void *data_fill_thread (stream_wrapper_t * wrapper);

static int create_fill_thread (streambuf_ctx_t * ctx)
{
    pthread_t tid;
    ctx->status = ST_BUFFER_IDLE;
    ctx->flag = ST_FLAG_NULL;
    int ret = pthread_create (&tid, NULL, (void *) &data_fill_thread, (void *)ctx);
    if (ret != 0)
    {
        dt_error (TAG "file:%s [%s:%d] data fill thread crate failed \n", __FILE__, __FUNCTION__, __LINE__);
        return -1;
    }
    ctx->fill_tid = tid;
    ctx->status = ST_BUFFER_RUNNING;
    dt_info (TAG, "data fill Thread start ok\n");
    return 0;
}

int pause_fill_thread (streambuf_ctx_t * ctx)
{
    ctx->flag = ST_FLAG_PAUSE;
    while (ctx->status != ST_BUFFER_PAUSED)
        usleep (100);
    ctx->flag = ST_FLAG_NULL;
    return 0;
}

int resume_fill_thread (streambuf_ctx_t * ctx)
{
    ctx->flag = ST_FLAG_NULL;
    ctx->status = ST_BUFFER_RUNNING;
    return 0;
}

int stop_fill_thread (streambuf_ctx_t * ctx)
{
    ctx->flag = ST_FLAG_NULL;
    ctx->status = ST_BUFFER_QUIT;
    pthread_join (ctx->fill_tid, NULL);
    return 0;
}

static void *data_fill_thread (stream_wrapper_t * wrapper)
{
    streambuf_ctx_t *ctx = (streambuf_ctx_t *)wrapper->stream_priv;
    stream_wrapper_t *real_st = ctx->wrapper;
    dt_buffer_t *sbuf = &ctx->sbuf;
  
    uint8_t tmp_bufp[READ_PER_TIME];
    int rlen = 0;
    int wlen = 0;

    do
    {
        usleep (100);
        
        if(ctx->flag == ST_FLAG_PAUSE)
            ctx->status = ST_BUFFER_PAUSED;
       
        if(ctx->status == ST_BUFFER_IDLE)
            continue;

        if(ctx->status == ST_BUFFER_QUIT)
            goto QUIT;

        if(ctx->status == ST_BUFFER_PAUSED)
        {
            rlen = 0;
            buf_reinit(sbuf);
            continue;
        }

        if(rlen == 0)
        {
            rlen = real_st->read(real_st,tmp_bufp,READ_PER_TIME);
            if(rlen < 0)
            {
                ctx->eof = 1;
                ctx->status = ST_BUFFER_QUIT;
                ctx->flag = ST_FLAG_NULL;
                dt_info(TAG,"read eof quit poll thread \n");
                goto QUIT;
            }
            dt_debug (TAG, "read ok size:%d \n",rlen);
        }
        
        if(buf_space(sbuf) < rlen)
            continue;
        wlen = buf_put(sbuf,tmp_bufp,rlen); 
        if (wlen == 0)
            continue;
        rlen -= wlen;
        if(rlen > 0) // here some err occured, skip this packet
        {
            rlen = 0;
            continue;
        }
    }
    while (1);
QUIT:
    dt_info (TAG, "data fill thread quit ok\n");
    pthread_exit (NULL);
    return 0;
}

static int stream_buffer_open (stream_wrapper_t * wrapper,char *stream_name)
{
    int ret = 0;
    stream_ctrl_t *info = &wrapper->info;
    memset(info,0,sizeof(*info));

    //open real stream
    stream_wrapper_t *real_st = (stream_wrapper_t *)wrapper->stream_priv;
    ret = real_st->open(real_st,stream_name);
    if(ret != DTERROR_NONE)
    {
        ret = DTERROR_FAIL;
        goto ERR0;
    }
    memcpy(info,&real_st->info,sizeof(stream_ctrl_t));
    
    // ctx
    streambuf_ctx_t *ctx = malloc(sizeof(streambuf_ctx_t));
    if(!ctx)
    {
        dt_info(TAG,"streambuf_ctx_t malloc failed, ret\n");
        ret = DTERROR_FAIL;
        goto ERR1;
    }
    memset(ctx,0,sizeof(streambuf_ctx_t));
    ctx->wrapper = real_st;
    //set buf size
    char value[512];
    int buffer_size = DEFAULT_STREAM_BUFSIZE;
    if(GetEnv("STREAM","stream.bufsize",value) > 0)
    {
        buffer_size = atoi(value);
        dt_info(TAG,"buffer size:%d \n",buffer_size);
    }
    else
        dt_info(TAG,"buffer size not set, use default:%d \n",buffer_size);
    // get tmp buffer
    dt_buffer_t *sbuf = &(ctx->sbuf);
    ret = buf_init(sbuf,buffer_size); 
    if(ret != DTERROR_NONE)
    {
        ret = DTERROR_FAIL;
        
        goto ERR1;
    }
    wrapper->stream_priv = ctx;

    //start read thread
    ret = create_fill_thread(ctx);
    if (ret == -1)
    {
        dt_error (TAG "file:%s [%s:%d] data fill thread start failed \n", __FILE__, __FUNCTION__, __LINE__);
        goto ERR2;
    }
    return DTERROR_NONE;
ERR2:
    buf_release(sbuf);
ERR1:
    real_st->close(real_st);
ERR0:
    return ret;
}

static int stream_buffer_read (stream_wrapper_t * wrapper,uint8_t *buf,int len)
{
    streambuf_ctx_t *ctx = (streambuf_ctx_t *)wrapper->stream_priv;
    stream_ctrl_t *info = &wrapper->info;
    dt_buffer_t *sbuf = &ctx->sbuf;

    int r = buf_get(sbuf,buf,len);
    dt_info(TAG,"read %d byte \n",r);
    if(r>0)
        info->cur_pos += r;
    if(r<=0 && ctx->eof == 1)
    {
        info->eof_flag = 1;
        return -1;
    }
    return r;
}

static int stream_buffer_seek (stream_wrapper_t * wrapper, int64_t pos, int whence)
{
    int ret = 0;
    streambuf_ctx_t *ctx = (streambuf_ctx_t *)wrapper->stream_priv;
    stream_wrapper_t *real_st = ctx->wrapper;
    stream_ctrl_t *info = &wrapper->info;
    dt_buffer_t *sbuf = &ctx->sbuf;

    pause_fill_thread(ctx);
    
    ret ==real_st->seek(real_st,pos,whence);
    if(ret != DTERROR_NONE)
        dt_error(TAG,"SEEK FAILED \n");
    
    resume_fill_thread(ctx);

    return ret;
}

static int stream_buffer_close (stream_wrapper_t * wrapper)
{
    streambuf_ctx_t *ctx = (streambuf_ctx_t *)wrapper->stream_priv;
    stream_wrapper_t *real_st = ctx->wrapper;
    stream_ctrl_t *info = &wrapper->info;
    dt_buffer_t *sbuf = &ctx->sbuf;

    stop_fill_thread(ctx);
    real_st->close(real_st);
    buf_release(ctx);
    return 0;
}

stream_wrapper_t stream_buffer = {
    .name = "buffer",
    .id = STREAM_BUFFER,
    .open = stream_buffer_open,
    .read = stream_buffer_read,
    .seek = stream_buffer_seek,
    .close = stream_buffer_close,
};
