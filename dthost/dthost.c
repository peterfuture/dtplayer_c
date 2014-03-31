#include "dthost.h"
#include "dtport_api.h"
#include "dtaudio_api.h"
#include "dtvideo_api.h"

#include "unistd.h"

#define TAG "host-MGT"
#define AVSYNC_THRESHOLD 100    //ms
#define AVSYNC_THRESHOLD_MAX  3*1000 //ms
#define AVSYNC_DROP_THRESHOLD  10*1000 //ms

//==Part1:PTS Relative
int host_sync_enable (dthost_context_t * hctx)
{
    return hctx->sync_enable && (hctx->sync_mode == DT_SYNC_AUDIO_MASTER);
}

int64_t host_get_apts (dthost_context_t * hctx)
{
    //return dtaudio_external_get_pts(hctx->audio_priv);
    return hctx->pts_audio;
}

int64_t host_get_vpts (dthost_context_t * hctx)
{
    //return dtvideo_externel_get_pts(hctx->video_priv);
    return hctx->pts_video;
}

int64_t host_get_systime (dthost_context_t * hctx)
{
    return hctx->sys_time;
}

int64_t host_get_avdiff (dthost_context_t * hctx)
{
    int64_t atime = -1;
    int64_t vtime = -1;
    atime = hctx->pts_audio;
    vtime = hctx->pts_video;
    hctx->av_diff = (atime - vtime) / 90; //ms
    return hctx->av_diff;
}

int host_update_apts (dthost_context_t * hctx, int64_t apts)
{
    hctx->pts_audio = apts;
    dt_debug (TAG, "update apts:%llx \n", apts);
    if (!hctx->para.has_video)
    {
        hctx->sys_time = apts;
        return 0;
    }
    if (!hctx->para.sync_enable) //sync disable, video will correct systime
        return 0;
    //maybe need to correct sys clock
    int64_t vpts = host_get_vpts (hctx);
    int64_t sys_time = host_get_systime (hctx);
    int64_t avdiff = host_get_avdiff (hctx);
    int64_t asdiff = (llabs) (apts - hctx->sys_time) / 90; //apts sys_time diff

    if (sys_time == -1)         //if systime have not been set,wait
        return 0;

    if (host_sync_enable (hctx) && avdiff / 90 > AVSYNC_THRESHOLD_MAX) //close sync
    {
        dt_info (TAG, "avdiff:%lld ecceed :%d ms, cloase sync \n", avdiff / 90, AVSYNC_THRESHOLD_MAX);
        hctx->sync_mode = DT_SYNC_VIDEO_MASTER;
        return 0;
    }

    if (hctx->sync_enable && hctx->sync_mode == DT_SYNC_VIDEO_MASTER && avdiff / 90 < AVSYNC_THRESHOLD_MAX) // enable sync again
        hctx->sync_mode = DT_SYNC_AUDIO_MASTER;

    if (avdiff < AVSYNC_THRESHOLD)
        return 0;
    if (host_sync_enable (hctx))
    {
        dt_info (TAG, "[%s:%d] correct sys time apts:%lld vpts:%lld sys_time:%lld AVDIFF:%d ASDIFF:%d\n", __FUNCTION__, __LINE__, apts, vpts, sys_time, avdiff, asdiff);
        hctx->sys_time = apts;
    }
    return 0;
}

int host_update_vpts (dthost_context_t * hctx, int64_t vpts)
{
    hctx->pts_video = vpts;
    dt_debug (TAG, "update vpts:%llx \n", vpts);
    //maybe need to correct sys clock
    //int64_t apts = host_get_apts(hctx);
    int64_t sys_time = host_get_systime (hctx);
    int64_t avdiff = host_get_avdiff (hctx);
    //int64_t vsdiff = llabs(vpts - hctx->sys_time)/90; //vpts sys_time diff

    if (sys_time == -1)
        return 0;
    if (!hctx->sync_enable && avdiff / 90 > AVSYNC_THRESHOLD)
    {
        dt_info (TAG, "[%s:%d] sync disable or avdiff too much, update systime with vpts, sys:%lld vpts:%lld\n", __FUNCTION__, __LINE__, sys_time, vpts);
        hctx->sys_time = vpts;
        return 0;
    }

    if (host_sync_enable (hctx) && avdiff / 90 > AVSYNC_THRESHOLD_MAX) //close sync
    {
        dt_info (TAG, "avdiff:%d ecceed :%d ms, cloase sync \n", avdiff / 90, AVSYNC_THRESHOLD_MAX);
        hctx->sync_mode = DT_SYNC_VIDEO_MASTER;
        return 0;
    }

    if (hctx->sync_enable && hctx->sync_mode == DT_SYNC_VIDEO_MASTER && avdiff / 90 < AVSYNC_THRESHOLD_MAX)
        hctx->sync_mode = DT_SYNC_AUDIO_MASTER;

    return 0;
}

