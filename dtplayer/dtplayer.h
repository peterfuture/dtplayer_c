#ifndef DTPLAYER_H
#define DTPLAYER_H

#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


#include "dt_utils.h"

#include "dtp.h"
#include "dthost_api.h"

typedef struct {
    int status;                 // 0 start 1 pause 2 quit
    int flag;
    pthread_t tid;
} io_loop_t;

typedef struct {
    // basic control
    int has_audio;
    int has_video;
    int has_sub;
    int video_pixel_format;

    // stream info
    int64_t start_time;
    int64_t first_time;

    // dest width height
    int width;
    int height;

    // ctrl flag
    int eof_flag;
    int sync_enable;
    int disable_hw_acodec;
    int disable_hw_vcodec;
    int disable_hw_scodec;

    // seek contrl
    int ctrl_wait_key_frame;
} player_ctrl_t;

typedef struct dtplayer_context {
    char *file_name;
    dtplayer_para_t player_para;

    void *demuxer_priv;
    dtp_media_info_t *media_info;

    player_ctrl_t ctrl_info;
    dthost_para_t host_para;
    void *host_priv;

    dtp_state_t state;

    dtp_update_cb update_cb;
    player_notify notify_cb;
    io_loop_t io_loop;
    pthread_t event_loop_id;

    int abort_request;
    dt_interrupt_cb interrupt_cb;

    service_t *player_service;
    void *cookie;
    dt_service_mgt_t *service_mgt;

    // audio-video-sub out device
    void *ao_device;
    void *vo_device;
    void *so_device;
} dtplayer_context_t;

void player_register_all();
void player_remove_all();
int player_init(dtplayer_context_t * dtp_ctx);
int player_set_video_size(dtplayer_context_t * dtp_ctx, int width, int height);
int player_prepare(dtplayer_context_t * dtp_ctx);
int player_start(dtplayer_context_t * dtp_ctx);
int player_pause(dtplayer_context_t * dtp_ctx);
int player_resume(dtplayer_context_t * dtp_ctx);
int player_dec_reset(dtplayer_context_t * dtp_ctx);
int player_seekto(dtplayer_context_t * dtp_ctx, int seek_time); //s
void set_player_status(dtplayer_context_t * dtp_ctx,
                       player_status_t status);
player_status_t get_player_status(dtplayer_context_t * dtp_ctx);
int player_get_mediainfo(dtplayer_context_t * dtp_ctx,
                         dtp_media_info_t *info); //s
int player_stop(dtplayer_context_t * dtp_ctx);
int player_video_resize(dtplayer_context_t * dtp_ctx, int w, int h);
int player_get_parameter(dtplayer_context_t *dtp_ctx, int cmd,
                         unsigned long arg);
int player_set_parameter(dtplayer_context_t *dtp_ctx, int cmd,
                         unsigned long arg);
void player_set_option(dtplayer_context_t *dtp_ctx, int category,
                       const char *name, const char *value);

#endif
