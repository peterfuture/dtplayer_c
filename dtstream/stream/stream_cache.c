#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "dt_utils.h"
#include "dt_macro.h"
#include "dt_error.h"
#include "dt_setting.h"

#include "dtstream.h"

#define TAG "STREAM-CACHE"

/*
 * stream-cache info
 * stream cache used to buffer data from real stream,
 * member:
 *
 * can_seek_here
 * |-------------------------------------------|
 *       |          |              |
 *    pre_rp        rp             wp
 * -------------------------------------------
 *
 * buf: data memory
 * pre_rp: can seek back here, for fast seek
 * rp: cur read pointer
 * wp: cur write pointer
 *
 * for online video, cache can support fast seek
 * through preserved data by pre_rp
 *
 * */


#ifndef AVSEEK_SIZE
#define AVSEEK_SIZE 0x10000
#endif

const int DEFAULT_CACHE_SIZE = 1024 * 1024 * 32; // 32m
const int READ_PER_TIME = 10 * 1024; // read 10k bytes per time
const float f_pre = 0.2; // at least preserve total*0.2 bytes data
const float f_post = 0.8;
typedef struct stream_cache {
    uint8_t *data;
    int level;
    int total_size;
    int pre_size;
    int fix_pre_size;
    //int post_size;

    int pre_rp;
    int rp;
    int wp;
    //int post_wp;
    dt_lock_t mutex;
} stream_cache_t;

static stream_cache_t * create_cache(int size, float pre)
{
    if (size <= 0) {
        goto ERR0;
    }

    stream_cache_t *cache = (stream_cache_t *)malloc(sizeof(stream_cache_t));
    if (!cache) {
        dt_error(TAG, "cache ctx failed");
        goto ERR0;
    }
    memset(cache, 0, sizeof(stream_cache_t));
    cache->data = (uint8_t *)malloc(size);
    if (!cache->data) {
        dt_error(TAG, "cache data malloc failed");
        goto ERR1;
    }
    cache->total_size = size;
    cache->fix_pre_size = size * pre;
    //cache->post_size = size * post;

    dt_lock_init(&cache->mutex, NULL);
    dt_info(TAG, "Create cache ok, total:%d fixpresize:%d pre:%f \n",
            cache->total_size, cache->fix_pre_size, pre);
    return cache;
ERR1:
    free(cache);
ERR0:
    return NULL;
}

static int cache_read(stream_cache_t *cache, uint8_t *out, int size)
{
    dt_lock(&cache->mutex);
    int len = MIN(cache->level, size);
    if (len == 0) {
        goto QUIT;
    }

    if (cache->wp > cache->rp) {
        memcpy(out, cache->data + cache->rp, len);
        cache->rp += len;
        cache->level -= len;
        cache->pre_size += len;
    } else if (len <= (int)(cache->total_size - cache->rp)) {
        memcpy(out, cache->data + cache->rp, len);
        cache->rp += len;
        cache->level -= len;
        cache->pre_size += len;
    } else {
        int tail_len = (int)(cache->total_size - cache->rp);
        memcpy(out, cache->data + cache->rp, tail_len);
        memcpy(out + tail_len, cache->data, len - tail_len);
        cache->rp = len - tail_len;
        cache->level -= len;
        cache->pre_size += len;
    }
QUIT:
    dt_unlock(&cache->mutex);
    return len;
}

static int cache_level(stream_cache_t *cache)
{
    return (cache->level);
}

static int cache_space(stream_cache_t *cache)
{
    return (cache->total_size - cache->level - cache->fix_pre_size);
}