int host_update_systime (dthost_context_t * hctx, int64_t sys_time)
{
    hctx->sys_time = sys_time;
    return 0;
}

int64_t host_get_current_time (dthost_context_t * hctx)
{
    int64_t ctime = -1;
    int64_t atime = -1;
    int64_t vtime = -1;

    if (hctx->para.has_audio)
        atime = hctx->pts_audio;
    if (hctx->para.has_video)
        vtime = hctx->pts_video;
    ctime = hctx->sys_time;
    dt_debug (TAG, "ctime:%llx atime:%llx vtime:%llx sys_time:%llx av_diff:%x sync mode:%x\n", ctime, atime, vtime, hctx->sys_time, hctx->av_diff, hctx->av_sync);
    return ctime;
}

//==Part2:Control
int host_start (dthost_context_t * hctx)
{
    int ret;
    int has_audio, has_video, has_sub;
    has_audio = hctx->para.has_audio;
    has_video = hctx->para.has_video;
    has_sub = hctx->para.has_sub;

    /*check start condition */
    int audio_start_flag = 0;
    int video_start_flag = 0;
    int64_t first_apts = -1;
    int64_t first_vpts = -1;
    dt_info (TAG, "check start condition has_audio:%d has_video:%d has_sub:%d\n", has_audio, has_video, has_sub);
    int print_cnt = 100;
    do
    {
        if (!has_audio)
            audio_start_flag = 1;
        else
            audio_start_flag = !((first_apts = dtaudio_get_first_pts (hctx->audio_priv)) == -1);
        if (!has_video)
            video_start_flag = 1;
        else
            video_start_flag = !((first_vpts = dtvideo_get_first_pts (hctx->video_priv)) == -1);
        if (audio_start_flag && video_start_flag)
            break;
        usleep (1000);
        if(print_cnt-- == 0)
        {
            dt_info (TAG, "audio:%d video:%d \n", audio_start_flag, video_start_flag);
            print_cnt = 100;
        }
    }
    while (1);

    dt_info (TAG, "first apts:%lld first vpts:%lld \n", first_apts, first_vpts);
    hctx->pts_audio = first_apts;
    hctx->pts_video = first_vpts;

    int drop_flag = 0;
    int av_diff_ms = abs (hctx->pts_video - hctx->pts_audio) / 90;
    if (av_diff_ms > 10 * 1000)
    {
        dt_info (TAG, "FIRST AV DIFF EXCEED 10S,DO NOT DROP\n");
        hctx->sync_mode = DT_SYNC_VIDEO_MASTER;
    }
    drop_flag = (av_diff_ms > 100 && av_diff_ms < AVSYNC_DROP_THRESHOLD); // exceed 100ms
    if (!host_sync_enable (hctx))
        drop_flag = 0;
    if (drop_flag)
    {
        if (hctx->pts_audio > hctx->pts_video)
            dtvideo_drop (hctx->video_priv, hctx->pts_audio);
        else
            dtaudio_drop (hctx->audio_priv, hctx->pts_video);
    }
    dt_info (TAG, "apts:%lld vpts:%lld \n", hctx->pts_audio, hctx->pts_video);

    if (has_audio)
    {
        ret = dtaudio_start (hctx->audio_priv);
        if (ret < 0)
        {
            dt_error (TAG, "[%s:%d] dtaudio start failed \n", __FUNCTION__, __LINE__);
            return -1;
        }
        dt_info (TAG, "[%s:%d] dtaudio start ok \n", __FUNCTION__, __LINE__);
    }
    if (has_video)
    {
        ret = dtvideo_start (hctx->video_priv);
        if (ret < 0)
        {
            dt_error (TAG, "[%s:%d] dtvideo start failed \n", __FUNCTION__, __LINE__);
            return -1;
        }
        dt_info (TAG, "[%s:%d]video start ok\n", __FUNCTION__, __LINE__);
    }

    return 0;
}

