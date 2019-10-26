#include "unistd.h"

#include "dt_error.h"
#include "dt_setting.h"

#include "dthost.h"
#include "dtport_api.h"
#include "dtaudio_api.h"
#include "dtvideo_api.h"
#include "dtsub_api.h"

#define TAG "HOST-MGT"

//==Part1:PTS Relative
int host_sync_enable(dthost_context_t * hctx)
{
    return hctx->sync_enable;
}

int64_t host_get_apts(dthost_context_t * hctx)
{
    //return dtaudio_external_get_pts(hctx->audio_priv);
    return hctx->pts_audio_current;
}

int64_t host_get_vpts(dthost_context_t * hctx)
{
    //return dtvideo_externel_get_pts(hctx->video_priv);
    return hctx->pts_video_current;
}

int64_t host_get_spts(dthost_context_t * hctx)
{
    //return dtvideo_externel_get_pts(hctx->video_priv);
    return hctx->pts_sub;
}

int64_t host_get_systime(dthost_context_t * hctx)
{
    return hctx->sys_time_current;
}

int64_t host_get_avdiff(dthost_context_t * hctx)
{
    int64_t atime = -1;
    int64_t vtime = -1;
    atime = hctx->pts_audio_current;
    vtime = hctx->pts_video_current;
    hctx->av_diff = (atime - vtime) / DT_PTS_FREQ_MS;
    return hctx->av_diff;
}

int host_update_apts(dthost_context_t * hctx, int64_t apts)
{
    int aout_close = 0;
    int64_t jump = llabs(apts - host_get_apts(hctx));
    if (jump / DT_PTS_FREQ_MS >= DT_SYNC_DISCONTINUE_THRESHOLD) {
        hctx->audio_discontinue_flag = 1;
        hctx->audio_discontinue_point = apts;
        hctx->audio_discontinue_step = jump;
        dt_info(TAG, "apts discontinue,jump:%llx :%llx -> %llx \n", jump,
                host_get_apts(hctx), apts);
    }

    hctx->pts_audio_last = hctx->pts_audio_current;
    hctx->pts_audio_current = apts;
    dt_debug(TAG, "update apts:%llx \n", apts);
    if (!hctx->para.has_video) {
        hctx->sys_time_current = apts;
        if (hctx->sys_time_start == DT_NOPTS_VALUE) {
            hctx->sys_time_start = apts;
        }

        return 0;
    }

    if (host_sync_enable(hctx) == 0) { //sync disable, video will correct systime
        return 0;
    }

    aout_close = host_get_aout_closed(hctx);
    if (aout_close) {
        dt_debug(TAG, "audio output closed. Not update pcr.\n");
        return 0;
    }

    int64_t vpts = host_get_vpts(hctx);
    int64_t sys_time = host_get_systime(hctx);
    int64_t avdiff = llabs(host_get_avdiff(hctx));
    int64_t asdiff = (llabs(apts - sys_time)) /
                     DT_PTS_FREQ_MS;  //apts sys_time diff

    if (sys_time == -1
        || sys_time == DT_NOPTS_VALUE) {       //if systime have not been set,wait
        return 0;
    }
    if (host_sync_enable(hctx) && avdiff > AVSYNC_THRESHOLD_MAX) { //close sync
        if (hctx->sync_mode == DT_SYNC_VIDEO_MASTER)
            dt_info(TAG, "avdiff:%lld ecceed :%d ms, cloase sync \n", avdiff,
                    AVSYNC_THRESHOLD_MAX);
        hctx->sync_mode = DT_SYNC_VIDEO_MASTER;
        return 0;
    }

    if (host_sync_enable(hctx) && hctx->sync_mode == DT_SYNC_VIDEO_MASTER
        && avdiff < AVSYNC_THRESHOLD_MAX) {
        hctx->sync_mode = DT_SYNC_AUDIO_MASTER;
    }

    if (asdiff < AVSYNC_THRESHOLD) {
        return 0;
    }
    if (host_sync_enable(hctx)) {
        dt_info(TAG,
                "[%s:%d] correct sys time apts:%llx vpts:%llx sys_time:%llx AVDIFF:%llx ASDIFF:%llx\n",
                __FUNCTION__, __LINE__, apts, vpts, sys_time, avdiff, asdiff);
        host_reset_systime(hctx, apts);
    }
    return 0;
}

