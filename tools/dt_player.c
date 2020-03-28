#include "dtp.h"
#include "version.h"

#include "stream_wrapper.h"
#include "demuxer_wrapper.h"
#include "dtp_plugin.h"

#include "commander.h"

#include <stdio.h>
#include <string.h>

#include "dt_player.h"

#define TAG "DT-PLAYER"

static player_t player;

#ifdef ENABLE_DTAP
int dtap_change_effect(ao_wrapper_t *wrapper);
#endif

static int update_cb(void *cookie, dtp_state_t * state)
{
    if (state->cur_status == PLAYER_STATUS_EXIT) {
        dt_info(TAG, "RECEIVE EXIT CMD\n");
        player.quit = 1;
    }
    if (state->cur_status == PLAYER_STATUS_PREPARE_START) {
        dt_info(TAG, "vdec type:%d.\n", state->vdec_type);
    }
    play_info_t *pinfo = &player.info;
    pinfo->cur_time = state->cur_time;
    pinfo->cur_time_ms = state->cur_time_ms;
    pinfo->duration = state->full_time;

    dt_debug(TAG, "UPDATECB CURSTATUS:%x \n", state->cur_status);
    dt_info(TAG, "CUR TIME %lld S  FULL TIME:%lld  \n", state->cur_time,
            state->full_time);
    return 0;
}