static int cache_write(stream_cache_t *cache, uint8_t *in, int size)
{
    dt_lock(&cache->mutex);
    int len = MIN(cache->total_size - cache->level - cache->fix_pre_size, size);
    if (len == 0) {
        goto QUIT;
    }

    if (cache->wp < cache->rp) {
        if (cache->wp + len >= cache->pre_rp) {
            dt_info(TAG, "[%s:%d]need to move pre from %d to %d \n", __FUNCTION__, __LINE__,
                    cache->pre_rp, cache->wp + len);
            cache->pre_rp = cache->wp + len;
            cache->pre_size = cache->rp - cache->pre_rp;
        }
        memcpy(cache->data + cache->wp, in, len);
        cache->wp += len;
        cache->level += len;
    } else if (len <= (int)(cache->total_size - cache->wp)) {
        if (cache->pre_rp > cache->rp) {
            if ((cache->wp + len) > cache->pre_rp) {
                dt_info(TAG, "[%s:%d]need to move pre from %d to %d \n", __FUNCTION__, __LINE__,
                        cache->pre_rp, cache->wp + len);
                cache->pre_rp = cache->wp + len;
                cache->pre_size = cache->rp + cache->total_size - cache->pre_size;
            }
        }
#if 0
        else if (cache->pre_rp > cache->rp) {
            cache->pre_size = cache->total_size - cache->pre_rp + cache->rp;
        } else {
            cache->pre_size = cache->rp - cache->pre_rp;
        }
#endif
        memcpy(cache->data + cache->wp, in, len);
        cache->wp += len;
        cache->level += len;
    } else {
        int tail_len = (int)(cache->total_size - cache->wp);
        memcpy(cache->data + cache->wp, in, tail_len);
        if (cache->pre_rp > cache->rp) {
            dt_info(TAG, "[%s:%d]need to move pre from %d to %d \n", __FUNCTION__, __LINE__,
                    cache->pre_rp, cache->wp + len);
            cache->pre_rp = len - tail_len;
            cache->pre_size = cache->rp - cache->pre_rp;
        } else if (cache->pre_rp < (len - tail_len)) {
            dt_info(TAG, "[%s:%d]need to move pre from %d to %d \n", __FUNCTION__, __LINE__,
                    cache->pre_rp, cache->wp + len);
            cache->pre_rp = len - tail_len;
            cache->pre_size = cache->rp - cache->pre_rp;
        }
        memcpy(cache->data, in + tail_len, len - tail_len);
        cache->wp = len - tail_len;
        cache->level += len;
    }

QUIT:
    dt_unlock(&cache->mutex);
    return len;
}

/*
 * cache - context
 * step - seek step bytes
 * orig - 0 back 1 forward
 *
 * ret : 0 ok -1 failed
 * */