int host_update_vpts(dthost_context_t * hctx, int64_t vpts)
{
    int64_t jump = llabs(vpts - host_get_vpts(hctx));
    if (jump / DT_PTS_FREQ_MS >= DT_SYNC_DISCONTINUE_THRESHOLD) {
        hctx->video_discontinue_flag = 1;
        hctx->video_discontinue_point = vpts;
        hctx->video_discontinue_step = jump;
        dt_info(TAG, "vpts discontinue,jump:%llx :%llx -> %llx \n", jump,
                host_get_vpts(hctx), vpts);
    }

    hctx->pts_video_last = hctx->pts_video_current;
    hctx->pts_video_current = vpts;
    //maybe need to correct sys clock
    //int64_t apts = host_get_apts(hctx);
    int64_t sys_time = host_get_systime(hctx);
    int64_t avdiff = llabs(host_get_avdiff(hctx));
    int64_t vsdiff = llabs(sys_time - vpts);

    if (sys_time == -1 || sys_time == DT_NOPTS_VALUE) {
        return 0;
    }

    //when sync == 0, update systime with vpts
    if (host_sync_enable(hctx) == 0) {
        if (vsdiff > AVSYNC_THRESHOLD) {
            host_reset_systime(hctx, vpts);
        }
        return 0;
    }

    if (host_sync_enable(hctx) && avdiff > AVSYNC_THRESHOLD_MAX) { //close sync
        if (hctx->sync_mode == DT_SYNC_VIDEO_MASTER)
            dt_info(TAG, "avdiff:0x%lld (ms) ecceed :%d ms, disable av sync \n", avdiff,
                    AVSYNC_THRESHOLD_MAX);
        hctx->sync_mode = DT_SYNC_VIDEO_MASTER;
        if (vsdiff > AVSYNC_THRESHOLD) {
            host_reset_systime(hctx, vpts);
        }
        return 0;
    }

    if (hctx->sync_enable && hctx->sync_mode == DT_SYNC_VIDEO_MASTER
        && avdiff < AVSYNC_THRESHOLD_MAX) {
        dt_info(TAG, "[%s:%d]avdiff:%lld(ms) < %d ms, enable av sync \n", __FUNCTION__,
                __LINE__, avdiff, AVSYNC_THRESHOLD_MAX);
        hctx->sync_mode = DT_SYNC_AUDIO_MASTER;
    }
    return 0;
}

int host_update_spts(dthost_context_t * hctx, int64_t spts)
{
    hctx->pts_sub = spts;
    dt_debug(TAG, "update spts:%llx \n", spts);
    return 0;
}

int host_reset_systime(dthost_context_t * hctx, int64_t sys_time)
{
    // First assignment
    dt_info(TAG, "[%s:%d] apts:%llx vpts:%llx sys_time:%llx->%llx\n", __FUNCTION__,
            __LINE__, host_get_apts(hctx), host_get_vpts(hctx), host_get_systime(hctx),
            sys_time);
    hctx->sys_time_last = hctx->sys_time_current;
    hctx->sys_time_start = sys_time;
    hctx->sys_time_current = sys_time;
    hctx->sys_time_start_time = dt_gettime();
    return 0;
}

int host_update_systime(dthost_context_t * hctx, int64_t sys_time)
{
    if (PTS_INVALID(hctx->sys_time_current)) {
        // First assignment
        hctx->sys_time_current = hctx->sys_time_first = sys_time;
        hctx->sys_time_start = hctx->sys_time_last = sys_time;
        hctx->sys_time_start_time = dt_gettime();
        return 0;
    }

    int64_t time_diff = dt_gettime() - hctx->sys_time_start_time;
    hctx->sys_time_last = hctx->sys_time_current;
    hctx->sys_time_current = hctx->sys_time_start + time_diff * 9 / 100;
    return 0;
}

int host_clear_discontinue_flag(dthost_context_t * hctx)
{
    hctx->audio_discontinue_flag = 0;
    hctx->video_discontinue_flag = 0;
    dt_info(TAG, "[%s:%d]Clear av discontinue flag \n", __FUNCTION__, __LINE__);
    return 0;
}

