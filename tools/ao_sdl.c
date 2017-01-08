#include "dt_utils.h"

#include "dtp_av.h"
#include "dtp_plugin.h"

#include <SDL/SDL.h>

#define TAG "AOUT-SDL"

#define SDL_AUDIO_BUFFER_SIZE 1024

const char *ao_sdl_name = "SDL AO";

typedef struct {
    SDL_AudioSpec wanted;     // config audio
    SDL_AudioSpec spec;     // config audio
    dt_buffer_t dbt;
} sdl_ao_ctx_t;

static void sdl_cb(void *userdata, uint8_t *buf, int size)
{
    ao_wrapper_t *ao = (ao_wrapper_t *)userdata;
    sdl_ao_ctx_t *ctx = (sdl_ao_ctx_t *)ao->ao_priv;
    if (buf_level(&ctx->dbt) < size) {
        return;
    }
    buf_get(&ctx->dbt, buf, size);
    return;
}

static int ao_sdl_init(ao_wrapper_t *ao, dtaudio_para_t *ppara)
{
    int ret = 0;
    sdl_ao_ctx_t *ctx = malloc(sizeof(*ctx));
    if (!ctx) {
        dt_info(TAG, "SDL CTX MALLOC FAILED \n");
        return -1;
    }
    memset(ctx, 0, sizeof(*ctx));
    if (buf_init(&ctx->dbt, ppara->dst_samplerate * 4 / 10) < 0) { // 100ms
        ret = -1;
        goto FAIL;
    }
    ao->ao_priv = ctx;

    if (!SDL_WasInit(SDL_INIT_AUDIO)) {
        SDL_Init(SDL_INIT_AUDIO);
    }

    SDL_AudioSpec *pwanted = &ctx->wanted;
    SDL_AudioSpec *spec = &ctx->spec;
    //set audio paras
    pwanted->freq = ppara->dst_samplerate;       // sample rate
    pwanted->format = AUDIO_S16SYS;             // bps
    pwanted->silence = 0;
    pwanted->channels = ppara->dst_channels;     // channels
    pwanted->samples = SDL_AUDIO_BUFFER_SIZE;// samples every time
    pwanted->callback = sdl_cb;              // callback
    pwanted->userdata = ao;

    if (SDL_OpenAudio(pwanted, spec) < 0) {  // open audio device
        printf("can't open audio.\n");
        ret = -1;
        goto FAIL;
    }

    if (spec->format != AUDIO_S16SYS) {
        dt_error(TAG,  "SDL advised audio format %d is not supported!\n", spec->format);
        return -1;
    }
    if (spec->channels != pwanted->channels) {
        dt_error(TAG, "SDL advised channel count %d is not supported!\n",
                 spec->channels);
        return -1;
    }

    SDL_PauseAudio(0);
    dt_info(TAG, "SDL AO Init OK\n");
    return 0;
FAIL:
    buf_release(&ctx->dbt);
    free(ctx);
    ao->ao_priv = NULL;
    return ret;
}

static int ao_sdl_play(ao_wrapper_t *ao, uint8_t * buf, int size)
{
    sdl_ao_ctx_t *ctx = (sdl_ao_ctx_t *)ao->ao_priv;
    return buf_put(&ctx->dbt, buf, size);
}

static int ao_sdl_pause(ao_wrapper_t *ao)
{
    SDL_PauseAudio(1);
    return 0;
}

static int ao_sdl_resume(ao_wrapper_t *ao)
{
    SDL_PauseAudio(0);
    return 0;
}

static int ao_sdl_level(ao_wrapper_t *ao)
{
    sdl_ao_ctx_t *ctx = (sdl_ao_ctx_t *)ao->ao_priv;
    return ctx->dbt.level;
}

static int64_t ao_sdl_get_latency(ao_wrapper_t *ao)
{
    sdl_ao_ctx_t *ctx = (sdl_ao_ctx_t *)ao->ao_priv;
    int level = buf_level(&ctx->dbt) + ctx->spec.size * 2;
    unsigned int sample_num;
    uint64_t latency;
    float pts_ratio = 0.0;
    pts_ratio = (double) 90000 / ao->para.dst_samplerate;
    sample_num = level / (ao->para.dst_channels * ao->para.bps / 8);
    latency = (sample_num * pts_ratio);
    return latency;
}

static int ao_sdl_get_volume(ao_wrapper_t *ao)
{
    dt_info(TAG, "getvolume: \n");
    return 0;
}

static int ao_sdl_set_volume(ao_wrapper_t *ao, int value)
{
    dt_info(TAG, "setvolume: %d \n", value);
    return 0;
}

static int ao_sdl_set_parameter(ao_wrapper_t *ao, int cmd, unsigned long arg)
{
    switch (cmd) {
    case DTP_AO_CMD_SET_VOLUME:
        ao_sdl_set_volume(ao, (int)arg);
        break;
    }
    return 0;
}

static int ao_sdl_get_parameter(ao_wrapper_t *ao, int cmd, unsigned long arg)
{
    switch (cmd) {
    case DTP_AO_CMD_GET_LATENCY:
        *(int *)(arg) = ao_sdl_get_latency(ao);
        break;
    case DTP_AO_CMD_GET_LEVEL:
        *(int *)(arg) = ao_sdl_level(ao);
        break;
    case DTP_AO_CMD_GET_VOLUME:
        *(int *)(arg) = ao_sdl_get_volume(ao);
        break;
    }
    return 0;
}

static int ao_sdl_stop(ao_wrapper_t *ao)
{
    if (ao->ao_priv) {
        sdl_ao_ctx_t *ctx = (sdl_ao_ctx_t *)ao->ao_priv;
        SDL_CloseAudio();
        buf_release(&ctx->dbt);
        free(ctx);
        ao->ao_priv = NULL;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    return 0;
}

int setup_ao(ao_wrapper_t *wrapper)
{
    if (wrapper == NULL) {
        return -1;
    }
    wrapper->id = AO_ID_SDL;
    wrapper->name = ao_sdl_name;
    wrapper->init = ao_sdl_init;
    wrapper->pause = ao_sdl_pause;
    wrapper->resume = ao_sdl_resume;
    wrapper->write = ao_sdl_play;
    wrapper->get_parameter = ao_sdl_get_parameter;
    wrapper->set_parameter = ao_sdl_set_parameter;
    wrapper->stop = ao_sdl_stop;
    return 0;
}

ao_wrapper_t ao_sdl_ops = {
    .id = AO_ID_SDL,
    .name = "sdl",
    .init = ao_sdl_init,
    .pause = ao_sdl_pause,
    .resume = ao_sdl_resume,
    .write = ao_sdl_play,
    .get_parameter = ao_sdl_get_parameter,
    .set_parameter = ao_sdl_set_parameter,
    .stop = ao_sdl_stop,
};
