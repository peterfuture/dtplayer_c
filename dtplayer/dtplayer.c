#include <pthread.h>

#include "dt_ini.h"
#include "dt_event.h"

#include "dtplayer.h"
#include "dtdemuxer_api.h"
#include "dtplayer_io.h"
#include "dtplayer_host.h"
#include "dtplayer_update.h"
#include "dtstream.h"
#include "dtdemuxer.h"
#include "dtaudio.h"
#include "dtvideo.h"
#include "dtsub.h"

#define TAG "PLAYER"

#if ENABLE_FFMPEG
#include "libavutil/dict.h"
static AVDictionary *av_options;
#endif

static void *event_handle_loop(dtplayer_context_t * dtp_ctx);

static int register_ok = 0;
void player_register_all()
{
    if (!register_ok) {
        stream_register_all();
        demuxer_register_all();
        audio_register_all();
        video_register_all();
        sub_register_all();
        register_ok = 1;
    }
}

void player_remove_all()
{
    if (register_ok) {
        stream_remove_all();
        demuxer_remove_all();
        audio_remove_all();
        video_remove_all();
        sub_remove_all();
        register_ok = 0;
    }
}

void set_player_status(dtplayer_context_t * dtp_ctx,
                       player_status_t status)
{
    dtp_ctx->state.last_status = dtp_ctx->state.cur_status;
    dtp_ctx->state.cur_status = status;
}

player_status_t get_player_status(dtplayer_context_t * dtp_ctx)
{
    return dtp_ctx->state.cur_status;
}

static int player_service_init(dtplayer_context_t * dtp_ctx)
{
    service_t *service = dt_alloc_service(EVENT_SERVER_ID_PLAYER,
                                          EVENT_SERVER_NAME_PLAYER);
    dt_info(TAG, "PLAYER SERVER INIT:%p \n", service);
    dt_register_service(dtp_ctx->service_mgt, service);
    dtp_ctx->player_service = service;
    return 0;
}

static int player_service_release(dtplayer_context_t * dtp_ctx)
{
    service_t *service = (service_t *)dtp_ctx->player_service;
    dt_remove_service(dtp_ctx->service_mgt, service);
    dtp_ctx->player_service = NULL;
    return 0;
}

static int player_interrupt_cb_default(void *opaque)
{
    dtplayer_context_t *ctx = (dtplayer_context_t *)opaque;
    return ctx->abort_request;
}

