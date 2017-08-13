#ifdef ENABLE_VO_SDL2

#include "dt_utils.h"

#include "dtp_av.h"
#include "dtp_plugin.h"

#ifdef ENABLE_DTAP
#include "dtap_api.h"
#endif

#include <SDL2/SDL.h>

#define TAG "AOUT-SDL2"

#define SDL_AUDIO_BUFFER_SIZE 1024

const char *ao_sdl2_name = "SDL2 AO";

typedef struct {
    SDL_AudioSpec wanted;     // config audio
    dt_buffer_t dbt;
} sdl2_ao_ctx_t;

ao_wrapper_t ao_sdl2_ops;
static ao_wrapper_t *wrapper = &ao_sdl2_ops;

#ifdef ENABLE_DTAP
dtap_context_t ap_ctx;
int cur_ae_id = EQ_EFFECT_FLAT;
int dtap_change_effect(ao_wrapper_t *wrapper)
{
    cur_ae_id = (cur_ae_id + 1) % 9;
    ap_ctx.para.item = cur_ae_id;
    dtap_update(&ap_ctx);
    dtap_init(&ap_ctx);
    return 0;
}
#endif

static void sdl2_cb(void *userdata, uint8_t *buf, int size)
{
    ao_context_t *aoc = (ao_context_t *)userdata;
    sdl2_ao_ctx_t *ctx = (sdl2_ao_ctx_t *)aoc->private_data;
    if (buf_level(&ctx->dbt) < size) {
        return;
    }
    buf_get(&ctx->dbt, buf, size);
    return;
}

static int ao_sdl2_init(ao_context_t *aoc)
{
    int ret = 0;
    dtaudio_para_t *para = &aoc->para;
    sdl2_ao_ctx_t *ctx = (sdl2_ao_ctx_t *)aoc->private_data;
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
    //set audio paras
    pwanted->freq = para->dst_samplerate;       // sample rate
    pwanted->format = AUDIO_S16;             // bps
    pwanted->channels = para->dst_channels;     // channels
    pwanted->samples = SDL_AUDIO_BUFFER_SIZE;// samples every time
    pwanted->callback = sdl2_cb;              // callback
    pwanted->userdata = aoc;

    dt_info(TAG, "sr:%d channels:%d \n", para->dst_samplerate, para->dst_channels);

    if (SDL_OpenAudio(pwanted, NULL) < 0) {  // open audio device
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

    dt_info(TAG, "SDL2 AO Init OK\n");
    return 0;
FAIL:
    buf_release(&ctx->dbt);
    free(ctx);
    return ret;
}

static int ao_sdl2_play(ao_context_t *aoc, uint8_t * buf, int size)
{
    sdl2_ao_ctx_t *ctx = (sdl2_ao_ctx_t *)aoc->private_data;

    //write ok or failed
    if (buf_space(&ctx->dbt) < size) {
        return 0;
    }

#ifdef ENABLE_DTAP
    dtap_frame_t frame;
    frame.in = buf;
    frame.in_size = size;
    if (ap_ctx.para.item != EQ_EFFECT_FLAT) {
        dtap_process(&ap_ctx, &frame);
    }
#endif

    return buf_put(&ctx->dbt, buf, size);
}

static int ao_sdl2_pause(ao_context_t *aoc)
{
    SDL_PauseAudio(1);
    return 0;
}

static int ao_sdl2_resume(ao_context_t *aoc)
{
    SDL_PauseAudio(0);
    return 0;
}

static int ao_sdl2_level(ao_context_t *aoc)
{
    sdl2_ao_ctx_t *ctx = (sdl2_ao_ctx_t *)aoc->private_data;
    return ctx->dbt.level;
}

static int64_t ao_sdl2_get_latency(ao_context_t *aoc)
{
    sdl2_ao_ctx_t *ctx = (sdl2_ao_ctx_t *)aoc->private_data;

    int level = buf_level(&ctx->dbt);
    unsigned int sample_num;
    uint64_t latency;
    float pts_ratio = 0.0;
    pts_ratio = (double) 90000 / aoc->para.dst_samplerate;
    sample_num = level / (aoc->para.dst_channels * aoc->para.bps / 8);
    latency = (sample_num * pts_ratio);
    return latency;
}

static int ao_sdl2_get_volume(ao_context_t *aoc)
{
    dt_info(TAG, "getvolume: \n");
    return 0;
}

static int ao_sdl2_set_volume(ao_context_t *aoc, int value)
{
    dt_info(TAG, "setvolume: %d \n", value);
    return 0;
}

static int ao_sdl2_set_parameter(ao_context_t *aoc, int cmd, unsigned long arg)
{
    switch (cmd) {
    case DTP_AO_CMD_SET_VOLUME:
        ao_sdl2_set_volume(aoc, (int)arg);
        break;
    }
    return 0;
}

static int ao_sdl2_get_parameter(ao_context_t *aoc, int cmd, unsigned long arg)
{
    switch (cmd) {
    case DTP_AO_CMD_GET_LATENCY:
        *(int *)(arg) = ao_sdl2_get_latency(aoc);
        break;
    case DTP_AO_CMD_GET_LEVEL:
        *(int *)(arg) = ao_sdl2_level(aoc);
        break;
    case DTP_AO_CMD_GET_VOLUME:
        *(int *)(arg) = ao_sdl2_get_volume(aoc);
        break;
    }
    return 0;
}

static int ao_sdl2_stop(ao_context_t *aoc)
{
    if (aoc->private_data) {
        sdl2_ao_ctx_t *ctx = (sdl2_ao_ctx_t *)aoc->private_data;
        SDL_CloseAudio();
        buf_release(&ctx->dbt);
        free(ctx);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }

#ifdef ENABLE_DTAP
    memset(&ap_ctx, 0 , sizeof(dtap_context_t));
    dtap_release(&ap_ctx);
#endif

    return 0;
}

ao_wrapper_t ao_sdl2_ops = {
    .id = AO_ID_SDL2,
    .name = "sdl2",
    .init = ao_sdl2_init,
    .pause = ao_sdl2_pause,
    .resume = ao_sdl2_resume,
    .write = ao_sdl2_play,
    .get_parameter = ao_sdl2_get_parameter,
    .set_parameter = ao_sdl2_set_parameter,
    .stop = ao_sdl2_stop,
    .private_data_size = sizeof(sdl2_ao_ctx_t),
};

#endif
