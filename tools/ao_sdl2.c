#include "dtaudio_output.h"
#include "dt_buffer.h"

#ifdef ENABLE_DTAP
#include "dtap_api.h"
#endif

#include <SDL2/SDL.h>

#define TAG "AOUT-SDL2"

#define SDL_AUDIO_BUFFER_SIZE 1024

typedef struct{
    SDL_AudioSpec wanted;     // config audio
    dt_buffer_t dbt;
}sdl_ao_ctx_t;

ao_wrapper_t ao_sdl2_ops;
static ao_wrapper_t *wrapper = &ao_sdl2_ops;

#ifdef ENABLE_DTAP
    dtap_context_t ap_ctx;
    int cur_ae_id = EQ_EFFECT_FLAT;
int dtap_change_effect()
{
    cur_ae_id = (cur_ae_id + 1) % 9;
    ap_ctx.para.item = cur_ae_id;
    dtap_update(&ap_ctx);
    return 0;
}
#endif

static void sdl2_cb(void *userdata,uint8_t *buf,int size)
{
    ao_wrapper_t *wrapper = (ao_wrapper_t *)userdata;
    sdl_ao_ctx_t *ctx = (sdl_ao_ctx_t *)wrapper->ao_priv;
    if(buf_level(&ctx->dbt) < size)
        return;
    buf_get(&ctx->dbt,buf,size);
    return;
}

static int ao_sdl2_init (dtaudio_output_t *aout, dtaudio_para_t *para)
{
    int ret = 0;
    memcpy(&wrapper->para,para,sizeof(dtaudio_para_t));
    sdl_ao_ctx_t *ctx = malloc(sizeof(*ctx));
    if(!ctx)
    {
        dt_info(TAG,"SDL CTX MALLOC FAILED \n");
        return -1;
    }
    memset(ctx,0,sizeof(*ctx));
    if(buf_init(&ctx->dbt,para->dst_samplerate * 4 / 10) < 0) // 100ms
    {
        ret = -1;
        goto FAIL;
    }
    wrapper->ao_priv = ctx;

    if (!SDL_WasInit(SDL_INIT_AUDIO))
        SDL_Init(SDL_INIT_AUDIO);

    SDL_AudioSpec *pwanted = &ctx->wanted;
    //set audio paras
    pwanted->freq = para->dst_samplerate;       // sample rate
    pwanted->format = AUDIO_S16;             // bps
    pwanted->channels = para->dst_channels;     // channels
    pwanted->samples = SDL_AUDIO_BUFFER_SIZE;// samples every time
    pwanted->callback = sdl2_cb;              // callback
    pwanted->userdata = wrapper;
   
    dt_info(TAG,"sr:%d channels:%d \n",para->dst_samplerate,para->dst_channels);

    if (SDL_OpenAudio(pwanted, NULL)<0)      // open audio device
    {
        printf("can't open audio.\n");
        ret = -1;
        goto FAIL;
    }
    SDL_PauseAudio(0);

#ifdef ENABLE_DTAP
    memset(&ap_ctx, 0 , sizeof(dtap_context_t));
    ap_ctx.para.samplerate = para->samplerate;
    ap_ctx.para.channels = para->channels;
    ap_ctx.para.data_width = para->data_width;
    ap_ctx.para.type = DTAP_EFFECT_EQ;
    ap_ctx.para.item = cur_ae_id;
    dtap_init(&ap_ctx);
#endif
    
    dt_info(TAG,"SDL2 AO Init OK\n");
    return 0;
FAIL:
    buf_release(&ctx->dbt);
    free(ctx);
    wrapper->ao_priv = NULL;
    return ret;
}

static int ao_sdl2_play (dtaudio_output_t *aout, uint8_t * buf, int size)
{
    sdl_ao_ctx_t *ctx = (sdl_ao_ctx_t *)wrapper->ao_priv;

    //write ok or failed
    if(buf_space(&ctx->dbt) < size)
        return 0;

#ifdef ENABLE_DTAP
    dtap_frame_t frame;
    frame.in = buf;
    frame.in_size = size;
    if(ap_ctx.para.item != EQ_EFFECT_FLAT)
        dtap_process(&ap_ctx, &frame);
#endif

    return buf_put(&ctx->dbt,buf,size);
}

static int ao_sdl2_pause (dtaudio_output_t *aout)
{
    SDL_PauseAudio(1);
    return 0;
}

static int ao_sdl2_resume (dtaudio_output_t *aout)
{
    SDL_PauseAudio(0);
    return 0;
}

static int ao_sdl2_level(dtaudio_output_t *aout)
{
    sdl_ao_ctx_t *ctx = (sdl_ao_ctx_t *)wrapper->ao_priv;
    return ctx->dbt.level;
}

static int64_t ao_sdl2_get_latency (dtaudio_output_t *aout)
{
    sdl_ao_ctx_t *ctx = (sdl_ao_ctx_t *)wrapper->ao_priv;
    
    int level = buf_level(&ctx->dbt);
    unsigned int sample_num;
    uint64_t latency;
    float pts_ratio = 0.0;
    pts_ratio = (double) 90000 / wrapper->para.dst_samplerate;
    sample_num = level / (wrapper->para.dst_channels * wrapper->para.bps / 8);
    latency = (sample_num * pts_ratio);
    return latency;
}

static int ao_sdl2_stop (dtaudio_output_t *aout)
{
    if(wrapper->ao_priv)
    {
        sdl_ao_ctx_t *ctx = (sdl_ao_ctx_t *)wrapper->ao_priv;
        SDL_CloseAudio();
        buf_release(&ctx->dbt);
        free(ctx);
        wrapper->ao_priv = NULL;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        wrapper->ao_priv = NULL;
    }

#ifdef ENABLE_DTAP
    memset(&ap_ctx, 0 , sizeof(dtap_context_t));
    dtap_release(&ap_ctx);    
#endif


    return 0;
}
    
ao_wrapper_t ao_sdl2_ops = {
    .id = AO_ID_SDL,
    .name = "sdl2",
    .ao_init = ao_sdl2_init,
    .ao_pause = ao_sdl2_pause,
    .ao_resume = ao_sdl2_resume,
    .ao_stop = ao_sdl2_stop,
    .ao_write = ao_sdl2_play,
    .ao_level = ao_sdl2_level,
    .ao_latency = ao_sdl2_get_latency,
};