int64_t host_get_current_time(dthost_context_t * hctx)
{
    int64_t ctime = -1;
    int64_t atime = -1;
    int64_t vtime = -1;

    if (hctx->para.has_audio) {
        atime = host_get_apts(hctx);
    }
    if (hctx->para.has_video) {
        vtime = host_get_vpts(hctx);
    }
    ctime = hctx->sys_time_current;
    dt_debug(TAG,
             "ctime:%llx atime:%llx vtime:%llx sys_time:%llx av_diff:%llx sync mode:%x\n",
             ctime, atime, vtime, hctx->sys_time_current, hctx->av_diff, hctx->av_sync);
    return ctime;
}

//==Part2:Control
int host_start(dthost_context_t * hctx)
{
    int ret;
    int has_audio, has_video, has_sub;
    has_audio = hctx->para.has_audio;
    has_video = hctx->para.has_video;
    has_sub = hctx->para.has_sub;

    int drop_enable = dtp_setting.host_drop;
    if (has_audio == 0 || has_video == 0) {
        drop_enable = 0;
    }
    if (drop_enable == 0) {
        dt_info(TAG, "[%s:%d] disable drop.\n", __FUNCTION__, __LINE__);
        hctx->drop_done = 1;
    }

    if (has_audio) {
        ret = dtaudio_start(hctx->audio_priv);
        if (ret < 0) {
            dt_error(TAG, "[%s:%d] dtaudio start failed \n", __FUNCTION__, __LINE__);
            return -1;
        }
        dt_info(TAG, "[%s:%d]audio start ok \n", __FUNCTION__, __LINE__);
    }
    if (has_video) {
        ret = dtvideo_start(hctx->video_priv);
        if (ret < 0) {
            dt_error(TAG, "[%s:%d] dtvideo start failed \n", __FUNCTION__, __LINE__);
            return -1;
        }
        dt_info(TAG, "[%s:%d]video start ok\n", __FUNCTION__, __LINE__);
    }
    if (has_sub) {
        ret = dtsub_start(hctx->sub_priv);
        if (ret < 0) {
            dt_error(TAG, "[%s:%d] dtvideo start failed \n", __FUNCTION__, __LINE__);
            return -1;
        }
        dt_info(TAG, "[%s:%d]sub start ok\n", __FUNCTION__, __LINE__);
    }

    return 0;
}

int host_pause(dthost_context_t * hctx)
{
    int ret;
    int has_audio, has_video, has_sub;
    has_audio = hctx->para.has_audio;
    has_video = hctx->para.has_video;
    has_sub = hctx->para.has_sub;

    if (has_audio) {
        ret = dtaudio_pause(hctx->audio_priv);
        if (ret < 0) {
            dt_error(TAG, "[%s:%d] dtaudio external pause failed \n", __FUNCTION__,
                     __LINE__);
        }
    }
    if (has_video) {
        ret = dtvideo_pause(hctx->video_priv);
    }
    if (has_sub)
        ;                       //ret = dtsub_pause();
    return 0;
}

int host_resume(dthost_context_t * hctx)
{
    int ret;
    int has_audio, has_video, has_sub;
    has_audio = hctx->para.has_audio;
    has_video = hctx->para.has_video;
    has_sub = hctx->para.has_sub;

    // startup pts
    if (has_video) {
        host_reset_systime(hctx, host_get_vpts(hctx));
    } else if (has_audio) {
        host_reset_systime(hctx, host_get_apts(hctx));
    }

    if (has_audio) {
        ret = dtaudio_resume(hctx->audio_priv);
        if (ret < 0) {
            dt_error(TAG, "[%s:%d] dtaudio external pause failed \n", __FUNCTION__,
                     __LINE__);
        }
    }

    if (has_video) {
        ret = dtvideo_resume(hctx->video_priv);
    }

    if (has_sub) ;

    return 0;
}

