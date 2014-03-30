#ifdef ENABLE_AO_ALSA

#include "../dtaudio_output.h"
#include <alsa/asoundlib.h>

#define TAG "AUDIO-OUT-ALSA"

#define DEFAULT_TIME_SIZE 3000  //3s

typedef struct{
    snd_pcm_t *handle;
    int buf_size;
    int buf_level;
    int buf_threshold;
    int trunk_size;
    int pause_support;

    int channels;
    int samplerate;
    int bps;
}alsa_ctx_t;

static snd_pcm_format_t format_to_alsa (int fmt)
{
    switch (fmt)
    {
    case 8:
        return SND_PCM_FORMAT_S8;
    case 16:
        return SND_PCM_FORMAT_S16_LE;
    case 24:
        return SND_PCM_FORMAT_S24_LE;
    case 32:
        return SND_PCM_FORMAT_S32_LE;
    default:
        return SND_PCM_FORMAT_S16_LE;
    }
}

static int ao_alsa_init (ao_wrapper_t *wrapper, void *parent)
{
    dtaudio_output_t *ao = (dtaudio_output_t *)parent; 
    wrapper->parent = parent;

    snd_pcm_t *alsa_handle;
    snd_pcm_hw_params_t *alsa_hwparams;
    snd_pcm_sw_params_t *alsa_swparams;
    snd_pcm_uframes_t size;
    snd_pcm_uframes_t boundary;

    alsa_ctx_t *ctx = (alsa_ctx_t *)malloc(sizeof(*ctx));
    if(!ctx)
        return -1;
    wrapper->ao_priv = ctx;

    int afmt = format_to_alsa (ao->para.bps);
    uint32_t channels = ao->para.channels;
    uint32_t sr = ao->para.samplerate;
    int bytes_per_sample = snd_pcm_format_physical_width (afmt) *channels / 8;
   
    int err = 0;
    err = snd_pcm_open (&alsa_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0)
    {
        dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
        free(ctx);
        wrapper->ao_priv = NULL;
        return -1;
    }

    snd_pcm_hw_params_alloca (&alsa_hwparams);
    snd_pcm_sw_params_alloca (&alsa_swparams);
    
    err = snd_pcm_hw_params_any (alsa_handle, alsa_hwparams);
    if (err < 0)
    {
        dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
        return -1;
    }

    err = snd_pcm_hw_params_set_access (alsa_handle, alsa_hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0)
    {
        dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
        return -1;
    }
    
    err = snd_pcm_hw_params_set_format (alsa_handle, alsa_hwparams, afmt);
    if (err < 0)
    {
        dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
        return -1;
    }
    
    err = snd_pcm_hw_params_set_channels_near (alsa_handle, alsa_hwparams, &channels);
    if (err < 0)
    {
        dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
        return -1;
    }

    err = snd_pcm_hw_params_set_rate_near (alsa_handle, alsa_hwparams, &sr, NULL);
    if (err < 0)
    {
        dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
        return -1;
    }

    err = snd_pcm_hw_params (alsa_handle, alsa_hwparams);
    if (err < 0)
    {
        dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
        return -1;
    }
    /******************************************************************
	 *  Set HW Params Finish
	******************************************************************/

    /* buffer size means the entire size of alsa pcm buffer size*/
    err = snd_pcm_hw_params_get_buffer_size (alsa_hwparams, &size);
    if (err < 0)
    {
        dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
        return -1;
    }
    
    ctx->buf_size = size * bytes_per_sample;
   
    /*period size means count processed every interrupt*/
    err = snd_pcm_hw_params_get_period_size (alsa_hwparams, &size, NULL);
    if (err < 0)
    {
        dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
        return -1;
    }
    ctx->trunk_size = size * bytes_per_sample;
    
    dt_info (TAG, "outburst: %d, bytes_per_sample: %d, buffersize: %d\n", ctx->trunk_size, bytes_per_sample, ctx->buf_size);

    /******************************************************************
	* set sw params
	******************************************************************/
    err = snd_pcm_sw_params_current (alsa_handle, alsa_swparams);
    if (err < 0)
    {
        dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
        return -1;
    }
#if SND_LIB_VERSION >= 0x000901
    err = snd_pcm_sw_params_get_boundary (alsa_swparams, &boundary);
#else
    boundary = 0x7fffffff;
#endif
    err = snd_pcm_sw_params_set_start_threshold (alsa_handle, alsa_swparams, size);
    if (err < 0)
    {
        dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
        return -1;
    }
    err = snd_pcm_sw_params_set_stop_threshold (alsa_handle, alsa_swparams, boundary);
    if (err < 0)
    {
        dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
        return -1;
    }
#if SND_LIB_VERSION >= 0x000901
    err = snd_pcm_sw_params_set_silence_size (alsa_handle, alsa_swparams, boundary);
    if (err < 0)
    {
        dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
        return -1;
    }
#endif

    err = snd_pcm_sw_params (alsa_handle, alsa_swparams);
    if (err < 0)
    {
        dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
        return -1;
    }

    ctx->pause_support = snd_pcm_hw_params_can_pause (alsa_hwparams);
    
    //buf size limit
    ctx->buf_threshold = bytes_per_sample * sr * DEFAULT_TIME_SIZE / 1000;
    dt_info (TAG, "alsa audio init ok! outburst:%d thres:%d \n", ctx->trunk_size,ctx->buf_threshold);
    ctx->handle = alsa_handle;
    ctx->channels = ao->para.channels;
    ctx->bps = ao->para.bps;
    ctx->samplerate = ao->para.samplerate;
    return 0;
}