int player_init(dtplayer_context_t * dtp_ctx)
{
    int ret = 0;
    dtplayer_para_t *para = &dtp_ctx->player_para;
    player_ctrl_t *ctrl_info = &dtp_ctx->ctrl_info;

    if (get_player_status(dtp_ctx) >= PLAYER_STATUS_INIT_ENTER) {
        dt_error(TAG, "player already inited \n");
        goto ERR0;
    }

    dt_update_setting();

    set_player_status(dtp_ctx, PLAYER_STATUS_INIT_ENTER);
    dt_info(TAG, "[%s:%d] START PLAYER INIT\n", __FUNCTION__, __LINE__);
    dtp_ctx->file_name = dtp_ctx->player_para.file_name;
    dtp_ctx->update_cb = dtp_ctx->player_para.update_cb;
    dtp_ctx->notify_cb = dtp_ctx->player_para.notify_cb;

    dtp_ctx->interrupt_cb.callback = player_interrupt_cb_default;
    dtp_ctx->interrupt_cb.opaque = (void *)dtp_ctx;

    dtdemuxer_para_t demux_para = {0};
    demux_para.file_name = dtp_ctx->file_name;
    demux_para.cb = &dtp_ctx->interrupt_cb;
#if ENABLE_FFMPEG
    demux_para.options = (void *)&av_options;
#endif
    ret = dtdemuxer_open(&dtp_ctx->demuxer_priv, &demux_para, dtp_ctx);
    if (ret < 0) {
        ret = -1;
        goto ERR0;
    }
    dtdemuxer_get_media_info(dtp_ctx->demuxer_priv, &dtp_ctx->media_info);
    dtp_media_info_t *info = dtp_ctx->media_info;
    track_info_t *tracks = &(info->tracks);
    /* setup player ctrl info
     *
     * have two ways to contrl, priority as follows
     * para
     * sys_set.ini
     *
     * */

    ctrl_info->has_audio = info->has_audio;
    ctrl_info->has_video = info->has_video;
    ctrl_info->has_sub = info->has_sub;
    ctrl_info->sync_enable = dtp_setting.player_sync_enable;
    if ((ctrl_info->has_audio && ctrl_info->has_video) == 0) {
        ctrl_info->sync_enable = 0;
    }
    dt_info(TAG,
            "initial setting, has audio:%d has video:%d has sub:%d sync_enable:%d\n",
            ctrl_info->has_audio, ctrl_info->has_video, ctrl_info->has_sub,
            ctrl_info->sync_enable);

    if (dtp_setting.player_noaudio) {
        ctrl_info->has_audio = 0;
    }
    if (dtp_setting.player_novideo) {
        ctrl_info->has_video = 0;
    }
    if (dtp_setting.player_nosub) {
        ctrl_info->has_sub = 0;
    }
    dt_info(TAG, "after ini setting, has audio:%d has video:%d has sub:%d \n",
            ctrl_info->has_audio, ctrl_info->has_video, ctrl_info->has_sub);

    //then update contrl info with para
    if (para->disable_avsync == 1) {
        ctrl_info->sync_enable = 0;
    }
    if (para->disable_audio == 1) {
        ctrl_info->has_audio = 0;
    }
    if (para->disable_video == 1) {
        ctrl_info->has_video = 0;
    }
    if (para->disable_sub == 1) {
        ctrl_info->has_sub = 0;
    }

    ctrl_info->disable_hw_acodec = para->disable_hw_acodec;
    ctrl_info->disable_hw_vcodec = para->disable_hw_vcodec;
    ctrl_info->disable_hw_scodec = para->disable_hw_scodec;
    ctrl_info->video_pixel_format = (para->video_pixel_format == -1) ?
                                    dtp_setting.video_pixel_format : para->video_pixel_format;

    // Update cur media index
    if (para->audio_index != -1) {
        info->cur_ast_index = para->audio_index;
    } else if (dtp_setting.audio_index != -1) {
        info->cur_ast_index = dtp_setting.audio_index;
    } else {
        info->cur_ast_index = 0;
    }

    if (para->video_index != -1) {
        info->cur_vst_index = para->video_index;
    } else if (dtp_setting.video_index != -1) {
        info->cur_vst_index = dtp_setting.video_index;
    } else {
        info->cur_vst_index = 0;
    }

    if (para->sub_index != -1) {
        info->cur_sst_index = para->sub_index;
    } else if (dtp_setting.sub_index != -1) {
        info->cur_sst_index = dtp_setting.sub_index;
    } else {
        info->cur_sst_index = 0;
    }

    //Fixme cur index valid check ...
    if (info->cur_ast_index >= tracks->ast_num) {
        dt_info(TAG, "Warnning, invalid ast index:%d max:%d reset to 0\n",
                info->cur_ast_index, tracks->ast_num - 1);
        info->cur_ast_index = 0;
    }

    if (info->cur_vst_index >= tracks->vst_num) {
        dt_info(TAG, "Warnning, invalid vst index:%d max:%d reset to 0\n",
                info->cur_vst_index, tracks->vst_num - 1);
        info->cur_vst_index = 0;
    }

    if (info->cur_sst_index >= tracks->sst_num) {
        dt_info(TAG, "Warnning, invalid sst index:%d max:%d reset to 0\n",
                info->cur_sst_index, tracks->sst_num - 1);
        info->cur_sst_index = 0;
    }

    dt_info(TAG, "ast_idx:%d vst_idx:%d sst_idx:%d \n",
            info->cur_ast_index,
            info->cur_vst_index,
            info->cur_sst_index);

    //other info setup
    ctrl_info->start_time = info->start_time;
    ctrl_info->first_time = -1;
    ctrl_info->width = para->width;
    ctrl_info->height = para->height;

    //invalid check
    if (!ctrl_info->has_audio && !ctrl_info->has_video) {
        dt_info(TAG, "HAVE NO A-V STREAM \n");
        goto ERR0;
    }

    //updte mediainfo --
    info->disable_audio = !ctrl_info->has_audio;
    info->disable_video = !ctrl_info->has_video;
    info->disable_sub = !ctrl_info->has_sub;
    dt_info(TAG, "Finally, ctrl info, audio:%d video:%d sub:%d \n",
            ctrl_info->has_audio, ctrl_info->has_video, ctrl_info->has_sub);


    dt_info(TAG, "[%s:%d] END PLAYER INIT, RET = %d\n", __FUNCTION__, __LINE__,
            ret);
    set_player_status(dtp_ctx, PLAYER_STATUS_INIT_EXIT);
    player_handle_cb(dtp_ctx);
    return 0;
ERR0:
    //set_player_status(dtp_ctx, PLAYER_STATUS_ERROR);
    //player_handle_cb(dtp_ctx);
    return -1;
}