int host_stop(dthost_context_t * hctx)
{
    int ret;
    int has_audio, has_video, has_sub;
    has_audio = hctx->para.has_audio;
    has_video = hctx->para.has_video;
    has_sub = hctx->para.has_sub;

    /*first stop audio module */
    if (has_audio) {
        ret = dtaudio_stop(hctx->audio_priv);
        if (ret < 0) {
            dt_error(TAG, "[%s:%d] dtaudio stop failed \n", __FUNCTION__, __LINE__);
        }
    }
    /*stop video mudule */
    if (has_video) {
        ret = dtvideo_stop(hctx->video_priv);
        if (ret < 0) {
            dt_error(TAG, "[%s:%d] dtvideo stop failed \n", __FUNCTION__, __LINE__);
        }
    }
    if (has_sub) {
        ret = dtsub_stop(hctx->sub_priv);
        if (ret < 0) {
            dt_error(TAG, "[%s:%d] dtsub stop failed \n", __FUNCTION__, __LINE__);
        }
    }
    /*stop dtport module at last */
    ret = dtport_stop(hctx->port_priv);
    if (ret < 0) {
        dt_error(TAG, "[%s:%d] dtport stop failed \n", __FUNCTION__, __LINE__);
    }
    return 0;
}

/*dthost code*/
int host_init(dthost_context_t * hctx)
{
    int ret;
    dthost_para_t *host_para = &hctx->para;
    /*
     *set sync mode
     * read from property file, get sync mode setted by user
     * if user not set ,get by default
     */
    if (host_para->has_audio && host_para->has_video) {
        hctx->av_sync = DT_SYNC_AUDIO_MASTER;
    } else {
        hctx->av_sync = DT_SYNC_VIDEO_MASTER;
    }
    hctx->sync_enable = (host_para->has_audio && host_para->has_video)
                        && host_para->sync_enable;
    hctx->av_diff = 0;

    hctx->sys_time_start = DT_NOPTS_VALUE;
    hctx->sys_time_first = DT_NOPTS_VALUE;
    hctx->sys_time_current = DT_NOPTS_VALUE;
    hctx->sys_time_last  = DT_NOPTS_VALUE;
    hctx->sys_time_start_time = -1;

    hctx->pts_audio_first = hctx->pts_audio_last = hctx->pts_audio_current =
                                DT_NOPTS_VALUE;
    hctx->audio_discontinue_flag = 0;
    hctx->audio_discontinue_point = -1;
    hctx->audio_discontinue_step = -1;
    hctx->pts_video_first = hctx->pts_video_last = hctx->pts_video_current =
                                DT_NOPTS_VALUE;
    hctx->video_discontinue_flag = 0;
    hctx->video_discontinue_point = -1;
    hctx->video_discontinue_step = -1;

    /*init port */
    dtport_para_t port_para;
    port_para.has_audio = host_para->has_audio;
    port_para.has_subtitle = host_para->has_sub;
    port_para.has_video = host_para->has_video;

    ret = dtport_init(&hctx->port_priv, &port_para, hctx);
    if (ret < 0) {
        goto ERR1;
    }
    dt_info(TAG, "[%s:%d] dtport init success \n", __FUNCTION__, __LINE__);

    /*init video */
    if (host_para->has_video) {
        dtvideo_para_t video_para;
        video_para.vfmt = host_para->video_format;
        video_para.d_width = host_para->video_dest_width;
        video_para.d_height = host_para->video_dest_height;
        video_para.s_width = host_para->video_src_width;
        video_para.s_height = host_para->video_src_height;
        video_para.s_pixfmt = host_para->video_src_pixfmt;
        video_para.d_pixfmt = host_para->video_dest_pixfmt;

        video_para.rate = host_para->video_rate;
        video_para.ratio = host_para->video_ratio;
        video_para.fps = host_para->video_fps;
        video_para.num = host_para->video_num;
        video_para.den = host_para->video_den;

        video_para.extradata_size = host_para->video_extra_size;
        if (video_para.extradata_size) {
            memset(video_para.extradata, 0, VIDEO_EXTRADATA_SIZE);
            memcpy(video_para.extradata, host_para->video_extra_data,
                   host_para->video_extra_size);
        }
        video_para.video_filter = host_para->video_filter;
        video_para.video_output = host_para->video_output;
        video_para.device = host_para->vo_device;
        video_para.avctx_priv = host_para->vctx_priv;
        video_para.flag = host_para->vflag;
        ret = dtvideo_init(&hctx->video_priv, &video_para, hctx);
        if (ret < 0) {
            goto ERR3;
        }
        dt_info(TAG, "[%s:%d] dtvideo init success \n", __FUNCTION__, __LINE__);
        if (!hctx->video_priv) {
            dt_error(TAG, "[%s:%d] dtvideo init failed video_priv ==NULL \n", __FUNCTION__,
                     __LINE__);
            goto ERR3;
        }
    }

    /*init audio */
    if (host_para->has_audio) {
        dtaudio_para_t audio_para;
        audio_para.service_mgt = host_para->service_mgt;
        audio_para.afmt = host_para->audio_format;
        audio_para.bps = host_para->audio_sample_fmt;
        audio_para.channels = host_para->audio_channel;
        audio_para.dst_channels = host_para->audio_dst_channels;
        audio_para.dst_samplerate = host_para->audio_dst_samplerate;
        audio_para.data_width = host_para->audio_sample_fmt;
        audio_para.samplerate = host_para->audio_samplerate;
        audio_para.num = host_para->audio_num;
        audio_para.den = host_para->audio_den;

        audio_para.extradata_size = host_para->audio_extra_size;
        if (host_para->audio_extra_size) {
            memset(audio_para.extradata, 0, AUDIO_EXTRADATA_SIZE);
            memcpy(audio_para.extradata, host_para->audio_extra_data,
                   host_para->audio_extra_size);
        }
        audio_para.audio_filter = host_para->audio_filter;
        audio_para.audio_output = host_para->audio_output;
        audio_para.device = host_para->ao_device;
        audio_para.avctx_priv = host_para->actx_priv;

        ret = dtaudio_init(&hctx->audio_priv, &audio_para, hctx);
        if (ret < 0) {
            goto ERR2;
        }
        dt_info(TAG, "[%s:%d]dtaudio init success \n", __FUNCTION__, __LINE__);
        if (!hctx->audio_priv) {
            dt_error(TAG, "[%s:%d] dtaudio init failed audio_priv ==NULL \n", __FUNCTION__,
                     __LINE__);
            return -1;
        }
    }
    /*init sub */
    if (host_para->has_sub) {
        dtsub_para_t sub_para;
        sub_para.sfmt = host_para->sub_format;
        sub_para.width = host_para->sub_width;
        sub_para.height = host_para->sub_height;
        sub_para.sub_output = -1;
        sub_para.device = host_para->so_device;
        sub_para.avctx_priv = host_para->sctx_priv;
        ret = dtsub_init(&hctx->sub_priv, &sub_para, hctx);
        if (ret < 0) {
            dt_error(TAG, "ERR: dtsub init failed \n");
            host_para->has_sub = 0;
        } else {
            dt_info(TAG, "[%s:%d]dtsub init success \n", __FUNCTION__, __LINE__);
        }
    }

    // setup callback
    hctx->notify = host_para->notify;
    hctx->parent = host_para->parent;
    return 0;
ERR1:
    return -1;
ERR2:
    dtport_stop(hctx->port_priv);
    return -2;
ERR3:
    dtport_stop(hctx->port_priv);
    if (host_para->has_audio) {
        dtaudio_stop(hctx->audio_priv);
    }
    return -3;
}

