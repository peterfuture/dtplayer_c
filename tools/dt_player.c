#include "dt_player.h"
#include "version.h"
#include "stream_wrapper.h"
#include "demuxer_wrapper.h"
#include "ao_wrapper.h"
#include "ad_wrapper.h"
#include "vo_wrapper.h"
#include "vd_wrapper.h"
#include "ui.h"

#include "dtplayer_api.h"
#include "dtplayer.h"
#include "commander.h"

#include <stdio.h>
#include <string.h>

#define TAG "DT-PLAYER"

static ply_ctx_t ply_ctx;

#ifdef ENABLE_DTAP
int dtap_change_effect(ao_wrapper_t *wrapper);
#endif

#ifdef ENABLE_AO_SDL2
void ao_sdl2_setup(ao_wrapper_t *wrapper);
#endif
#ifdef ENABLE_AO_SDL
void ao_sdl_setup(ao_wrapper_t *wrapper);
void vo_sdl_setup(vo_wrapper_t *wrapper);
#endif

static int update_cb(void *cookie, player_state_t * state)
{
    if (state->cur_status == PLAYER_STATUS_EXIT) {
        dt_info(TAG, "RECEIVE EXIT CMD\n");
        ply_ctx.exit_flag = 1;
    }
    ply_ctx.cur_time = state->cur_time;
    ply_ctx.cur_time_ms = state->cur_time_ms;
    ply_ctx.duration = state->full_time;

    dt_debug(TAG, "UPDATECB CURSTATUS:%x \n", state->cur_status);
    dt_info(TAG, "CUR TIME %lld S  FULL TIME:%lld  \n", state->cur_time, state->full_time);
    return 0;
}

extern vd_wrapper_t vd_ex_ops;
static void register_ex_all()
{
    //player_register_ex_stream();
    //player_register_ex_demuxer();
    //player_register_ex_ao();
    //player_register_ex_ad();
    //player_register_ex_vo();
    //dtplayer_register_ext_vd(&vd_ex_ops);
}

static void on_loop(command_t *self)
{
    dtplayer_para_t *para = (dtplayer_para_t *)self->data;
    para->loop_mode = atoi(self->arg);
}

static void on_set_width(command_t *self)
{
    dtplayer_para_t *para = (dtplayer_para_t *)self->data;
    para->width = atoi(self->arg);
}

static void on_set_height(command_t *self)
{
    dtplayer_para_t *para = (dtplayer_para_t *)self->data;
    para->height = atoi(self->arg);
}

static void on_disable_audio(command_t *self)
{
    dtplayer_para_t *para = (dtplayer_para_t *)self->data;
    para->disable_audio = 1;
}

static void on_disable_video(command_t *self)
{
    dtplayer_para_t *para = (dtplayer_para_t *)self->data;
    para->disable_video = 1;
}

static void on_disable_sub(command_t *self)
{
    dtplayer_para_t *para = (dtplayer_para_t *)self->data;
    para->disable_sub = 1;
}

static void on_select_audio(command_t *self)
{
    dtplayer_para_t *para = (dtplayer_para_t *)self->data;
    para->audio_index = atoi(self->arg);
}

static void on_select_video(command_t *self)
{
    dtplayer_para_t *para = (dtplayer_para_t *)self->data;
    para->video_index = atoi(self->arg);
}

static void on_select_sub(command_t *self)
{
    dtplayer_para_t *para = (dtplayer_para_t *)self->data;
    para->sub_index = atoi(self->arg);
}

static void on_disable_sync(command_t *self)
{
    dtplayer_para_t *para = (dtplayer_para_t *)self->data;
    para->disable_avsync = 1;
}

