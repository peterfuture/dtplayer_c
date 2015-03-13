#include "dtplayer_update.h"

#define TAG "PLAYER-UPDATE"

static int calc_cur_time(dtplayer_context_t * dtp_ctx, host_state_t * host_state)
{
    player_ctrl_t *ctrl_info = &dtp_ctx->ctrl_info;
    player_state_t *play_stat = &dtp_ctx->state;

    if (ctrl_info->start_time > 0) {
        play_stat->start_time = ctrl_info->start_time;
        dt_info(TAG, "START TIME:%lld \n", ctrl_info->start_time);
    }

    if (ctrl_info->first_time == -1 && host_state->sys_time_current != -1) {
        ctrl_info->first_time = host_state->sys_time_current;
        dt_info(TAG, "SET FIRST TIME:%lld \n", ctrl_info->first_time);
    }

    if (play_stat->full_time == 0) {
        play_stat->full_time = dtp_ctx->media_info->duration;
    }

    int has_audio = ctrl_info->has_audio;
    int has_video = ctrl_info->has_video;
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

    if (has_audio && has_video) {
        if (audio_discontinue_flag && video_discontinue_flag) { // both discontinue occured
            play_stat->discontinue_point_ms = play_stat->cur_time_ms;
            if (avdiff < AVSYNC_THRESHOLD_MAX) {
                dthost_clear_discontinue_flag(dtp_ctx->host_priv);
            }
            sys_time = pts_video_current;
            if (audio_discontinue_point > 0) {
                start_time = audio_discontinue_point;
            }
        } else if (audio_discontinue_flag) { // only audio discontinue occured
            if (avdiff < AVSYNC_THRESHOLD_MAX) {
                dthost_clear_discontinue_flag(dtp_ctx->host_priv);
            }
            sys_time = pts_video_current;
            if (video_discontinue_point > 0) {
                start_time = video_discontinue_point;
            }
        } else if (video_discontinue_flag) { // only video discontinue occured
            if (avdiff < AVSYNC_THRESHOLD_MAX) {
                dthost_clear_discontinue_flag(dtp_ctx->host_priv);
            }
            sys_time = pts_audio_current;
            if (audio_discontinue_point > 0) {
                start_time = audio_discontinue_point;
            }
        } else {
            sys_time = pts_video_current;
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
    dt_debug(TAG, "apts:%llx audio_discontinue_flag:%d audio_discontinue_point:%llx\n", pts_audio_current, audio_discontinue_flag, audio_discontinue_point);
    dt_debug(TAG, "vpts:%llx video_discontinue_flag:%d video_discontinue_point:%llx\n", pts_video_current, video_discontinue_flag, video_discontinue_point);
    dt_debug(TAG, "sys_time:%llx start_time:%llx\n", (sys_time - play_stat->discontinue_point_ms * DT_PTS_FREQ_MS), start_time);
    dt_debug(TAG, "---------------------------------------------\n");

    return 0;
}

/*
 * handle callback, update status and cur_time to uplevel
 * */
int player_handle_cb(dtplayer_context_t * dtp_ctx)
{
    player_state_t *state = &dtp_ctx->state;
    if (dtp_ctx->update_cb) {
        dtp_ctx->update_cb(dtp_ctx->cookie, state);
    }
    return 0;

}

void player_update_state(dtplayer_context_t * dtp_ctx)
{
    player_state_t *play_stat = &dtp_ctx->state;
    host_state_t host_state;

    /*update host state */
    dthost_get_state(dtp_ctx->host_priv, &host_state);

    /*calc cur time */
    calc_cur_time(dtp_ctx, &host_state);

    /*show info */
    dt_info(TAG, "[%s:%d]alevel:%d vlevel:%d slevel:%d cur_time:%lld(s) %lld(ms) duration:%lld(s) \n", __FUNCTION__, __LINE__, host_state.abuf_level, host_state.vbuf_level, host_state.sbuf_level, play_stat->cur_time, play_stat->cur_time_ms, dtp_ctx->media_info->duration);
}