static int notify_cb(void *cookie, int cmd, unsigned long ext1, unsigned long ext2)
{
    switch (cmd) {
        case DTP_EVENTS_FIRST_AUDIO_DECODE:
            dt_info(TAG, "DTP_EVENTS_FIRST_AUDIO_DECODE \n");
            break;
        case DTP_EVENTS_FIRST_VIDEO_DECODE:
            dt_info(TAG, "DTP_EVENTS_FIRST_VIDEO_DECODE \n");
            break;
        case DTP_EVENTS_BUFFERING_START:
            dt_info(TAG, "DTP_EVENTS_BUFFERING_START \n");
            break;
        case DTP_EVENTS_BUFFERING_END:
            dt_info(TAG, "DTP_EVENTS_BUFFERING_END \n");
            break;
        default:
            dt_info(TAG, "Unkown cmd \n");
            break;
    }
    return 0;
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

static void on_select_vpf(command_t *self)
{
    dtplayer_para_t *para = (dtplayer_para_t *)self->data;
    para->video_pixel_format = atoi(self->arg);
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
    command_option(&program, "-dw",  "--width <n>",     "specify destiny width",
                   on_set_width);
    command_option(&program, "-dh",  "--height <n>",    "specify destiny height",
                   on_set_height);
    command_option(&program, "-na",  "--disable_audio", "disable audio",
                   on_disable_audio);
    command_option(&program, "-nv",  "--disable_video", "disable video",
                   on_disable_video);
    command_option(&program, "-ns",  "--disable_sub",   "disable sub",
                   on_disable_sub);
    command_option(&program, "-ast", "--audio_index",   "specify audio index",
                   on_select_audio);
    command_option(&program, "-vst", "--video_index",   "specify video index",
                   on_select_video);
    command_option(&program, "-sst", "--sub_index",     "specify sub index",
                   on_select_sub);
    command_option(&program, "-l",   "--loop <n>",      "enable loop",
                   on_loop);
    command_option(&program, "-nsy", "--disable-sync",  "disable avsync",
                   on_disable_sync);
    //command_option(&program, "-pf",  "--pixel_format",  "video pixfmt specify(0 yuv420p 1 rgb565 2 nv12)", on_select_vpf);

    command_parse(&program, argc, argv);

    //version_info();
    if (argc < 2) {
        command_help(&program);
        return 0;
    }

    // av options set - samplecode
    /*
        dtplayer_set_option(NULL, OPTION_CATEGORY_FFMPEG, "protocol_whitelist", "file,http,hls,udp,rtp,rtsp,tcp");
        dtplayer_set_option(NULL, OPTION_CATEGORY_FFMPEG, "loglevel", "100");
        dtplayer_set_option(NULL, OPTION_CATEGORY_FFMPEG, "timeout", "5000000");

        dtplayer_set_option(NULL, OPTION_CATEGORY_DTP, "player.live_timeout", "10000");
        dtplayer_set_option(NULL, OPTION_CATEGORY_DTP, "player.log_level", "2"); // 3-error 2-info 1-warnning 0-debug
        dtplayer_set_option(NULL, OPTION_CATEGORY_DTP, "port.audio_max_num", "8");
        dtplayer_set_option(NULL, OPTION_CATEGORY_DTP, "port.audio_max_size", "1000000"); // 10M
        dtplayer_set_option(NULL, OPTION_CATEGORY_DTP, "port.video_max_num", "8");
        dtplayer_set_option(NULL, OPTION_CATEGORY_DTP, "port.video_max_size", "10000000"); // 50M
    */
    memset(&player, 0, sizeof(player_t));
    para.update_cb = update_cb;
    para.notify_cb = notify_cb;
    para.file_name = argv[argc - 1];
    player.file_name = para.file_name;

    void *player_priv = NULL;
    dtp_media_info_t info;
    player_priv = dtplayer_init(&para);
    if (!player_priv) {
        return -1;
    }

    ret = dtplayer_get_mediainfo(player_priv, &info);
    if (ret < 0) {
        dt_info(TAG, "get mediainfo failed, quit \n");
        return -1;
    }
    dt_info(TAG, "get mediainfo ok, filesize:%lld fulltime:%lld S \n",
            info.file_size, info.duration);

    //set default window size
    int width = 720;
    int height = 480;
    vstream_info_t *vstream = info.tracks.vstreams[0];
    if (info.has_video) {
        width = (para.width > 0) ? para.width : vstream->width;
        height = (para.height > 0) ? para.height : vstream->height;
    }
    if (width <= 0 || width > 4096) {
        width = 720;
    }
    if (height <= 0 || height > 2160) {
        height = 480;
    }

    if (info.has_video) {
        dt_info(TAG, "src-width: %d src-height: %d \n", vstream->width,
                vstream->height);
    }
    dt_info(TAG, "dst-width: %d dst-height: %d \n", width, height);

    player.disp_width = width;
    player.disp_height = height;

    // setup gui
    gui_para_t gui_para;
    gui_para.width = width;
    gui_para.height = height;
    gui_para.pixfmt = 0; // yuv420 default.

    ret = setup_gui(&player.gui, &gui_para);
    if (ret < 0) {
        return -1;
    }

    if (info.has_video) {
#ifdef ENABLE_VO_SDL
        extern vo_wrapper_t vo_sdl_ops;
        dtplayer_register_plugin(DTP_PLUGIN_TYPE_VO, &vo_sdl_ops);
#endif
#ifdef ENABLE_VO_SDL2
        extern vo_wrapper_t vo_sdl2_ops;
        dtplayer_register_plugin(DTP_PLUGIN_TYPE_VO, &vo_sdl2_ops);
#endif
    }
    if (info.has_audio) {
#ifdef ENABLE_AO_SDL
        extern ao_wrapper_t ao_sdl_ops;
        dtplayer_register_plugin(DTP_PLUGIN_TYPE_AO, &ao_sdl_ops);
#endif
#ifdef ENABLE_AO_SDL2
        extern ao_wrapper_t ao_sdl2_ops;
        dtplayer_register_plugin(DTP_PLUGIN_TYPE_AO, &ao_sdl2_ops);
#endif
    }
    if (info.has_sub) {
#ifdef ENABLE_VO_SDL2
        extern so_wrapper_t so_sdl2_ops;
        dtplayer_register_plugin(DTP_PLUGIN_TYPE_SO, &so_sdl2_ops);
#endif
    }


    dtplayer_start(player_priv);

    //event handle
    gui_event_t event = EVENT_NONE;
    args_t arg;
    while (1) {
        if (player.quit == 1) {
            break;
        }
        event = player.gui->get_event(player.gui, &arg);
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
            dtplayer_seekto(player_priv, player.info.cur_time + arg.arg1);
            break;
        case EVENT_SEEK_RATIO: {
            int target_ts = player.info.duration * arg.arg1 / arg.arg2;
            dtplayer_seekto(player_priv, target_ts);
            break;
        }
        case EVENT_STOP:
            dt_info(TAG, "enter stop \n");
            dtplayer_stop(player_priv);
            goto QUIT_CHECK;
            break;
        case EVENT_RESIZE:
            dt_info(TAG, "resize, w:%d h:%d \n", arg.arg1, arg.arg2);
            player.disp_width = arg.arg1;
            player.disp_height = arg.arg2;
            player.gui->set_info(player.gui, GUI_CMD_SET_SIZE, arg);
            break;
        case EVENT_VOLUME_ADD:
            dt_info(TAG, " volume add \n");
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
    while (!player.quit) {
        usleep(10 * 1000);
        break;
    }
    player.gui->stop(player.gui);
    command_free(&program);
    dt_info("", "QUIT DTPLAYER-TEST\n");

    return 0;
}