int player_set_video_size(dtplayer_context_t * dtp_ctx, int width, int height)
{
    player_ctrl_t *pctrl = &dtp_ctx->ctrl_info;
    dt_info(TAG, "[%s:%d] width:%d height:%d\n", __FUNCTION__, __LINE__, width,
            height);

    if (pctrl->width == -1) {
        pctrl->width = width;
        pctrl->height = height;
    }

    if (pctrl->width == width && pctrl->height == height) {
        return -1;
    }

    int ret = player_host_resize(dtp_ctx, width, height);
    if (ret == 0) {
        pctrl->width = width;
        pctrl->height = height;
    }

    dt_info(TAG, "set dst width:%d height:%d \n", pctrl->width, pctrl->height);
    return 0;
}

int player_prepare(dtplayer_context_t *dtp_ctx)
{
    int ret = 0;
    pthread_t tid;

    if (get_player_status(dtp_ctx) >= PLAYER_STATUS_PREPARE_START) {
        dt_error(TAG, "player already started \n");
        return 0;
    }

    set_player_status(dtp_ctx, PLAYER_STATUS_PREPARE_START);

    /* init & start player service first */
    player_service_init(dtp_ctx);
    ret = pthread_create(&tid, NULL, (void *)&event_handle_loop, (void *)dtp_ctx);
    if (ret == -1) {
        dt_error(TAG, "file:%s [%s:%d] player io thread start failed \n", __FILE__,
                 __FUNCTION__, __LINE__);
        player_service_release(dtp_ctx);
        goto ERR3;
    }
    dt_info(TAG, "[%s:%d] create event handle loop thread id = %lu\n", __FUNCTION__,
            __LINE__, tid);
    dtp_ctx->event_loop_id = tid;

    ret = player_host_init(dtp_ctx);
    if (ret < 0) {
        dt_error(TAG, "[%s:%d] player_host_init failed!\n", __FUNCTION__, __LINE__);
        goto ERR3;
    }
    ret = start_io_thread(dtp_ctx);
    if (ret == -1) {
        dt_error(TAG, "file:%s [%s:%d] player io thread start failed \n", __FILE__,
                 __FUNCTION__, __LINE__);
        goto ERR2;
    }

    // Notify vdec type info
    host_state_t host_state;
    dtp_state_t *state = &dtp_ctx->state;
    player_host_get_info(dtp_ctx, HOST_CMD_GET_STATE, (unsigned long)(&host_state));
    state->vdec_type = host_state.vdec_type;
    player_handle_cb(dtp_ctx);

    set_player_status(dtp_ctx, PLAYER_STATUS_PREPARED);

    return 0;
ERR2:
    player_host_stop(dtp_ctx);
ERR3:
    set_player_status(dtp_ctx, PLAYER_STATUS_ERROR);
    player_handle_cb(dtp_ctx);
    pthread_join(dtp_ctx->event_loop_id, NULL);
    return ret;

}

