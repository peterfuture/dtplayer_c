#include "dtplayer_update.h"
#include "dtplayer_host.h"

#define TAG "PLAYER-UPDATE"

#define FIRST_TIME_DIFF_MAX 2*90000 // 2s

static int calc_cur_time(dtplayer_context_t * dtp_ctx,
                         host_state_t * host_state)
{
    player_ctrl_t *ctrl_info = &dtp_ctx->ctrl_info;
    dtp_state_t *play_stat = &dtp_ctx->state;

    int has_audio = ctrl_info->has_audio;
    int has_video = ctrl_info->has_video;
    int64_t apts_first = host_state->pts_audio_first;
    int64_t vpts_first = host_state->pts_video_first;

    if (ctrl_info->start_time >= 0) {
        play_stat->start_time = ctrl_info->start_time;
        // For now, we use first time from ffmpeg ic->start_time
        ctrl_info->first_time = ctrl_info->start_time;
        dt_info(TAG, "START TIME:0x%llx \n", ctrl_info->start_time);
    }

    if (ctrl_info->first_time == -1) {
        if (has_audio && has_video) {
            if (PTS_VALID(apts_first) && PTS_VALID(vpts_first)) {
                if (llabs(apts_first - vpts_first) < FIRST_TIME_DIFF_MAX) {
                    ctrl_info->first_time = (apts_first > vpts_first) ? vpts_first : apts_first;
                } else {
                    if (llabs(vpts_first - play_stat->start_time) < FIRST_TIME_DIFF_MAX) {
                        ctrl_info->first_time = vpts_first;
                    } else if (llabs(apts_first - play_stat->start_time) < FIRST_TIME_DIFF_MAX) {
                        ctrl_info->first_time = apts_first;
                    }
                }
            } else if (PTS_VALID(apts_first)) {
                ctrl_info->first_time = apts_first;
            } else if (PTS_VALID(vpts_first)) {
                ctrl_info->first_time = vpts_first;
            }
        } else {
            ctrl_info->first_time = ctrl_info->start_time;
        }

        dt_info(TAG,
                "[apts_first:0x%llx] [vpts_first:0x%llx] [starttime:0x%llx] -> first_time:0x%llx \n",
                apts_first, vpts_first, play_stat->start_time, ctrl_info->first_time);
    }

    if (play_stat->full_time == 0) {
        play_stat->full_time = dtp_ctx->media_info->duration;
    }

    int64_t sys_time_start = host_state->sys_time_start;
    int64_t sys_time_current = host_state->sys_time_current;
    int64_t pts_audio_current = host_state->pts_audio_current;
    int     audio_discontinue_flag = host_state->audio_discontinue_flag;
    int64_t audio_discontinue_point = host_state->audio_discontinue_point;
    int64_t pts_video_current = host_state->pts_video_current;
    int     video_discontinue_flag = host_state->video_discontinue_flag;
    int64_t video_discontinue_point = host_state->video_discontinue_point;

    int avdiff = llabs(pts_audio_current - pts_video_current) / DT_PTS_FREQ_MS;
    int64_t sys_time = -1;
    int64_t start_time = ctrl_info->first_time;
    int discontinue_flag = 0;

    if (has_audio && has_video) {
        if (audio_discontinue_flag
            && video_discontinue_flag) { // both discontinue occured
            play_stat->discontinue_point_ms = play_stat->cur_time_ms;
            if (avdiff < AVSYNC_THRESHOLD_MAX) {
                player_host_set_info(dtp_ctx, HOST_CMD_SET_DISCONTINUE_FLAG,
                                     (unsigned long)(&discontinue_flag));
            }
            sys_time = pts_video_current;
            if (audio_discontinue_point > 0) {
                start_time = audio_discontinue_point;
            }
        } else if (audio_discontinue_flag) { // only audio discontinue occured
            if (avdiff < AVSYNC_THRESHOLD_MAX) {
                player_host_set_info(dtp_ctx, HOST_CMD_SET_DISCONTINUE_FLAG,
                                     (unsigned long)(&discontinue_flag));
            }
            sys_time = pts_video_current;
            if (video_discontinue_point > 0) {
                start_time = video_discontinue_point;
            }
        } else if (video_discontinue_flag) { // only video discontinue occured
            if (avdiff < AVSYNC_THRESHOLD_MAX) {
                player_host_set_info(dtp_ctx, HOST_CMD_SET_DISCONTINUE_FLAG,
                                     (unsigned long)(&discontinue_flag));
            }
            sys_time = pts_audio_current;
            if (audio_discontinue_point > 0) {
                start_time = audio_discontinue_point;
            }
        } else {
            if (PTS_VALID(vpts_first)) {
                sys_time = pts_video_current;
            } else {
                sys_time = pts_audio_current;
            }
            if (audio_discontinue_point > 0) {
                start_time = audio_discontinue_point;
            }
        }
    } else {
        sys_time = sys_time_current;
        start_time = sys_time_start;
    }

    sys_time += play_stat->discontinue_point_ms * DT_PTS_FREQ_MS;
    sys_time -= start_time;
    play_stat->cur_time = sys_time / DT_PTS_FREQ;
    play_stat->cur_time_ms = sys_time / DT_PTS_FREQ_MS;

    dt_debug(TAG, "---------------------------------------------\n");
    dt_debug(TAG,
             "apts:%llx audio_discontinue_flag:%d audio_discontinue_point:%llx\n",
             pts_audio_current, audio_discontinue_flag, audio_discontinue_point);
    dt_debug(TAG,
             "vpts:%llx video_discontinue_flag:%d video_discontinue_point:%llx\n",
             pts_video_current, video_discontinue_flag, video_discontinue_point);
    dt_debug(TAG, "sys_time:%llx start_time:%llx\n",
             (sys_time - play_stat->discontinue_point_ms * DT_PTS_FREQ_MS), start_time);
    dt_debug(TAG, "---------------------------------------------\n");

    return 0;
}