static int cache_seek(stream_cache_t *cache, int step, int orig)
{
    if (step <= 0) {
        return 0;
    }
    dt_lock(&cache->mutex);
    int ret = 0;

    if (!orig) { // seek backward
        if (cache->pre_size < step) { // seek out of bound, failed
            dt_info(TAG, "[%s:%d]rp:%d wp:%d pre_rp:%d level:%d pre_size:%d \n",
                    __FUNCTION__, __LINE__, cache->rp, cache->wp, cache->pre_rp, cache->level,
                    cache->pre_size);
            ret = -1;
            goto QUIT;
        }

        if (cache->pre_rp < cache->rp) {
            dt_info(TAG, "[%s:%d]rp:%d wp:%d pre_rp:%d level:%d pre_size:%d \n",
                    __FUNCTION__, __LINE__, cache->rp, cache->wp, cache->pre_rp, cache->level,
                    cache->pre_size);
            cache->rp -= step;
            cache->pre_size -= step;
            cache->level += step;
            goto QUIT;
        }

        if (cache->pre_rp > cache->rp) {
            if (cache->rp > step) {
                dt_info(TAG, "[%s:%d]rp:%d wp:%d pre_rp:%d level:%d pre_size:%d \n",
                        __FUNCTION__, __LINE__, cache->rp, cache->wp, cache->pre_rp, cache->level,
                        cache->pre_size);
                cache->rp -= step;
                cache->pre_size -= step;
                cache->level += step;
                goto QUIT;
            } else {
                dt_info(TAG, "[%s:%d]rp:%d wp:%d pre_rp:%d level:%d pre_size:%d \n",
                        __FUNCTION__, __LINE__, cache->rp, cache->wp, cache->pre_rp, cache->level,
                        cache->pre_size);
                int left = step - cache->rp;
                cache->rp = cache->total_size - left;
                cache->pre_size -= step;
                cache->level += step;
                goto QUIT;

            }
        }
        ret = -1; // failed here
        goto QUIT;
    }

    if (orig) {
        if (cache->level < step) { // seek out of bound, failed
            dt_info(TAG, "[%s:%d]rp:%d wp:%d pre_rp:%d level:%d pre_size:%d \n",
                    __FUNCTION__, __LINE__, cache->rp, cache->wp, cache->pre_rp, cache->level,
                    cache->pre_size);
            ret = -1;
            goto QUIT;
        }

        if (cache->rp < cache->wp) {
            dt_info(TAG, "[%s:%d]rp:%d wp:%d pre_rp:%d level:%d pre_size:%d \n",
                    __FUNCTION__, __LINE__, cache->rp, cache->wp, cache->pre_rp, cache->level,
                    cache->pre_size);
            cache->rp += step;
            cache->pre_size += step;
            cache->level -= step;
            goto QUIT;
        }

        if (cache->rp > cache->wp) {
            if ((cache->total_size - cache->rp) > step) {
                dt_info(TAG, "[%s:%d]rp:%d wp:%d pre_rp:%d level:%d pre_size:%d \n",
                        __FUNCTION__, __LINE__, cache->rp, cache->wp, cache->pre_rp, cache->level,
                        cache->pre_size);
                cache->rp += step;
                cache->pre_size += step;
                cache->level -= step;
                goto QUIT;
            } else {
                dt_info(TAG, "[%s:%d]rp:%d wp:%d pre_rp:%d level:%d pre_size:%d \n",
                        __FUNCTION__, __LINE__, cache->rp, cache->wp, cache->pre_rp, cache->level,
                        cache->pre_size);
                int left = step - (cache->total_size - cache->rp);
                cache->rp = left;
                cache->pre_size += step;
                cache->level -= step;
                goto QUIT;
            }

            ret = -1;
            goto QUIT;
        }

        ret = -1;
        goto QUIT;
    }
QUIT:
    dt_info(TAG, "cache seek, step:%d ret:%d orig:%d \n", step, ret, orig);
    dt_unlock(&cache->mutex);
    return ret;
}

static int cache_reset(stream_cache_t *cache)
{
    dt_lock(&cache->mutex);
    cache->rp = cache->wp = cache->pre_rp = 0;
    cache->pre_size = cache->level = 0;
    dt_unlock(&cache->mutex);
    return 0;
}

static int release_cache(stream_cache_t *cache)
{
    if (!cache) {
        return 0;
    }
    dt_lock(&cache->mutex);
    if (cache->data) {
        free(cache->data);
    }
    free(cache);
    return 0;
}


const int ST_FLAG_NULL = 0;
const int ST_FLAG_PAUSE = 1;
const int ST_FLAG_EOF = 2;

typedef enum {
    ST_STATUS_IDLE = 0,
    ST_STATUS_PAUSED,
    ST_STATUS_RUNNING,
    ST_STATUS_QUIT,
} st_status_t;

typedef struct {
    stream_cache_t *cache;
    int cache_flag;
    st_status_t cache_status;
    pthread_t cache_tid;
    stream_wrapper_t *wrapper; // point to real stream
} cache_ctx_t;

static void *cache_thread(cache_ctx_t * ctx);

static int create_cache_thread(cache_ctx_t * ctx)
{
    pthread_t tid;
    ctx->cache_status = ST_STATUS_IDLE;
    ctx->cache_flag = ST_FLAG_NULL;
    int ret = pthread_create(&tid, NULL, (void *) &cache_thread, (void *)ctx);
    if (ret != 0) {
        dt_error(TAG, "file:%s [%s:%d] data fill thread crate failed \n", __FILE__,
                 __FUNCTION__, __LINE__);
        return -1;
    }
    ctx->cache_tid = tid;
    ctx->cache_status = ST_STATUS_RUNNING;
    dt_info(TAG, "cache Thread create ok\n");
    return 0;
}