int player_start(dtplayer_context_t * dtp_ctx)
{
    int ret;
    set_player_status(dtp_ctx, PLAYER_STATUS_START);
    ret = player_host_start(dtp_ctx);
    if (ret != 0) {
        dt_error(TAG, "[%s:%d] player host start failed \n", __FILE__, __LINE__);
        set_player_status(dtp_ctx, PLAYER_STATUS_ERROR);
        goto ERR1;
    }

    if (dtp_ctx->abort_request == 1) {
        return 0;
    }

    dt_info(TAG, "PLAYER START OK\n");
    set_player_status(dtp_ctx, PLAYER_STATUS_RUNNING);
    player_handle_cb(dtp_ctx);

    return 0;

ERR1:
    dtp_ctx->abort_request = 1;
    stop_io_thread(dtp_ctx);
    player_host_stop(dtp_ctx);
    set_player_status(dtp_ctx, PLAYER_STATUS_ERROR);
    player_handle_cb(dtp_ctx);
    pthread_join(dtp_ctx->event_loop_id, NULL);
    return ret;
}

int player_pause(dtplayer_context_t * dtp_ctx)
{
    if (get_player_status(dtp_ctx) == PLAYER_STATUS_PAUSED) {
        return player_resume(dtp_ctx);
    }

    if (player_host_pause(dtp_ctx) == -1) {
        dt_error(TAG, "PAUSE NOT HANDLED\n");
        return -1;
    }
    set_player_status(dtp_ctx, PLAYER_STATUS_PAUSED);
    player_handle_cb(dtp_ctx);
    return 0;
}

int player_resume(dtplayer_context_t * dtp_ctx)
{
    if (get_player_status(dtp_ctx) != PLAYER_STATUS_PAUSED) {
        return -1;
    }
    if (player_host_resume(dtp_ctx) == -1) {
        dt_error(TAG, "RESUME NOT HANDLED\n");
        return -1;
    }
    set_player_status(dtp_ctx, PLAYER_STATUS_RESUME);
    player_handle_cb(dtp_ctx);
    set_player_status(dtp_ctx, PLAYER_STATUS_RUNNING);
    return 0;
}

int player_dec_reset(dtplayer_context_t *dtp_ctx)
{
    player_ctrl_t *ctrl_info = &dtp_ctx->ctrl_info;
    if (get_player_status(dtp_ctx) < PLAYER_STATUS_INIT_EXIT) {
        return -1;
    }
    pause_io_thread(dtp_ctx);
    player_host_stop(dtp_ctx);
    player_host_init(dtp_ctx);
    ctrl_info->ctrl_wait_key_frame = 1;
    resume_io_thread(dtp_ctx);
    player_host_start(dtp_ctx);
    return 0;
}

int player_seekto(dtplayer_context_t * dtp_ctx, int seek_time)
{
    player_ctrl_t *ctrl_info = &dtp_ctx->ctrl_info;

    if (get_player_status(dtp_ctx) < PLAYER_STATUS_INIT_EXIT) {
        return -1;
    }

    // Handle buffeirng-seek case
    if (get_player_status(dtp_ctx) < PLAYER_STATUS_BUFFERING) {
        set_player_status(dtp_ctx, PLAYER_STATUS_RUNNING);
        dtp_ctx->notify_cb(dtp_ctx->cookie, DTP_EVENTS_BUFFERING_END, NULL, NULL);
    }

    dtp_state_t *play_stat = &dtp_ctx->state;
    play_stat->cur_time = (int64_t)seek_time;
    play_stat->cur_time_ms = (int64_t)seek_time * 1000;
    dt_info(TAG, "[%s:%d]trace\n", __FUNCTION__, __LINE__);
    set_player_status(dtp_ctx, PLAYER_STATUS_SEEK_ENTER);
    if (io_thread_running(dtp_ctx)) {
        pause_io_thread(dtp_ctx);
    }
    dt_info(TAG, "[%s:%d]trace\n", __FUNCTION__, __LINE__);
    //player_host_pause(dtp_ctx);
    player_host_stop(dtp_ctx);
    dt_info(TAG, "[%s:%d]trace\n", __FUNCTION__, __LINE__);
    int ret = dtdemuxer_seekto(dtp_ctx->demuxer_priv, (int64_t)seek_time);
    if (ret == -1) {
        goto FAIL;
    }
    dt_info(TAG, "[%s:%d]trace\n", __FUNCTION__, __LINE__);
    player_host_init(dtp_ctx);
    // key frame waiting
    ctrl_info->ctrl_wait_key_frame = 1;
    if (io_thread_running(dtp_ctx)) { // maybe io thread already quit
        resume_io_thread(dtp_ctx);
    } else {
        start_io_thread(dtp_ctx);
    }

    player_update_state(dtp_ctx);
    set_player_status(dtp_ctx, PLAYER_STATUS_SEEK_EXIT);
    player_handle_cb(dtp_ctx);

    //check if another seek event comming soom
    service_t *service = (service_t *)dtp_ctx->player_service;
    event_t *event = dt_peek_event(service);
    if (event != NULL && event->type == PLAYER_EVENT_SEEK) {
        dt_info(TAG, "execute queue seekto event \n");
        return 0;
    }
    player_host_start(dtp_ctx);
    set_player_status(dtp_ctx, PLAYER_STATUS_RUNNING);
    player_update_state(dtp_ctx);
    player_handle_cb(dtp_ctx);

    return 0;
FAIL:
    //seek fail, continue running
    dt_error(TAG, "[%s:%d] seek failed, continue playing \n", __FUNCTION__,
             __LINE__);
    resume_io_thread(dtp_ctx);
    if (io_thread_running(dtp_ctx)) {
        player_host_resume(dtp_ctx);
    }
    set_player_status(dtp_ctx, PLAYER_STATUS_SEEK_EXIT);
    player_handle_cb(dtp_ctx);
    set_player_status(dtp_ctx, PLAYER_STATUS_RUNNING);
    player_handle_cb(dtp_ctx);
    return 0;
}