int host_pause (dthost_context_t * hctx)
{
    int ret;
    int has_audio, has_video, has_sub;
    has_audio = hctx->para.has_audio;
    has_video = hctx->para.has_video;
    has_sub = hctx->para.has_sub;

    if (has_audio)
    {
        ret = dtaudio_pause (hctx->audio_priv);
        if (ret < 0)
            dt_error (TAG, "[%s:%d] dtaudio external pause failed \n", __FUNCTION__, __LINE__);
    }
    if (has_video)
        ret = dtvideo_pause (hctx->video_priv);
    if (has_sub)
        ;                       //ret = dtsub_pause();
    return 0;
}

int host_resume (dthost_context_t * hctx)
{
    int ret;
    int has_audio, has_video, has_sub;
    has_audio = hctx->para.has_audio;
    has_video = hctx->para.has_video;
    has_sub = hctx->para.has_sub;

    if (has_audio)
    {
        ret = dtaudio_resume (hctx->audio_priv);
        if (ret < 0)
            dt_error (TAG, "[%s:%d] dtaudio external pause failed \n", __FUNCTION__, __LINE__);
    }
    if (has_video)
        ret = dtvideo_resume (hctx->video_priv);
    if (has_sub)
        ;

    return 0;
}

int host_stop (dthost_context_t * hctx)
{
    int ret;
    int has_audio, has_video, has_sub;
    has_audio = hctx->para.has_audio;
    has_video = hctx->para.has_video;
    has_sub = hctx->para.has_sub;

    /*first stop audio module */
    if (has_audio)
    {
        ret = dtaudio_stop (hctx->audio_priv);
        if (ret < 0)
            dt_error (TAG, "[%s:%d] dtaudio stop failed \n", __FUNCTION__, __LINE__);
    }
    /*stop video mudule */
    if (has_video)
    {
        ret = dtvideo_stop (hctx->video_priv);
        if (ret < 0)
            dt_error (TAG "[%s:%d] dtvideo stop failed \n", __FUNCTION__, __LINE__);
    }
    if (has_sub)
        ;
    /*stop dtport module at last */
    ret = dtport_stop (hctx->port_priv);
    if (ret < 0)
        dt_error (TAG, "[%s:%d] dtport stop failed \n", __FUNCTION__, __LINE__);
    return 0;
}

/*dthost code*/
int host_init (dthost_context_t * hctx)
{
    int ret;
    dthost_para_t *host_para = &hctx->para;
    /*
     *set sync mode
     * read from property file, get sync mode setted by user
     * if user not set ,get by default
     */
    if (host_para->has_audio && host_para->has_video)
        hctx->av_sync = DT_SYNC_AUDIO_MASTER;
    else
        hctx->av_sync = DT_SYNC_VIDEO_MASTER;
    hctx->sync_enable = host_para->sync_enable;
    hctx->av_diff = 0;
    hctx->pts_audio = hctx->pts_video = hctx->sys_time = -1;

    /*init port */
    dtport_para_t port_para;
    port_para.has_audio = host_para->has_audio;
    port_para.has_subtitle = host_para->has_sub;
    port_para.has_video = host_para->has_video;

    ret = dtport_init (&hctx->port_priv, &port_para, hctx);
    if (ret < 0)
        goto ERR1;
    dt_info (TAG, "[%s:%d] dtport init success \n", __FUNCTION__, __LINE__);

    /*init video */
    if (host_para->has_video)
    {
        dtvideo_para_t video_para;
        video_para.vfmt = host_para->video_format;
        video_para.d_width = host_para->video_dest_width;
        video_para.d_height = host_para->video_dest_height;
        video_para.s_width = host_para->video_src_width;
        video_para.s_height = host_para->video_src_height;
        video_para.s_pixfmt = host_para->video_src_pixfmt;
        video_para.d_pixfmt = DTAV_PIX_FMT_YUV420P;
        video_para.rate = host_para->video_rate;
        video_para.ratio = host_para->video_ratio;
        video_para.fps = host_para->video_fps;
        video_para.num = host_para->video_num;
        video_para.den = host_para->video_den;

        video_para.extradata_size = host_para->video_extra_size;
        if (video_para.extradata_size)
        {
            memset (video_para.extradata, 0, VIDEO_EXTRADATA_SIZE);
            memcpy (video_para.extradata, host_para->video_extra_data, host_para->video_extra_size);
        }
        video_para.video_filter = host_para->video_filter;
        video_para.video_output = host_para->video_output;
        video_para.avctx_priv = host_para->vctx_priv;
        ret = dtvideo_init (&hctx->video_priv, &video_para, hctx);
        if (ret < 0)
            goto ERR3;
        dt_info (TAG, "[%s:%d] dtvideo init success \n", __FUNCTION__, __LINE__);
        if (!hctx->video_priv)
        {
            dt_error (TAG, "[%s:%d] dtvideo init failed video_priv ==NULL \n", __FUNCTION__, __LINE__);
            goto ERR3;
        }
    }

    /*init audio */
    if (host_para->has_audio)
    {
        dtaudio_para_t audio_para;
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
        if (host_para->audio_extra_size)
        {
            memset (audio_para.extradata, 0, AUDIO_EXTRADATA_SIZE);
            memcpy (audio_para.extradata, host_para->audio_extra_data, host_para->audio_extra_size);
        }
        audio_para.audio_filter = host_para->audio_filter;
        audio_para.audio_output = host_para->audio_output;
        audio_para.avctx_priv = host_para->actx_priv;

        ret = dtaudio_init (&hctx->audio_priv, &audio_para, hctx);
        if (ret < 0)
            goto ERR2;
        dt_info (TAG, "[%s:%d]dtaudio init success \n", __FUNCTION__, __LINE__);
        if (!hctx->audio_priv)
        {
            dt_error (TAG, "[%s:%d] dtaudio init failed audio_priv ==NULL \n", __FUNCTION__, __LINE__);
            return -1;
        }
    }
    /*init sub */
    return 0;
  ERR1:
    return -1;
  ERR2:
    dtport_stop (hctx->port_priv);
    return -2;
  ERR3:
    dtport_stop (hctx->port_priv);
    if (host_para->has_audio)
    {
        dtaudio_stop (hctx->audio_priv);
    }
    return -3;
}