int pause_cache_thread(cache_ctx_t * ctx)
{
    ctx->cache_flag = ST_FLAG_PAUSE;
    while (ctx->cache_status != ST_STATUS_IDLE) {
        usleep(100);
    }
    ctx->cache_flag = ST_FLAG_NULL;
    dt_info(TAG, "pause cache thread ok \n");
    return 0;
}

int resume_cache_thread(cache_ctx_t * ctx)
{
    ctx->cache_flag = ST_FLAG_NULL;
    ctx->cache_status = ST_STATUS_RUNNING;
    dt_info(TAG, "resume cache thread ok \n");
    return 0;
}

int stop_cache_thread(cache_ctx_t * ctx)
{
    ctx->cache_flag = ST_FLAG_NULL;
    ctx->cache_status = ST_STATUS_QUIT;
    pthread_join(ctx->cache_tid, NULL);
    return 0;
}

static void *cache_thread(cache_ctx_t * ctx)
{
    stream_wrapper_t *real_st = ctx->wrapper;
    stream_cache_t *cache = ctx->cache;

    uint8_t tmp_buf[READ_PER_TIME];
    int rlen = 0;
    int wlen = 0;

    do {
        usleep(10000);

        if (ctx->cache_flag == ST_FLAG_PAUSE) {
            ctx->cache_status = ST_STATUS_PAUSED;
        }

        if (ctx->cache_status == ST_STATUS_IDLE) {
            usleep(100000);
            continue;
        }

        if (ctx->cache_status == ST_STATUS_QUIT) {
            goto QUIT;
        }

        if (ctx->cache_status == ST_STATUS_PAUSED) { // means clean everything
            rlen = 0;
            ctx->cache_flag = ST_FLAG_NULL;
            ctx->cache_status = ST_STATUS_IDLE;
            dt_info(TAG, "paused thread, reset rlen and cache\n");
            continue;
        }

        if (rlen == 0) {
            rlen = real_st->read(real_st, tmp_buf, READ_PER_TIME);
            if (rlen < 0) {
                rlen = 0;
                ctx->cache_flag = ST_FLAG_EOF; // eof will enter IDLE MODE
                ctx->cache_status = ST_STATUS_IDLE;
                dt_info(TAG, "read eof, poll thread enter idle\n");
                continue;
            }
            dt_debug(TAG, "read ok size:%d \n", rlen);
        }

        if (cache_space(cache) < rlen) {
            usleep(100000);
            continue;
        }

        wlen = cache_write(cache, tmp_buf, rlen);
        if (wlen == 0) {
            continue;
        }

        rlen -= wlen;
        if (rlen > 0) { // here some err occured, skip this packet
            rlen = 0;
            continue;
        }
    } while (1);
QUIT:
    dt_info(TAG, "cache thread quit ok\n");
    pthread_exit(NULL);
    return 0;
}

static int stream_cache_open(stream_wrapper_t * wrapper, char *stream_name)
{
    int ret = 0;
    stream_ctrl_t *info = &wrapper->info;
    memset(info, 0, sizeof(*info));

    //open real stream
    stream_wrapper_t *real_st = (stream_wrapper_t *)wrapper->stream_priv;
    ret = real_st->open(real_st, stream_name);
    if (ret != DTERROR_NONE) {
        ret = DTERROR_FAIL;
        goto ERR0;
    }
    memcpy(info, &real_st->info, sizeof(stream_ctrl_t));

    //get buf size
    int cache_size = dtp_setting.stream_cache_size;
    dt_info(TAG, "cache size:%d \n", cache_size);

    cache_ctx_t *ctx = (cache_ctx_t *)malloc(sizeof(cache_ctx_t));
    if (!ctx) {
        dt_info(TAG, "cache_ctx_t malloc failed, ret\n");
        ret = DTERROR_FAIL;
        goto ERR1;
    }
    memset(ctx, 0, sizeof(cache_ctx_t));
    ctx->wrapper = real_st;

    // get tmp buffer
    ctx->cache = create_cache(cache_size, f_pre);
    if (!ctx->cache) {
        ret = DTERROR_FAIL;
        goto ERR2;
    }
    wrapper->stream_priv = ctx;

    //start read thread
    ret = create_cache_thread(ctx);
    if (ret == -1) {
        dt_error(TAG, "file:%s [%s:%d] data fill thread start failed \n", __FILE__,
                 __FUNCTION__, __LINE__);
        goto ERR3;
    }
    return DTERROR_NONE;
ERR3:
    release_cache(ctx->cache);
ERR2:
    free(ctx);
ERR1:
    real_st->close(real_st);
ERR0:
    return ret;
}