int host_video_resize(dthost_context_t * hctx, int w, int h)
{
    int has_video = hctx->para.has_video;

    if (! has_video) {
        dt_warning(TAG, "RESIZE FAILED ,HAVE NO VIDEO \n");
        return -1;
    }

    host_pause(hctx);
    int ret = dtvideo_resize(hctx->video_priv, w, h);
    if (ret == 0) {
        hctx->para.video_dest_width = w;
        hctx->para.video_dest_height = h;
    }
    dtaudio_resume(hctx->audio_priv);

    return ret;
}

//==Part3:Data IO Relative

int host_write_frame(dthost_context_t * hctx, dt_av_pkt_t * frame, int type)
{

    dthost_para_t *host_para = &hctx->para;
    if (host_para->has_sub == 0 && type == DTP_MEDIA_TYPE_SUBTITLE) {
        return DTERROR_NONE;
    }

    return dtport_write_frame(hctx->port_priv, frame, type);
}

int host_read_frame(dthost_context_t * hctx, dt_av_pkt_t **frame, int type)
{
    if (hctx == NULL) {
        dt_error(TAG, "dthost_context is NULL\n");
        return -1;
    }
    if (NULL == hctx->port_priv) {
        dt_error(TAG, "dtport is NULL\n");
        return -1;
    }
    dt_debug(TAG, "[%s:%d]READ FRAME BEGIN \n", __FUNCTION__, __LINE__);
    int ret = dtport_read_frame(hctx->port_priv, frame, type);
    dt_debug(TAG, "[%s:%d]READ FRAME BEGIN \n", __FUNCTION__, __LINE__);
    return ret;
}

