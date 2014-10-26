#include "dtaudio_output.h"
#include "dt_buffer.h"

#include <SDL/SDL.h>

#define TAG "AOUT-SDL"

#define SDL_AUDIO_BUFFER_SIZE 1024

const char *ao_sdl_name = "SDL AO";

typedef struct{
    SDL_AudioSpec wanted;     // config audio
    dt_buffer_t dbt;
}sdl_ao_ctx_t;

static void sdl_cb(void *userdata,uint8_t *buf,int size)
{
    dtaudio_output_t *ao = (dtaudio_output_t *)userdata;
    sdl_ao_ctx_t *ctx = (sdl_ao_ctx_t *)ao->ao_priv;
    if(buf_level(&ctx->dbt) < size)
        return;
    buf_get(&ctx->dbt,buf,size);
    return;
}

static int ao_sdl_init (dtaudio_output_t *aout, dtaudio_para_t *ppara)
{
    int ret = 0;
    ao_wrapper_t *wrapper = aout->wrapper;
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
    aout->ao_priv = ctx;

    if (!SDL_WasInit(SDL_INIT_AUDIO))
        SDL_Init(SDL_INIT_AUDIO);

    SDL_AudioSpec *pwanted = &ctx->wanted;
    //set audio paras
    pwanted->freq = ppara->dst_samplerate;       // sample rate
    pwanted->format = AUDIO_S16;             // bps
    pwanted->channels = ppara->dst_channels;     // channels
    pwanted->samples = SDL_AUDIO_BUFFER_SIZE;// samples every time
    pwanted->callback = sdl_cb;              // callback
    pwanted->userdata = aout;
    
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
    aout->ao_priv = NULL;
    return ret;
}

static int ao_sdl_play (dtaudio_output_t *ao, uint8_t * buf, int size)
{
    sdl_ao_ctx_t *ctx = (sdl_ao_ctx_t *)ao->ao_priv;
    return buf_put(&ctx->dbt,buf,size);
}

static int ao_sdl_pause (dtaudio_output_t *ao)
{
    SDL_PauseAudio(1);
    return 0;
}

static int ao_sdl_resume (dtaudio_output_t *ao)
{
    SDL_PauseAudio(0);
    return 0;
}

static int ao_sdl_level(dtaudio_output_t *ao)
{
    sdl_ao_ctx_t *ctx = (sdl_ao_ctx_t *)ao->ao_priv;
    return ctx->dbt.level;
}

static int64_t ao_sdl_get_latency (dtaudio_output_t *ao)
{
    sdl_ao_ctx_t *ctx = (sdl_ao_ctx_t *)ao->ao_priv;
    int level = buf_level(&ctx->dbt);
    unsigned int sample_num;
    uint64_t latency;
    float pts_ratio = 0.0;
    pts_ratio = (double) 90000 / ao->para.dst_samplerate;
    sample_num = level / (ao->para.dst_channels * ao->para.bps / 8);
    latency = (sample_num * pts_ratio);
    return latency;
}

static int ao_sdl_stop (dtaudio_output_t *ao)
{
    if(ao->ao_priv)
    {
        sdl_ao_ctx_t *ctx = (sdl_ao_ctx_t *)ao->ao_priv;
        SDL_CloseAudio();
        buf_release(&ctx->dbt);
        free(ctx);
        ao->ao_priv = NULL;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    return 0;
}

static int ao_sdl_get_volume(ao_wrapper_t *ao)
{
    dt_info(TAG,"getvolume: \n");
    return 0;
}

static int ao_sdl_set_volume(ao_wrapper_t *ao, int value)
{
    dt_info(TAG,"setvolume: %d \n", value);
    return 0;
}

void ao_sdl_setup(ao_wrapper_t *wrapper)
{
    if(wrapper == NULL) return;
    wrapper->id = AO_ID_SDL;
    wrapper->name = ao_sdl_name;
    wrapper->ao_init = ao_sdl_init;
    wrapper->ao_pause = ao_sdl_pause;
    wrapper->ao_resume = ao_sdl_resume;
    wrapper->ao_stop = ao_sdl_stop;
    wrapper->ao_write = ao_sdl_play;
    wrapper->ao_level = ao_sdl_level;
    wrapper->ao_latency = ao_sdl_get_latency;
    wrapper->ao_get_volume = ao_sdl_get_volume;
    wrapper->ao_set_volume = ao_sdl_set_volume;
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
    .ao_get_volume = ao_sdl_get_volume,
    .ao_set_volume = ao_sdl_set_volume,
};