static int ao_alsa_level (ao_wrapper_t *wrapper)
{
    alsa_ctx_t *ctx = (alsa_ctx_t *)wrapper->ao_priv;
    return ctx->buf_level;
}

#define ALSA_RESERVE_THRESHOLD 10*1024
static int ao_alsa_play (ao_wrapper_t * wrapper, uint8_t * buf, int size)
{
    alsa_ctx_t *ctx = (alsa_ctx_t *)wrapper->ao_priv;
    snd_pcm_t *alsa_handle = (snd_pcm_t *) ctx->handle;
    dtaudio_output_t *ao = (dtaudio_output_t *)wrapper->parent; 
    
    int bytes_per_sample = ao->para.bps * ao->para.channels / 8;
    int num_frames = size / bytes_per_sample;
    snd_pcm_sframes_t res = 0;
    uint8_t *data = buf;

    if (!alsa_handle)
        return -1;

    if (num_frames == 0)
        return 0;

    if (ao_alsa_level (wrapper) >= ctx->buf_threshold)
    {
        dt_debug (TAG, "ALSA EXCEED THRESHOLD,size:%d  thres:%d \n", ao_alsa_level (wrapper), ctx->buf_threshold);
        return 0;
    }

    res = snd_pcm_writei (alsa_handle, data, num_frames);
    if (res == -EINTR)
    {
        dt_info (TAG, "ALSA HAS NO SPACE, WRITE AGAIN\n");
        return res;
    }
    if (res == -ESTRPIPE)
    {
        snd_pcm_resume (alsa_handle);
        return res;
    }
    if (res == -EBADFD)
    {
        snd_pcm_reset (alsa_handle);
        return res;
    }
    if (res < 0)
    {
        snd_pcm_prepare (alsa_handle);
        dt_info (TAG, "snd pcm write failed prepare!\n");
        return -1;
        //goto rewrite;
    }
    if (res < num_frames)
    {
        data += res * bytes_per_sample;
        num_frames -= res;
        //goto rewrite;
    }
    return res * bytes_per_sample;
}