int player_get_mediainfo(dtplayer_context_t * dtp_ctx, dtp_media_info_t *info)
{
    if (!info) {
        return -1;
    }
    memcpy(info, dtp_ctx->media_info, sizeof(*info));
    return 0;
}

int player_stop(dtplayer_context_t * dtp_ctx)
{
    dt_info(TAG, "PLAYER STOP STATUS SET\n");
    set_player_status(dtp_ctx, PLAYER_STATUS_STOP);
    dtp_ctx->abort_request = 1;
    return 0;
}

int player_get_parameter(dtplayer_context_t *dtp_ctx, int cmd,
                         unsigned long arg)
{
    return 0;
}

int player_set_parameter(dtplayer_context_t *dtp_ctx, int cmd,
                         unsigned long arg)
{
    if (!dtp_ctx) {
        return -1;
    }
    switch (cmd) {
    case DTP_CMD_SET_AODEVICE:
        dtp_ctx->ao_device = (void *)arg;
        break;
    case DTP_CMD_SET_VODEVICE:
        dtp_ctx->vo_device = (void *)arg;
        break;
    case DTP_CMD_SET_SODEVICE:
        dtp_ctx->so_device = (void *)arg;
        break;
    case DTP_CMD_SET_LOOPING:
        dtp_ctx->player_para.loop_mode = (int)arg;
        break;
    default:
        break;
    }

    return 0;
}

void player_set_option(dtplayer_context_t *dtp_ctx, int category,
                       const char *name, const char *value)
{
#if ENABLE_FFMPEG
    AVDictionary **d = &av_options;           // "create" an empty dictionary
    char *k = av_strdup(name);       // if your strings are already allocated,
    char *v = av_strdup(value);     // you can avoid copying them like this
#endif
    if (category == OPTION_CATEGORY_DTP) {
        goto dtp_option;
    }
    if (category == OPTION_CATEGORY_FFMPEG) {
        return; // no support, will crash
        //goto ffmpeg_option;
    }
    // unknown option
    return;
dtp_option:
    if (strcmp(name, "player.live_timeout") == 0) {
        dtp_setting.player_live_timeout = atoi(value);
        dt_info(TAG, "use specify live timeout:%dms\n",
                dtp_setting.player_live_timeout);
    } else if (strcmp(name, "player.log_level") == 0) {
        dtp_setting.log_level = atoi(value);
        dt_info(TAG, "use specify loglevel:%d\n", dtp_setting.log_level);
        dt_set_log_level(dtp_setting.log_level);
    } else if (strcmp(name, "port.audio_max_num") == 0) {
        dtp_setting.audio_max_num = atoi(value);
        dt_info(TAG, "use specify audio_max_num:%d\n", atoi(value));
    } else if (strcmp(name, "port.audio_max_size") == 0) {
        dtp_setting.audio_max_size = atoi(value);
        dt_info(TAG, "use specify audio_max_size:%d\n", atoi(value));
    } else if (strcmp(name, "port.video_max_num") == 0) {
        dtp_setting.video_max_num = atoi(value);
        dt_info(TAG, "use specify video_max_num:%d\n", atoi(value));
    } else if (strcmp(name, "port.video_max_size") == 0) {
        dtp_setting.video_max_size = atoi(value);
        dt_info(TAG, "use specify video_max_size:%d\n", atoi(value));
    }
    return;
ffmpeg_option:
#if ENABLE_FFMPEG
    av_dict_set(d, k, v, AV_DICT_DONT_STRDUP_KEY | AV_DICT_DONT_STRDUP_VAL);
    dt_info(TAG, "ffmpeg option set. key:%s value:%s. count:%d\n", k, v,
            av_dict_count(*d));
#endif
    return;
}