//==Part4:Status
int host_get_state(dthost_context_t * hctx, host_state_t * state)
{
    int has_audio = hctx->para.has_audio;
    int has_video = hctx->para.has_video;
    int has_sub = hctx->para.has_sub;
    buf_state_t buf_state;
    dec_state_t dec_state;
    if (has_audio) {
        dtport_get_state(hctx->port_priv, &buf_state, DTP_MEDIA_TYPE_AUDIO);
        dtaudio_get_state(hctx->audio_priv, &dec_state);
        state->abuf_level = buf_state.data_len;
        state->adec_err_cnt = dec_state.adec_error_count;
        state->adec_last_ms = dec_state.adec_last_ms;
    } else {
        state->abuf_level = -1;
        state->adec_err_cnt = -1;
    }

    if (has_video) {
        dtport_get_state(hctx->port_priv, &buf_state, DTP_MEDIA_TYPE_VIDEO);
        dtvideo_get_state(hctx->video_priv, &dec_state);
        state->vbuf_level = buf_state.data_len;
        state->vdec_err_cnt = dec_state.vdec_error_count;
        state->vdec_last_ms = dec_state.vdec_last_ms;
        state->vdec_type = dec_state.vdec_type;
    } else {
        state->vbuf_level = -1;
        state->vdec_err_cnt = -1;
    }

    if (has_sub) {
        dtport_get_state(hctx->port_priv, &buf_state, DTP_MEDIA_TYPE_SUBTITLE);
        state->sbuf_level = buf_state.data_len;
        state->sdec_err_cnt = 0;
    } else {
        state->sbuf_level = -1;
        state->sdec_err_cnt = -1;
    }

    // Update pts
    state->sys_time_start = hctx->sys_time_start;
    state->sys_time_current = hctx->sys_time_current;
    state->pts_audio_first = hctx->pts_audio_first;
    state->pts_audio_current = hctx->pts_audio_current;
    state->audio_discontinue_flag = hctx->audio_discontinue_flag;
    state->audio_discontinue_point = hctx->audio_discontinue_point;
    state->pts_video_first = hctx->pts_video_first;
    state->pts_video_current = hctx->pts_video_current;
    state->video_discontinue_flag = hctx->video_discontinue_flag;
    state->video_discontinue_point = hctx->video_discontinue_point;

    return 0;
}

int host_get_aout_closed(dthost_context_t *hctx)
{
    int aout_close = 0;
    buf_state_t buf_state;
    if (hctx->para.has_audio) {
        aout_close = dtaudio_get_out_closed(hctx->audio_priv);
        dtport_get_state(hctx->port_priv, &buf_state, DTP_MEDIA_TYPE_AUDIO);
        if (buf_state.data_len > 0) {
            aout_close = 0;
        }
    } else {
        aout_close = 1;
    }

    return aout_close;
}

int host_get_vout_closed(dthost_context_t *hctx)
{
    int vout_close = 0;
    buf_state_t buf_state;

    if (hctx->para.has_video) {
        vout_close = dtvideo_get_out_closed(hctx->video_priv);
        dtport_get_state(hctx->port_priv, &buf_state, DTP_MEDIA_TYPE_VIDEO);
        if (buf_state.data_len > 0) {
            vout_close = 0;
        }
    } else {
        vout_close = 1;
    }

    return vout_close;
}

int host_get_out_closed(dthost_context_t * hctx)
{
    int aout_close = host_get_aout_closed(hctx);
    int vout_close = host_get_vout_closed(hctx);
    dt_info(TAG, "aout_close:%d vout_close:%d \n", aout_close, vout_close);
    return (aout_close && vout_close);
}
