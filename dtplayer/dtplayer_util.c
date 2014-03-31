#include "dtplayer_util.h"

#define TAG "PLAYER-UTIL"

int player_host_init (dtplayer_context_t * dtp_ctx)
{
    int ret;
    char value[512];
    
    dthost_para_t *host_para = &dtp_ctx->host_para;
    player_ctrl_t *pctrl = &dtp_ctx->ctrl_info;
    memset (host_para, 0, sizeof (dthost_para_t));
    dt_media_info_t *media_info = dtp_ctx->media_info;

    /* init dthost param */
    host_para->has_audio = pctrl->has_audio;
    host_para->has_video = pctrl->has_video;
    host_para->has_sub = pctrl->has_sub;
    host_para->sync_enable = pctrl->sync_enable;

    //----audio ----
    dt_info (TAG, "[%s:%d] has video:%d has audio:%d has sub:%d \n", __FUNCTION__, __LINE__, host_para->has_video, host_para->has_audio, host_para->has_sub);
    if (pctrl->has_audio)
    {
        if (pctrl->cur_ast_index >= media_info->ast_num)
        {
            dt_warning (TAG, "audio index set exceed max audio num ,reset to 0 \n");
            pctrl->cur_ast_index = 0;
        }
        astream_info_t *astream = media_info->astreams[pctrl->cur_ast_index];
        host_para->audio_format = astream->format;
        host_para->audio_channel = astream->channels;
        host_para->audio_samplerate = astream->sample_rate;
        int downmix = 0;
        if(GetEnv("AUDIO","audio.downmix",value) > 0)
        {
            downmix = atoi(value);
            dt_info(TAG,"downmix:%d \n",downmix);
        }
        else
            dt_info(TAG,"downmix:not found \n");

        if(downmix)
            host_para->audio_dst_channels = (astream->channels>2)?2:astream->channels;
        else    
            host_para->audio_dst_channels = astream->channels;
        host_para->audio_dst_samplerate = astream->sample_rate;
        host_para->audio_bitrate = astream->bit_rate;
        host_para->audio_sample_fmt = astream->bps;

        host_para->audio_num = astream->time_base.num;
        host_para->audio_den = astream->time_base.den;

        host_para->audio_extra_size = astream->extradata_size;
        host_para->actx_priv = astream->codec_priv; //AVCodecContext
        dt_info (TAG, "[%s:%d] audio extra_size:%d time_base.num:%d den:%d\n", __FUNCTION__, __LINE__, host_para->audio_extra_size, host_para->audio_num, host_para->audio_den);
        if (host_para->audio_extra_size > 0)
        {
            memset (host_para->audio_extra_data, 0, host_para->audio_extra_size);
            memcpy ((unsigned char *) host_para->audio_extra_data, astream->extradata, host_para->audio_extra_size);
        }
        host_para->audio_filter = -1; //default
        host_para->audio_output = -1; //defualt
    }
    //----video ----
    if (pctrl->has_video)
    {
        if (pctrl->cur_vst_index >= media_info->vst_num)
        {
            dt_warning (TAG, "video index set exceed max video num ,reset to 0 \n");
            pctrl->cur_vst_index = 0;
        }

        vstream_info_t *vstream = media_info->vstreams[pctrl->cur_vst_index];

        host_para->video_format = vstream->format;
        host_para->video_src_width = vstream->width;
        host_para->video_src_height = vstream->height;
        host_para->video_dest_width = (pctrl->width == -1) ? vstream->width : pctrl->width;
        host_para->video_dest_height = (pctrl->height == -1) ? vstream->height : pctrl->height;

        host_para->video_ratio = (vstream->sample_aspect_ratio.num << 16) | vstream->sample_aspect_ratio.den;
        host_para->video_num = vstream->time_base.num;
        host_para->video_den = vstream->time_base.den;
        if (vstream->frame_rate_ratio.den && vstream->frame_rate_ratio.num)
            host_para->video_fps = vstream->frame_rate_ratio.num / (double) vstream->frame_rate_ratio.den;
        else
            host_para->video_fps = vstream->time_base.den / (double) vstream->time_base.num;
        host_para->video_rate = (int) (90000 / host_para->video_fps);

        host_para->audio_extra_size = vstream->extradata_size;
        host_para->video_src_pixfmt = vstream->pix_fmt;

        host_para->vctx_priv = vstream->codec_priv;

        dt_info (TAG, "[%s:%d]format:%d width:%d height:%d fmt:%d rate:%d ratio:%d fps:%g \n", __FUNCTION__, __LINE__, host_para->video_format, host_para->video_src_width, host_para->video_src_height, host_para->video_src_pixfmt, host_para->video_rate, host_para->video_ratio, host_para->video_fps);
        if (host_para->video_extra_size > 0)
        {
            memset (host_para->video_extra_data, 0, host_para->video_extra_size);
            memcpy ((unsigned char *) host_para->video_extra_data, vstream->extradata, host_para->video_extra_size);
        }
        host_para->video_filter = -1; //defualt
        host_para->video_output = -1; //defualt
    }
    //----sub part------
    /* init dthost */
    dtp_ctx->host_priv = NULL;
    ret = dthost_init (&dtp_ctx->host_priv, host_para);
    if (ret < 0)
    {
        dt_error (TAG, "[%s:%d] DTHOST_INIF FAILED \n", __FUNCTION__, __LINE__);
        goto FAIL;
    }
    dt_info (TAG, "player host init ok \n");
    return 0;
  FAIL:
    return ret;
}

int player_host_start (dtplayer_context_t * dtp_ctx)
{
    return dthost_start (dtp_ctx->host_priv);
}

int player_host_pause (dtplayer_context_t * dtp_ctx)
{
    return dthost_pause (dtp_ctx->host_priv);
}

int player_host_resume (dtplayer_context_t * dtp_ctx)
{
    return dthost_resume (dtp_ctx->host_priv);
}

int player_host_stop (dtplayer_context_t * dtp_ctx)
{
    int ret = dthost_stop (dtp_ctx->host_priv);
    dt_info (TAG, "host module quit ok \n");
    return ret;
}