static int stream_cache_read(stream_wrapper_t * wrapper, uint8_t *buf, int len)
{
    cache_ctx_t *ctx = (cache_ctx_t *)wrapper->stream_priv;
    stream_ctrl_t *info = &wrapper->info;
    stream_cache_t *cache = ctx->cache;

    int level = cache_level(cache);
    if (level == 0) {
        return 0;
    }
    int r = cache_read(cache, buf, len);
    dt_debug(TAG, "read %d byte \n", r);
    if (r > 0) {
        info->cur_pos += r;
    }
    if (r <= 0 && ctx->cache_flag == ST_FLAG_EOF) {
        if (ctx->cache_flag == ST_FLAG_EOF) {
            info->eof_flag = 1;
        } else {
            r = 0;
        }
    }
    return r;
}

static int stream_cache_seek(stream_wrapper_t * wrapper, int64_t pos,
                             int whence)
{
    int ret = 0;
    cache_ctx_t *ctx = (cache_ctx_t *)wrapper->stream_priv;
    stream_cache_t *cache = ctx->cache;
    stream_wrapper_t *real_st = ctx->wrapper;
    stream_ctrl_t *info = &wrapper->info;
    info->eof_flag = 0;
    dt_info(TAG, "Enter cache seek \n");
    // ret stream size
    if (whence == AVSEEK_SIZE) {
        dt_debug(TAG, "REQUEST STREAM SIZE:%lld \n", info->stream_size);
        return info->stream_size;
    }
    int step;
    int orig;
    if (whence == SEEK_SET) {
        step = abs(info->cur_pos - pos);
        orig = (info->cur_pos > pos) ? 0 : 1;
    }
    if (whence == SEEK_CUR) {
        step = abs(pos);
        orig = (pos < 0) ? 0 : 1;
    }

    if (cache_seek(cache, step, orig) == 0) { // seek in cache
        dt_info(TAG, "cache seek success \n");
    } else { // seek through real stream ops
        pause_cache_thread(ctx);
        ret = real_st->seek(real_st, pos,
                            whence); // fixme: seek maybe failed for online video
        if (ret != DTERROR_NONE) {
            dt_error(TAG, "SEEK FAILED \n");
        }
        cache_reset(ctx->cache);
        resume_cache_thread(ctx);
    }

    info->eof_flag = 0;
    if (whence == SEEK_SET) {
        info->cur_pos = pos;
    }
    if (whence == SEEK_CUR) {
        info->cur_pos += pos;
    }

    return ret;
}

static int stream_cache_close(stream_wrapper_t * wrapper)
{
    cache_ctx_t *ctx = (cache_ctx_t *)wrapper->stream_priv;
    stream_wrapper_t *real_st = ctx->wrapper;

    stop_cache_thread(ctx);
    release_cache(ctx->cache);
    real_st->close(real_st);
    free(ctx);
    return 0;
}

stream_wrapper_t stream_cache = {
    .name = "stream cache",
    .id = STREAM_CACHE,
    .open = stream_cache_open,
    .read = stream_cache_read,
    .seek = stream_cache_seek,
    .close = stream_cache_close,
};