static int ao_alsa_space (alsa_ctx_t *ctx)
{
    snd_pcm_t *alsa_handle = (snd_pcm_t *) ctx->handle;

    int ret;
    snd_pcm_status_t *status;
    int bytes_per_sample = ctx->bps * ctx->channels / 8;
    snd_pcm_status_alloca (&status);

    if ((ret = snd_pcm_status (alsa_handle, status)) < 0)
    {
        dt_info ("%s,%d: %s\n", __FUNCTION__, __LINE__, snd_strerror (ret));
        return 0;
    }
    ret = snd_pcm_status_get_avail (status) * bytes_per_sample;
    if (ret > ctx->buf_size)
        ret = ctx->buf_size;
    
    return ret;
}

static int ao_alsa_pause (ao_wrapper_t *wrapper)
{
    alsa_ctx_t *ctx = (alsa_ctx_t *)wrapper->ao_priv;
    snd_pcm_t *handle = (snd_pcm_t *) ctx->handle;
    
    int ret = 0;
    if (ctx->pause_support == 1)
    {
        ret = snd_pcm_pause (handle, 1);
        dt_info (TAG, "ALSA PAUSED SUPPORT PAUSE DIRECTLY  ret:%d status:%d\n", ret, snd_pcm_state (handle));
    }
    else
        snd_pcm_drop (handle);
    dt_info (TAG, "ALSA PAUSED \n");
    return 0;
}

static int ao_alsa_resume (ao_wrapper_t *wrapper)
{
    alsa_ctx_t *ctx = (alsa_ctx_t *)wrapper->ao_priv;
    snd_pcm_t *handle = (snd_pcm_t *) ctx->handle;
 
    int ret = 0;
    if (ctx->pause_support == 1)
    {
        ret = snd_pcm_pause (handle, 0);
        dt_info (TAG, "ALSA PAUSED SUPPORT RESUME DIRECTLY  ret:%d status:%d \n", ret, snd_pcm_state (handle));
    }
    else
        snd_pcm_prepare (handle);
    dt_info (TAG, "ALSA RESUME \n");
    return 0;
}

static int64_t ao_alsa_get_latency (ao_wrapper_t *wrapper)
{
    alsa_ctx_t *ctx = (alsa_ctx_t *)wrapper->ao_priv;
    dtaudio_output_t *ao = (dtaudio_output_t *)wrapper->parent;

    unsigned int sample_num;
    uint64_t latency, latency_s;
    
    ctx->buf_level = ctx->buf_size - ao_alsa_space (ctx);
    sample_num = ctx->buf_level / (ao->para.channels * ao->para.bps / 8);
    
    float pts_ratio = (double) 90000 / ao->para.samplerate;
    latency = (sample_num * pts_ratio);
    latency_s = latency/90000;

    dt_debug (TAG, "[%s:%d]==alsa_level:%d thres:%d sample_num:%d buffersize:%d latency:%llu latency_s:%llu \n", __FUNCTION__, __LINE__, ctx->buf_level, ctx->buf_threshold, sample_num, ctx->buf_size, latency, latency_s);
    return latency;
}
static int ao_alsa_stop (ao_wrapper_t *wrapper)
{
    alsa_ctx_t *ctx = (alsa_ctx_t *)wrapper->ao_priv;
    if(!ctx)
        return 0;
    
    snd_pcm_t *alsa_handle = (snd_pcm_t *) ctx->handle;
    if (alsa_handle)
    {
        snd_pcm_drop (alsa_handle);
        snd_pcm_drain (alsa_handle);
        snd_pcm_close (alsa_handle);
    }
    free(ctx);
    wrapper->ao_priv = NULL;

    return 0;
}


ao_wrapper_t ao_alsa_ops = {
    .id = AO_ID_ALSA,
    .name = "alsa",
    .ao_init = ao_alsa_init,
    .ao_pause = ao_alsa_pause,
    .ao_resume = ao_alsa_resume,
    .ao_stop = ao_alsa_stop,
    .ao_write = ao_alsa_play,
    .ao_latency = ao_alsa_get_latency,
    .ao_level = ao_alsa_level,
};
#endif /* ENABLE_AO_ALSA */