static char * get_event_name(int cmd)
{
    switch (cmd) {
    case PLAYER_EVENT_START:
        return "player_event_start";
    case PLAYER_EVENT_PAUSE:
        return "player_event_pause";
    case PLAYER_EVENT_RESUME:
        return "player_event_resume";
    case PLAYER_EVENT_STOP:
        return "player_event_stop";
    case PLAYER_EVENT_SEEK:
        return "player_event_seek";
    default:
        return "unkown cmd";
    }
    return "null";
}

static int player_handle_event(dtplayer_context_t * dtp_ctx)
{
    service_t *service = (service_t *)dtp_ctx->player_service;
    event_t *event = dt_get_event(service);
START:
    if (!event) {
        dt_debug(TAG, "GET EVENT NULL \n");
        return 0;
    }
    dt_info(TAG, "GET EVENT:%d %s\n", event->type, get_event_name(event->type));
    switch (event->type) {
    case PLAYER_EVENT_START:
        player_start(dtp_ctx);
        break;
    case PLAYER_EVENT_PAUSE:
        player_pause(dtp_ctx);
        break;
    case PLAYER_EVENT_RESUME:
        player_resume(dtp_ctx);
        break;
    case PLAYER_EVENT_STOP:
        player_stop(dtp_ctx);
        break;
    case PLAYER_EVENT_SEEK:
        player_seekto(dtp_ctx, event->para.np);
        break;
    default:
        break;
    }
    if (event) {
        free(event);
        event = NULL;
    }

    //check if still have event
    event = dt_peek_event(service);
    if (event) {
        event = dt_get_event(service);
        goto START;
    }
    return 0;
}