//==Part3:Data IO Relative

int host_write_frame (dthost_context_t * hctx, dt_av_frame_t * frame, int type)
{
    return dtport_write_frame (hctx->port_priv, frame, type);
}

int host_read_frame (dthost_context_t * hctx, dt_av_frame_t * frame, int type)
{
    if (hctx == NULL)
    {
        dt_error (TAG, "dthost_context is NULL\n");
        return -1;
    }
    if (NULL == hctx->port_priv)
    {
        dt_error (TAG, "dtport is NULL\n");
        return -1;
    }
    return dtport_read_frame (hctx->port_priv, frame, type);
}

//==Part4:Status
int host_get_state (dthost_context_t * hctx, host_state_t * state)
{
    int has_audio = hctx->para.has_audio;
    int has_video = hctx->para.has_video;
    buf_state_t buf_state;
    dec_state_t dec_state;
    if (has_audio)
    {
        dtport_get_state (hctx->port_priv, &buf_state, DT_TYPE_AUDIO);
        dtaudio_get_state (hctx->audio_priv, &dec_state);
        state->abuf_level = buf_state.data_len;
        state->adec_err_cnt = dec_state.adec_error_count;
        state->cur_apts = hctx->pts_audio;
    }
    else
    {
        state->abuf_level = -1;
        state->cur_apts = -1;
        state->adec_err_cnt = -1;
    }

    if (has_video)
    {
        dtport_get_state (hctx->port_priv, &buf_state, DT_TYPE_VIDEO);
        dtvideo_get_state (hctx->video_priv, &dec_state);
        state->vbuf_level = buf_state.data_len;
        state->vdec_err_cnt = dec_state.vdec_error_count;
        state->cur_vpts = hctx->pts_video;
    }
    else
    {
        state->vbuf_level = -1;
        state->cur_vpts = -1;
        state->vdec_err_cnt = -1;
    }
    dt_debug (TAG, "[%s:%d] apts:%lld vpts:%lld systime:%lld tsync_enable:%d sync_mode:%d \n", __FUNCTION__, __LINE__, hctx->pts_audio, hctx->pts_video, hctx->sys_time, hctx->sync_enable, hctx->sync_mode);
    state->cur_systime = hctx->sys_time;
    return 0;
}

int host_get_out_closed (dthost_context_t * hctx)
{
    int aout_close = 0;
    int vout_close = 0;
    if (hctx->para.has_audio)
        aout_close = dtaudio_get_out_closed (hctx->audio_priv);
    else
        aout_close = 1;
    if (hctx->para.has_video)
        vout_close = dtvideo_get_out_closed (hctx->video_priv);
    else
        vout_close = 1;
    if (aout_close && vout_close)
        dt_info (TAG, "AV OUT CLOSED \n");
    return (aout_close && vout_close);
}
