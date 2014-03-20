#include "dtplayer_update.h"

#define TAG "PLAYER-UPDATE"

static int calc_cur_time (dtplayer_context_t * dtp_ctx, host_state_t * host_state)
{
    player_ctrl_t *ctrl_info = &dtp_ctx->ctrl_info;
    player_state_t *play_stat = &dtp_ctx->state;

    if (ctrl_info->start_time > 0)
    {
        play_stat->start_time = ctrl_info->start_time;
        dt_info (TAG, "START TIME:%lld \n", ctrl_info->start_time);
    }

    if (ctrl_info->first_time == -1 && host_state->cur_systime != -1)
    {
        ctrl_info->first_time = host_state->cur_systime;
        dt_info (TAG, "SET FIRST TIME:%lld \n", ctrl_info->first_time);
    }
    //int64_t sys_time =(host_state->cur_systime>play_stat->start_time)?(host_state->cur_systime - play_stat->start_time):host_state->cur_systime;
    int64_t sys_time = (host_state->cur_systime > ctrl_info->first_time) ? (host_state->cur_systime - ctrl_info->first_time) : host_state->cur_systime;
    play_stat->cur_time = sys_time / 90000;
    play_stat->cur_time_ms = sys_time / 90;
    return 0;
}

/*
 * handle callback, update status and cur_time to uplevel
 * */
int player_handle_cb (dtplayer_context_t * dtp_ctx)
{
    player_state_t *state = &dtp_ctx->state;
    if (dtp_ctx->update_cb)
        dtp_ctx->update_cb (state);
    return 0;

}

void player_update_state (dtplayer_context_t * dtp_ctx)
{
    player_state_t *play_stat = &dtp_ctx->state;
    host_state_t host_state;

    /*update host state */
    dthost_get_state (dtp_ctx->host_priv, &host_state);

    /*calc cur time */
    calc_cur_time (dtp_ctx, &host_state);

    /*show info */
    dt_info (TAG, "Abuflevel:%d vbuflevel:%d cur_time:%lld(s) %lld(ms) duration:%lld(s) \n ", host_state.abuf_level, host_state.vbuf_level, play_stat->cur_time, play_stat->cur_time_ms, dtp_ctx->media_info->duration);
}
