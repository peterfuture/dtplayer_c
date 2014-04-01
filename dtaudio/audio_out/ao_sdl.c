#include "dtaudio_output.h"
#include "dt_buffer.h"

#include <SDL/SDL.h>

#define TAG "AOUT-SDL"

#define SDL_AUDIO_BUFFER_SIZE 1024

typedef struct{
    SDL_AudioSpec wanted;     // config audio
    dt_buffer_t dbt;
}sdl_ao_ctx_t;

static void sdl_cb(void *userdata,uint8_t *buf,int size)
{
    ao_wrapper_t *wrapper = (ao_wrapper_t *)userdata;
    sdl_ao_ctx_t *ctx = (sdl_ao_ctx_t *)wrapper->ao_priv;
    if(buf_level(&ctx->dbt) < size)
        return;
    buf_get(&ctx->dbt,buf,size);
    return;
}

static int ao_sdl_init (ao_wrapper_t *wrapper, void *parent)
{
    int ret = 0;
    dtaudio_output_t *ao = (dtaudio_output_t *)parent;
    dtaudio_para_t *ppara = &ao->para;
    wrapper->parent = parent;
    sdl_ao_ctx_t *ctx = malloc(sizeof(*ctx));
    if(!ctx)
    {
        dt_info(TAG,"SDL CTX MALLOC FAILED \n");
        return -1;
    }
    memset(ctx,0,sizeof(*ctx));
    if(buf_init(&ctx->dbt,ppara->dst_samplerate * 4 / 10) < 0) // 100ms
    {
        ret = -1;
        goto FAIL;
    }
    wrapper->ao_priv = ctx;

    if (!SDL_WasInit(SDL_INIT_AUDIO))
        SDL_Init(SDL_INIT_AUDIO);

    SDL_AudioSpec *pwanted = &ctx->wanted;
    //set audio paras
    pwanted->freq = ppara->dst_samplerate;       // sample rate
    pwanted->format = AUDIO_S16;             // bps
    pwanted->channels = ppara->dst_channels;     // channels
    pwanted->samples = SDL_AUDIO_BUFFER_SIZE;// samples every time
    pwanted->callback = sdl_cb;              // callback
    pwanted->userdata = wrapper;
    
    if (SDL_OpenAudio(pwanted, NULL)<0)      // open audio device
    {
        printf("can't open audio.\n");
        ret = -1;
        goto FAIL;
    }
    SDL_PauseAudio(0);
    dt_info(TAG,"SDL AO Init OK\n"); 
    return 0;
FAIL:
    buf_release(&ctx->dbt);
    free(ctx);
    wrapper->ao_priv = NULL;
    return ret;
}

static int ao_sdl_play (ao_wrapper_t *wrapper, uint8_t * buf, int size)
{
    sdl_ao_ctx_t *ctx = (sdl_ao_ctx_t *)wrapper->ao_priv;
    return buf_put(&ctx->dbt,buf,size);
}

static int ao_sdl_pause (ao_wrapper_t *wrapper)
{
    SDL_PauseAudio(1);
    return 0;
}

static int ao_sdl_resume (ao_wrapper_t *wrapper)
{
    SDL_PauseAudio(0);
    return 0;
}

static int ao_sdl_level(ao_wrapper_t *wrapper)
{
    sdl_ao_ctx_t *ctx = (sdl_ao_ctx_t *)wrapper->ao_priv;
    return ctx->dbt.level;
}

static int64_t ao_sdl_get_latency (ao_wrapper_t *wrapper)
{
    sdl_ao_ctx_t *ctx = (sdl_ao_ctx_t *)wrapper->ao_priv;
    dtaudio_output_t *ao = (dtaudio_output_t *)wrapper->parent;
    
    int level = buf_level(&ctx->dbt);
    unsigned int sample_num;
    uint64_t latency;
    float pts_ratio = 0.0;
    pts_ratio = (double) 90000 / ao->para.dst_samplerate;
    sample_num = level / (ao->para.dst_channels * ao->para.bps / 8);
    latency = (sample_num * pts_ratio);
    return latency;
}

static int ao_sdl_stop (ao_wrapper_t * wrapper)
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
    return 0;
}
    
ao_wrapper_t ao_sdl_ops = {
    .id = AO_ID_SDL,
    .name = "sdl",
    .ao_init = ao_sdl_init,
    .ao_pause = ao_sdl_pause,
    .ao_resume = ao_sdl_resume,
    .ao_stop = ao_sdl_stop,
    .ao_write = ao_sdl_play,
    .ao_level = ao_sdl_level,
    .ao_latency = ao_sdl_get_latency,
};