/*
 * handle callback, update status and cur_time to uplevel
 * */
int player_handle_cb(dtplayer_context_t * dtp_ctx)
{
    dtp_state_t *state = &dtp_ctx->state;
    if (dtp_ctx->update_cb) {
        dtp_ctx->update_cb(dtp_ctx->cookie, state);
    }
    return 0;
}

static int need_reset_decoder(dtplayer_context_t *dtp_ctx, host_state_t *state)
{
    dtp_media_info_t *info = dtp_ctx->media_info;
    if (state->vdec_last_ms <= 0) {
        return 0;
    }
    if (info->livemode <= 0) {
        return 0;
    }

    int64_t block_ms = dt_gettime() / 1000 - state->vdec_last_ms;
    if (block_ms < dtp_setting.player_live_timeout) {
        return 0;
    }
    dt_info(TAG,
            "[%s:%d] video block too long reset. last decoded time:%lldms now:%lldms block:%lld ms\n",
            __FUNCTION__, __LINE__, state->vdec_last_ms, dt_gettime() / 1000, block_ms);

    return 1;
}

void player_update_state(dtplayer_context_t * dtp_ctx)
{
    dtp_state_t *play_stat = &dtp_ctx->state;
    host_state_t host_state;

    if (get_player_status(dtp_ctx) < PLAYER_STATUS_RUNNING) {
        return;
    }

    /*update host state */
    player_host_get_info(dtp_ctx, HOST_CMD_GET_STATE, (unsigned long)(&host_state));

    /* live reset check */
    int need_reset = need_reset_decoder(dtp_ctx, &host_state);
    if (need_reset) {
        player_dec_reset(dtp_ctx);
        return;
    }

    /*calc cur time */
    calc_cur_time(dtp_ctx, &host_state);

    /*update lav & cache info*/
    play_stat->acache_size = host_state.abuf_level;
    play_stat->vcache_size = host_state.vbuf_level;
    play_stat->scache_size = host_state.sbuf_level;
   
    /*show info */
    dt_info(TAG,
            "[%s:%d]alevel:%d vlevel:%d slevel:%d cur_time:%lld(s) %lld(ms) duration:%lld(s) eof:%d\n",
            __FUNCTION__, __LINE__, host_state.abuf_level, host_state.vbuf_level,
            host_state.sbuf_level, play_stat->cur_time, play_stat->cur_time_ms,
            dtp_ctx->media_info->duration, dtp_ctx->ctrl_info.eof_flag);
}
