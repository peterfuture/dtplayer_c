#include "dt_setting.h"

#include "dtplayer_host.h"

#define TAG "PLAYER-HOST"

int host_notify(void *Handle, int cmd, unsigned long ext1, unsigned long ext2)
{
    dtplayer_context_t *dtp_ctx = (dtplayer_context_t *)Handle;
    dtp_ctx->notify_cb(dtp_ctx->cookie, cmd, ext1, ext2);
    return 0;
}

int player_host_init(dtplayer_context_t * dtp_ctx)
{
    int ret;

    dthost_para_t *host_para = &dtp_ctx->host_para;
    player_ctrl_t *pctrl = &dtp_ctx->ctrl_info;
    memset(host_para, 0, sizeof(dthost_para_t));
    dtp_media_info_t *media_info = dtp_ctx->media_info;
    track_info_t *tracks = &(media_info->tracks);

    /* init dthost param */
    host_para->service_mgt = (void *)dtp_ctx->service_mgt;
    host_para->has_audio = pctrl->has_audio;
    host_para->has_video = pctrl->has_video;
    host_para->has_sub = pctrl->has_sub;
    host_para->sync_enable = pctrl->sync_enable;
    host_para->parent = (void *)dtp_ctx;
    //----audio ----
    dt_info(TAG, "[%s:%d] has video:%d has audio:%d has sub:%d \n", __FUNCTION__,
            __LINE__, host_para->has_video, host_para->has_audio, host_para->has_sub);
    if (pctrl->has_audio) {
        if (pctrl->disable_hw_acodec == 1) {
            host_para->aflag |= DTAV_FLAG_DISABLE_HW_CODEC;
            dt_info(TAG, "[%s:%d] disable omx\n", __FUNCTION__, __LINE__);
        }

        if (media_info->cur_ast_index >= tracks->ast_num) {
            dt_warning(TAG, "audio index set exceed max audio num ,reset to 0 \n");
            media_info->cur_ast_index = 0;
        }
        astream_info_t *astream = tracks->astreams[media_info->cur_ast_index];
        host_para->audio_format = astream->format;
        host_para->audio_channel = astream->channels;
        host_para->audio_samplerate = astream->sample_rate;
        int downmix = dtp_setting.audio_downmix;
        dt_info(TAG, "downmix:%d \n", downmix);
        if (downmix) {
            host_para->audio_dst_channels = (astream->channels > 2) ? 2 : astream->channels;
            dt_info(TAG, "downmix: %d -> %d \n", astream->channels,
                    host_para->audio_dst_channels);
        } else {
            host_para->audio_dst_channels = 0;
        }
        // Fixme: user set dst samplerate
        host_para->audio_dst_samplerate = 0;
        host_para->audio_bitrate = astream->bit_rate;
        host_para->audio_sample_fmt = astream->bps;

        host_para->audio_num = astream->time_base.num;
        host_para->audio_den = astream->time_base.den;

        host_para->audio_extra_size = astream->extradata_size;
        host_para->actx_priv = astream->codec_priv; //AVCodecContext
        if (host_para->audio_extra_size > 0) {
            memset(host_para->audio_extra_data, 0, host_para->audio_extra_size);
            memcpy((unsigned char *) host_para->audio_extra_data, astream->extradata,
                   host_para->audio_extra_size);
        }
        host_para->audio_filter = -1; //default
        host_para->audio_output = -1; //defualt
        host_para->ao_device = dtp_ctx->ao_device;
        dt_info(TAG, "host-audio setup ok, cur index:%d \n", media_info->cur_ast_index);
        dt_info(TAG, "[%s:%d] audio extra_size:%d time_base.num:%d den:%d\n",
                __FUNCTION__, __LINE__, host_para->audio_extra_size, host_para->audio_num,
                host_para->audio_den);
    }
    //----video ----
    if (pctrl->has_video) {
        if (media_info->cur_vst_index >= tracks->vst_num) {
            dt_warning(TAG, "video index set exceed max video num ,reset to 0 \n");
            media_info->cur_vst_index = 0;
        }

        vstream_info_t *vstream = tracks->vstreams[media_info->cur_vst_index];

        //disable hw vcodec
        if (pctrl->disable_hw_vcodec == 1) {
            host_para->vflag |= DTAV_FLAG_DISABLE_HW_CODEC;
            dt_info(TAG, "[%s:%d] disable omx\n", __FUNCTION__, __LINE__);
        }

        //int vout_type = pctrl->video_pixel_format;
        int vout_type = dtp_setting.video_pixel_format;
        dt_info(TAG, "vout type:%d - 0 yuv420 1 rgb565 2 nv12 \n", vout_type);

        host_para->video_format = vstream->format;
        host_para->video_src_width = vstream->width;
        host_para->video_src_height = vstream->height;
        host_para->video_dest_width = (pctrl->width <= 0) ? vstream->width :
                                      pctrl->width;
        host_para->video_dest_height = (pctrl->height <= 0) ? vstream->height :
                                       pctrl->height;

        host_para->video_ratio = (vstream->sample_aspect_ratio.num << 16) |
                                 vstream->sample_aspect_ratio.den;
        host_para->video_num = vstream->time_base.num;
        host_para->video_den = vstream->time_base.den;
        if (vstream->frame_rate_ratio.den && vstream->frame_rate_ratio.num) {
            host_para->video_fps = vstream->frame_rate_ratio.num / (double)
                                   vstream->frame_rate_ratio.den;
        } else {
            host_para->video_fps = vstream->time_base.den / (double) vstream->time_base.num;
        }
        host_para->video_rate = (int)(90000 / host_para->video_fps);

        host_para->video_extra_size = vstream->extradata_size;
        host_para->video_src_pixfmt = vstream->pix_fmt;
        if (vout_type == 0) {
            host_para->video_dest_pixfmt = DTAV_PIX_FMT_YUV420P;
        }
        if (vout_type == 1) {
            host_para->video_dest_pixfmt = DTAV_PIX_FMT_RGB565LE;
        }
        if (vout_type == 2) {
            host_para->video_dest_pixfmt = DTAV_PIX_FMT_NV12;
        }

        host_para->vctx_priv = vstream->codec_priv;

        if (host_para->video_extra_size > 0) {
            memset(host_para->video_extra_data, 0, host_para->video_extra_size);
            memcpy((unsigned char *) host_para->video_extra_data, vstream->extradata,
                   host_para->video_extra_size);
        }
        host_para->video_filter = -1; //defualt
        host_para->video_output = -1; //defualt
        host_para->vo_device = dtp_ctx->vo_device;
        dt_info(TAG, "host-video Setup ok, cur index:%d \n", media_info->cur_vst_index);
        dt_info(TAG,
                "[%s:%d]format:%d width:%d->%d height:%d->%d fmt:%d rate:%d ratio:%d fps:%g extra_size:%d\n",
                __FUNCTION__, __LINE__, host_para->video_format, host_para->video_src_width,
                host_para->video_dest_width, host_para->video_src_height,
                host_para->video_dest_height, host_para->video_src_pixfmt,
                host_para->video_rate, host_para->video_ratio, host_para->video_fps,
                host_para->video_extra_size);
        dt_info(TAG, "timebase->num:%d timebase->den:%d \n", host_para->video_num,
                host_para->video_den);
    }
    //----sub part------
    if (pctrl->has_sub) {
        if (media_info->cur_sst_index >= tracks->sst_num) {
            dt_warning(TAG, "sub index set exceed max sub num ,reset to 0 \n");
            media_info->cur_sst_index = 0;
        }

        sstream_info_t *sstream = tracks->sstreams[media_info->cur_sst_index];

        host_para->sub_format = sstream->format;
        host_para->sub_width = sstream->width;
        host_para->sub_height = sstream->height;
        host_para->so_device = dtp_ctx->so_device;
        host_para->sctx_priv = sstream->codec_priv;
        dt_info(TAG, "host-sub setup ok, cur index:%d \n", media_info->cur_sst_index);
    }

    /* init dthost */
    dtp_ctx->host_priv = NULL;
    host_para->notify = host_notify;
    ret = dthost_init(&dtp_ctx->host_priv, host_para);
    if (ret < 0) {
        dt_error(TAG, "[%s:%d] DTHOST_INIF FAILED \n", __FUNCTION__, __LINE__);
        goto FAIL;
    }
    dt_info(TAG, "player host init ok \n");
    return 0;
FAIL:
    return ret;
}

int player_host_start(dtplayer_context_t * dtp_ctx)
{
    return dthost_start(dtp_ctx->host_priv);
}

int player_host_pause(dtplayer_context_t * dtp_ctx)
{
    return dthost_pause(dtp_ctx->host_priv);
}

int player_host_resume(dtplayer_context_t * dtp_ctx)
{
    return dthost_resume(dtp_ctx->host_priv);
}

int player_host_stop(dtplayer_context_t * dtp_ctx)
{
    int ret = dthost_stop(dtp_ctx->host_priv);
    dt_info(TAG, "host module quit ok \n");
    return ret;
}

int player_host_resize(dtplayer_context_t * dtp_ctx, int w, int h)
{
    return dthost_video_resize(dtp_ctx->host_priv, w, h);
}

int player_host_get_info(dtplayer_context_t *dtp_ctx, enum HOST_CMD cmd,
                         unsigned long arg)
{
    return dthost_get_info(dtp_ctx->host_priv, cmd, arg);
}

int player_host_set_info(dtplayer_context_t *dtp_ctx, enum HOST_CMD cmd,
                         unsigned long arg)
{
    return dthost_set_info(dtp_ctx->host_priv, cmd, arg);
}