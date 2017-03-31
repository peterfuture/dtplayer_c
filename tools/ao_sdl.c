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
    ao_context_t *aoc = (ao_context_t *)userdata;
    sdl_ao_ctx_t *ctx = (sdl_ao_ctx_t *)aoc->private_data;
    if (buf_level(&ctx->dbt) < size) {
        return;
    }
    buf_get(&ctx->dbt, buf, size);
    return;
}

static int ao_sdl_init(ao_context_t *aoc)
{
    int ret = 0;
    dtaudio_para_t *para = &aoc->para;
    sdl_ao_ctx_t *ctx = (sdl_ao_ctx_t *)aoc->private_data;
    if (!ctx) {
        dt_info(TAG, "SDL CTX MALLOC FAILED \n");
        return -1;
    }
    memset(ctx, 0, sizeof(*ctx));
    if (buf_init(&ctx->dbt, para->dst_samplerate * 4 / 10) < 0) { // 100ms
        ret = -1;
        goto FAIL;
    }

    if (!SDL_WasInit(SDL_INIT_AUDIO)) {
        SDL_Init(SDL_INIT_AUDIO);
    }

    SDL_AudioSpec *pwanted = &ctx->wanted;
    SDL_AudioSpec *spec = &ctx->spec;
    //set audio paras
    pwanted->freq = para->dst_samplerate;       // sample rate
    pwanted->format = AUDIO_S16SYS;             // bps
    pwanted->silence = 0;
    pwanted->channels = para->dst_channels;     // channels
    pwanted->samples = SDL_AUDIO_BUFFER_SIZE;// samples every time
    pwanted->callback = sdl_cb;              // callback
    pwanted->userdata = aoc;

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
    return ret;
}

static int ao_sdl_play(ao_context_t *aoc, uint8_t * buf, int size)
{
    sdl_ao_ctx_t *ctx = (sdl_ao_ctx_t *)aoc->private_data;
    return buf_put(&ctx->dbt, buf, size);
}

static int ao_sdl_pause(ao_context_t *aoc)
{
    SDL_PauseAudio(1);
    return 0;
}

static int ao_sdl_resume(ao_context_t *aoc)
{
    SDL_PauseAudio(0);
    return 0;
}

static int ao_sdl_level(ao_context_t *aoc)
{
    sdl_ao_ctx_t *ctx = (sdl_ao_ctx_t *)aoc->private_data;
    return ctx->dbt.level;
}

static int64_t ao_sdl_get_latency(ao_context_t *aoc)
{
    sdl_ao_ctx_t *ctx = (sdl_ao_ctx_t *)aoc->private_data;
    int level = buf_level(&ctx->dbt) + ctx->spec.size * 2;
    unsigned int sample_num;
    uint64_t latency;
    float pts_ratio = 0.0;
    pts_ratio = (double) 90000 / aoc->para.dst_samplerate;
    sample_num = level / (aoc->para.dst_channels * aoc->para.bps / 8);
    latency = (sample_num * pts_ratio);
    return latency;
}

static int ao_sdl_get_volume(ao_context_t *aoc)
{
    dt_info(TAG, "getvolume: \n");
    return 0;
}

static int ao_sdl_set_volume(ao_context_t *aoc, int value)
{
    dt_info(TAG, "setvolume: %d \n", value);
    return 0;
}

static int ao_sdl_set_parameter(ao_context_t *aoc, int cmd, unsigned long arg)
{
    switch (cmd) {
    case DTP_AO_CMD_SET_VOLUME:
        ao_sdl_set_volume(aoc, (int)arg);
        break;
    }
    return 0;
}

static int ao_sdl_get_parameter(ao_context_t *aoc, int cmd, unsigned long arg)
{
    switch (cmd) {
    case DTP_AO_CMD_GET_LATENCY:
        *(int *)(arg) = ao_sdl_get_latency(aoc);
        break;
    case DTP_AO_CMD_GET_LEVEL:
        *(int *)(arg) = ao_sdl_level(aoc);
        break;
    case DTP_AO_CMD_GET_VOLUME:
        *(int *)(arg) = ao_sdl_get_volume(aoc);
        break;
    }
    return 0;
}

static int ao_sdl_stop(ao_context_t *aoc)
{
    if (aoc->private_data) {
        sdl_ao_ctx_t *ctx = (sdl_ao_ctx_t *)aoc->private_data;
        SDL_CloseAudio();
        buf_release(&ctx->dbt);
        free(ctx);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
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
    .private_data_size = sizeof(sdl_ao_ctx_t)
};