#define VERSION "0.1"
int main(int argc, char **argv)
{
    int ret = 0;
    dtplayer_para_t para;
    memset(&para, 0, sizeof(dtplayer_para_t));
    command_t program;
    command_init(&program, "dtplayer", VERSION);
    program.data = &para;
    program.usage = "[options] <url>";
    command_option(&program, "-dw", "--width <n>", "specify destiny width", on_set_width);
    command_option(&program, "-dh", "--height <n>", "specify destiny height", on_set_height);
    command_option(&program, "-na", "--disable_audio", "disable audio", on_disable_audio);
    command_option(&program, "-nv", "--disable_video", "disable video", on_disable_video);
    command_option(&program, "-ns", "--disable_sub", "disable sub", on_disable_sub);
    command_option(&program, "-ast", "--audio_index <n>", "specify audio index", on_select_audio);
    command_option(&program, "-vst", "--video_index <n>", "specify video index", on_select_video);
    command_option(&program, "-sst", "--sub_index <n>", "specify sub index", on_select_sub);
    command_option(&program, "-l", "--loop <n>", "enable loop", on_loop);
    command_option(&program, "-nsy", "--disable-sync", "disable avsync", on_disable_sync);

    command_parse(&program, argc, argv);

    //version_info();
    if (argc < 2) {
        command_help(&program);
        return 0;
    }

    memset(&ply_ctx, 0, sizeof(ply_ctx_t));
    para.update_cb = update_cb;
    para.file_name = argv[argc - 1];
    ply_ctx.file_name = para.file_name;

#ifdef ENABLE_AO_SDL2
    ao_sdl2_setup(&ply_ctx.ao);
    dtplayer_register_ext_ao(&ply_ctx.ao);
#endif

#ifdef ENABLE_AO_SDL
    ao_sdl_setup(&ply_ctx.ao);
    dtplayer_register_ext_ao(&ply_ctx.ao);
    vo_sdl_setup(&ply_ctx.vo);
    dtplayer_register_ext_vo(&ply_ctx.vo);
#endif

    player_register_all();
    register_ex_all();

    void *player_priv = NULL;
    dt_media_info_t info;
    player_priv = dtplayer_init(&para);
    if (!player_priv) {
        return -1;
    }
    ret = dtplayer_get_mediainfo(player_priv, &info);
    if (ret < 0) {
        dt_info(TAG, "get mediainfo failed, quit \n");
        return -1;
    }
    dt_info(TAG, "get mediainfo ok, filesize:%lld fulltime:%lld S \n", info.file_size, info.duration);

    //set default window size
    int width = 720;
    int height = 480;
    vstream_info_t *vstream = info.vstreams[0];
    if (info.has_video) {
        width = (para.width) ? para.width : vstream->width;
        height = (para.height) ? para.height : vstream->height;
    }
    if (width <= 0 || width > 4096) {
        width = 720;
    }
    if (height <= 0 || height > 2160) {
        height = 480;
    }

    if (info.has_video) {
        dt_info(TAG, "src-width: %d src-height: %d \n", vstream->width, vstream->height);
    }
    dt_info(TAG, "dst-width: %d dst-height: %d \n", width, height);

    ply_ctx.disp_width = width;
    ply_ctx.disp_height = height;

    ply_ctx.ui_ctx = (ui_ctx_t *)dt_malloc(sizeof(ui_ctx_t));
    ui_init(&ply_ctx, ply_ctx.ui_ctx);

    dtplayer_start(player_priv);

    //event handle
    player_event_t event = EVENT_NONE;
    args_t arg;
    while (1) {
        if (ply_ctx.exit_flag == 1) {
            break;
        }
        event = get_event(&arg, &ply_ctx);
        if (event == EVENT_NONE) {
            usleep(100);
            continue;
        }

        switch (event) {
        case EVENT_PAUSE:
            dtplayer_pause(player_priv);
            break;
        case EVENT_RESUME:
            dtplayer_resume(player_priv);
            break;
        case EVENT_SEEK:
            dtplayer_seekto(player_priv, arg.arg1);
            break;
        case EVENT_STOP:
            dtplayer_stop(player_priv);
            goto QUIT_CHECK;
            break;
        case EVENT_RESIZE:
            dt_info(TAG, "resize, w:%d h:%d \n", arg.arg1, arg.arg2);
            ui_window_resize(arg.arg1, arg.arg2);
            ply_ctx.disp_width = arg.arg1;
            ply_ctx.disp_height = arg.arg2;
            //dtplayer_set_video_size(player_priv, arg.arg1, arg.arg2);
            break;
        case EVENT_VOLUME_ADD:
            dt_info(TAG, " volume add \n");
            ply_ctx.volume++;
            ply_ctx.volume = ply_ctx.volume % 10;
            ply_ctx.ao.ao_set_volume(&ply_ctx.ao, ply_ctx.volume);
            break;
#ifdef ENABLE_DTAP
        case EVENT_AE:
            dtap_change_effect(&ply_ctx.ao);
            break;
#endif
        default:
            dt_info(TAG, "UNKOWN CMD, IGNORE \n");
            break;
        }

        usleep(10 * 1000);  // sleep 10ms
    }
QUIT_CHECK:
    while (!ply_ctx.exit_flag) {
        usleep(100);
        break;
    }
    ui_stop();
    command_free(&program);
    dt_info("", "QUIT DTPLAYER-TEST\n");
    return 0;
}
