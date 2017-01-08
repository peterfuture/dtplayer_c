#include "dt_player.h"
#include "version.h"
#include "stream_wrapper.h"
#include "demuxer_wrapper.h"
#include "dtp_plugin.h"

#include "dtp.h"
#include "commander.h"

#include <stdio.h>
#include <string.h>

#define TAG "DT-PROBE"

static player_t player;

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
    command_option(&program, "-dw", "--width <n>", "specify destiny width",
                   on_set_width);
    command_option(&program, "-dh", "--height <n>", "specify destiny height",
                   on_set_height);
    command_option(&program, "-na", "--disable_audio", "disable audio",
                   on_disable_audio);
    command_option(&program, "-nv", "--disable_video", "disable video",
                   on_disable_video);
    command_option(&program, "-ns", "--disable_sub", "disable sub", on_disable_sub);
    command_option(&program, "-ast", "--audio_index <n>", "specify audio index",
                   on_select_audio);
    command_option(&program, "-vst", "--video_index <n>", "specify video index",
                   on_select_video);
    command_option(&program, "-sst", "--sub_index <n>", "specify sub index",
                   on_select_sub);
    command_option(&program, "-l", "--loop <n>", "enable loop", on_loop);
    command_option(&program, "-nsy", "--disable-sync", "disable avsync",
                   on_disable_sync);
    command_option(&program, "-vpf", "--video_pixel_format <n>",
                   "video pixel format: 0-yuv420 1-rgb565 2-rgb24 3-rgba", on_select_vpf);

    command_parse(&program, argc, argv);

    //version_info();
    if (argc < 2) {
        command_help(&program);
        return 0;
    }

    memset(&player, 0, sizeof(player_t));
    para.update_cb = NULL;
    para.file_name = argv[argc - 1];
    player.file_name = para.file_name;

    player_register_all();

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
    return 0;
}
