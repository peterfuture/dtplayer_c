#include "dt_setting.h"
#include "dt_ini.h"
#include "dt_log.h"

#include <stdlib.h>
#include <string.h>

dt_setting_t dtp_setting;

#define TAG "DTP-SETTING"

int dt_update_setting()
{
    char value[512];

    dt_ini_open(NULL);

    if (dtp_setting.log_level <= 0) {
        dtp_setting.log_level = 0;
        if (dt_ini_get_entry("LOG", "log.level", value) >= 0) {
            dtp_setting.log_level = atoi(value);
        }
    }
    memset(dtp_setting.log_filter, 0, 1024);
    dtp_setting.log_filter[0] = '\0';
    if (dt_ini_get_entry("LOG", "log.filter", value) >= 0 && strlen(value) > 0) {
        memcpy(dtp_setting.log_filter, value, strlen(value));
    }
    dt_info(TAG, "log.filter:%s \n", dtp_setting.log_filter);
    // STREAM
    dtp_setting.stream_cache = 0;
    if (dt_ini_get_entry("STREAM", "stream.cache", value) >= 0) {
        dtp_setting.stream_cache = atoi(value);
    }
    dtp_setting.stream_cache_size = 32 * 1024 * 1024; // remove later
    if (dt_ini_get_entry("STREAM", "stream.cachesize", value) >= 0) {
        dtp_setting.stream_cache_size = atoi(value);
    }

    // DEMUXER
    dtp_setting.demuxer_probe = 0;
    if (dt_ini_get_entry("DEMUXER", "demuxer.probe", value) >= 0) {
        dtp_setting.demuxer_probe = atoi(value);
    }
    dtp_setting.demuxer_probe_size = 1024 * 1024;
    if (dt_ini_get_entry("DEMUXER", "demuxer.probesize", value) >= 0) {
        dtp_setting.demuxer_probe_size = atoi(value);
    }
    dtp_setting.demuxer_seek_keyframe = 1;
    if (dt_ini_get_entry("DEMUXER", "demuxer.seek.keyframe", value) >= 0) {
        dtp_setting.demuxer_seek_keyframe = atoi(value);
    }

    // AUDIO
    dtp_setting.audio_downmix = 1;
    if (dt_ini_get_entry("AUDIO", "audio.downmix", value) >= 0) {
        dtp_setting.audio_downmix = atoi(value);
    }
    dtp_setting.audio_index = -1;
    if (dt_ini_get_entry("AUDIO", "audio.index", value) >= 0) {
        dtp_setting.audio_index = atoi(value);
    }

    // VIDEO
    dtp_setting.video_pts_mode = 0;
    if (dt_ini_get_entry("VIDEO", "video.ptsmode", value) >= 0) {
        dtp_setting.video_pts_mode = atoi(value);
    }
    dtp_setting.video_pixel_format = 0;
    if (dt_ini_get_entry("VIDEO", "video.pixelformat", value) >= 0) {
        dtp_setting.video_pixel_format = atoi(value);
    }
    dtp_setting.video_index = -1;
    if (dt_ini_get_entry("VIDEO", "video.index", value) >= 0) {
        dtp_setting.video_index = atoi(value);
    }
    dtp_setting.video_render_mode = 0;
    if (dt_ini_get_entry("VIDEO", "video.render_mode", value) >= 0) {
        dtp_setting.video_render_mode = atoi(value);
    }
    dtp_setting.video_render_duration = 0;
    if (dt_ini_get_entry("VIDEO", "video.render_duration", value) >= 0) {
        dtp_setting.video_render_duration = atoi(value);
    }

    // SUB
    dtp_setting.sub_index = -1;
    if (dt_ini_get_entry("SUB", "sub.index", value) >= 0) {
        dtp_setting.sub_index = atoi(value);
    }


    // PLAYER
    dtp_setting.player_dump_mode = 0;
    if (dt_ini_get_entry("PLAYER", "player.dumpmode", value) >= 0) {
        dtp_setting.player_dump_mode = atoi(value);
    }
    dtp_setting.player_noaudio = 0;
    if (dt_ini_get_entry("PLAYER", "player.noaudio", value) >= 0) {
        dtp_setting.player_noaudio = atoi(value);
    }
    dtp_setting.player_novideo = 0;
    if (dt_ini_get_entry("PLAYER", "player.novideo", value) >= 0) {
        dtp_setting.player_novideo = atoi(value);
    }
    dtp_setting.player_sync_enable = 1;
    if (dt_ini_get_entry("PLAYER", "player.sync.enable", value) >= 0) {
        dtp_setting.player_sync_enable = atoi(value);
    }
    dtp_setting.player_seekmode = -1;
    if (dt_ini_get_entry("PLAYER", "player.seekmode", value) >= 0) {
        dtp_setting.player_seekmode = atoi(value);
    }
    dtp_setting.player_live_timeout = 3000;
    if (dt_ini_get_entry("PLAYER", "player.live_timeout", value) >= 0) {
        dtp_setting.player_live_timeout = atoi(value);
    }

    // HOST
    dtp_setting.host_drop = 1;
    if (dt_ini_get_entry("HOST", "host.drop", value) >= 0) {
        dtp_setting.host_drop = atoi(value);
    }
    dtp_setting.host_drop_thres = 100;
    if (dt_ini_get_entry("HOST", "host.drop.thres", value) >= 0) {
        dtp_setting.host_drop_thres = atoi(value);
    }
    dtp_setting.host_drop_timeout = 3000;
    if (dt_ini_get_entry("HOST", "host.drop.timeout", value) >= 0) {
        dtp_setting.host_drop_timeout = atoi(value);
    }
    dtp_setting.host_sync_thres = 100;
    if (dt_ini_get_entry("HOST", "host.sync.thres", value) >= 0) {
        dtp_setting.host_sync_thres = atoi(value);
    }

    dt_info(TAG, "update dtp setting ok \n");

    dt_ini_release();
    return 0;
}