static int player_buffering_check(dtplayer_context_t * dtp_ctx)
{
    #define ABUF_ENTER_SIZE 1024*100
    #define VBUF_ENTER_SIZE 1024*512

    #define ABUF_EXIT_SIZE 1024*300
    #define VBUF_EXIT_SIZE 1024*1024

    dtp_state_t *stat = &dtp_ctx->state;
    player_ctrl_t *ctrl_info = &dtp_ctx->ctrl_info;
    int acache_size = 0;
    int acache_time = 0;
    int vcache_size = 0;
    int vcache_time = 0;

    int has_audio = ctrl_info->has_audio;
    int has_video = ctrl_info->has_video;

    int eof = dtp_ctx->ctrl_info.eof_flag;

    char *url = dtp_ctx->file_name;
    int live_playback = dt_strstart(url, "udp://", &url) != 0
        || dt_strstart(url, "rtp://", &url) != 0
        || dt_strstart(url, "rtmp://", &url) != 0;

    int network_playback = dt_strstart(url, "udp://", &url) != 0
        || dt_strstart(url, "rtp://", &url) != 0
        || dt_strstart(url, "http://", &url) != 0
        || dt_strstart(url, "https://", &url) != 0
        || dt_strstart(url, "rtsp://", &url) != 0
        || dt_strstart(url, "rtmp://", &url) != 0;

    int cur_status = get_player_status(dtp_ctx);

    acache_size = stat->acache_size;
    acache_time = stat->acache_time;
    vcache_size = stat->vcache_size;
    vcache_time = stat->vcache_time;

    if (network_playback == 0)
        return 0;

    if (has_audio == 0 && has_video == 0)
        return 0;
    
    int need_enter = acache_size <= ABUF_ENTER_SIZE && vcache_size <= VBUF_ENTER_SIZE;
    int need_exit =  acache_size >= ABUF_EXIT_SIZE && vcache_size >= VBUF_EXIT_SIZE;
    if (eof == 1) {
        need_enter = 0;
        need_exit = 1;
    }

    // only enter buffering when running
    if (need_enter && get_player_status(dtp_ctx) == PLAYER_STATUS_RUNNING) {
        if (player_host_pause(dtp_ctx) == -1) {
            dt_error(TAG, "PAUSE NOT HANDLED\n");
            return -1;
        }
        set_player_status(dtp_ctx, PLAYER_STATUS_BUFFERING);
        dtp_ctx->notify_cb(dtp_ctx->cookie, DTP_EVENTS_BUFFERING_START, NULL, NULL);
        dt_info(TAG, "Enter buffing: Audio[%d<%d] Video[%d<%d]\n", 
            acache_size, ABUF_ENTER_SIZE, vcache_size, VBUF_ENTER_SIZE);
    }

    if (need_exit && get_player_status(dtp_ctx) == PLAYER_STATUS_BUFFERING) {
        if (player_host_resume(dtp_ctx) == -1) {
            dt_error(TAG, "Resume NOT HANDLED\n");
            return -1;
        }
        set_player_status(dtp_ctx, PLAYER_STATUS_RUNNING);
        dtp_ctx->notify_cb(dtp_ctx->cookie, DTP_EVENTS_BUFFERING_END, NULL, NULL);
        dt_info(TAG, "Exit buffing: Audio[%d>%d] Video[%d>%d]\n", 
            acache_size, ABUF_EXIT_SIZE, vcache_size, VBUF_EXIT_SIZE);
    }
    return 0;
}

static void *event_handle_loop(dtplayer_context_t * dtp_ctx)
{
    int render_closed = 0;
    int loop = dtp_ctx->player_para.loop_mode;
    dtp_state_t *stat = &dtp_ctx->state;

    while (1) {
        player_handle_event(dtp_ctx);
        if (get_player_status(dtp_ctx) == PLAYER_STATUS_STOP) {
            goto QUIT;
        }

        usleep(300 * 1000);    // 1/3s update
        if (get_player_status(dtp_ctx) < PLAYER_STATUS_RUNNING) {
            continue;
        }

        player_update_state(dtp_ctx);
        player_handle_cb(dtp_ctx);

        // buffering check
        player_buffering_check(dtp_ctx);

        if (!dtp_ctx->ctrl_info.eof_flag) {
            continue;
        }

        player_host_get_info(dtp_ctx, HOST_CMD_GET_RENDER_CLOSED,
                             (unsigned long)(&render_closed));
        if (render_closed == 1 && stat->full_time > 0
            && stat->cur_time >= stat->full_time - 10) {
            if (loop > 0) {
                event_t *event = dt_alloc_event(EVENT_SERVER_ID_PLAYER, PLAYER_EVENT_SEEK);
                event->para.np = 0;
                dt_send_event_sync(dtp_ctx->service_mgt, event);
                continue;
            }

            dt_info(TAG, "playend.\n");
            goto QUIT;
        }
    }
    /* when playend itself ,we need to release manually */
QUIT:
    stop_io_thread(dtp_ctx);
    player_host_stop(dtp_ctx);
#if ENABLE_FFMPEG
    //AVDictionary **d = &av_options;           // "create" an empty dictionary
    //dt_info(TAG, "dict count:%d \n", av_dict_count(*d));
    //av_dict_free(d);
    //av_options = NULL;
#endif
    dtdemuxer_close(dtp_ctx->demuxer_priv);
    player_service_release(dtp_ctx);
    dt_service_release(dtp_ctx->service_mgt);
    dt_info(TAG, "EXIT PLAYER EVENT HANDLE LOOP\n");
    set_player_status(dtp_ctx, PLAYER_STATUS_EXIT);
    player_handle_cb(dtp_ctx);
    free(dtp_ctx);
    player_remove_all();
    pthread_exit(NULL);
    return NULL;
}
