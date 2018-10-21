#include "dtport.h"

#define TAG "PORT-MGT"
//==Part1:Control
int port_stop(dtport_context_t * pctx)
{
    int has_video, has_audio, has_subtitle;
    has_audio = pctx->param.has_audio;
    has_video = pctx->param.has_video;
    has_subtitle = pctx->param.has_subtitle;
    if (has_audio) {
        packet_queue_release(&pctx->queue_audio);
    }
    if (has_video) {
        packet_queue_release(&pctx->queue_video);
    }
    if (has_subtitle) {
        packet_queue_release(&pctx->queue_subtitle);
    }
    return 0;
}

int port_init(dtport_context_t * pctx, dtport_para_t * para)
{
    int ret = 0;
    memcpy(&(pctx->param), para, sizeof(dtport_para_t));
    if (para->has_audio == 0 && para->has_video == 0) {
        dt_error(TAG, "[%s:%d] no av stream found \n", __FUNCTION__, __LINE__);
        return -1;
    }
    if (para->has_audio) {
        ret = packet_queue_init(&(pctx->queue_audio));
        if (ret < 0) {
            dt_info(TAG, "[%s:%d] port  init audio queeu failed has_audio:%d\n",
                    __FUNCTION__, __LINE__, pctx->param.has_audio);
            goto ERR0;
        }
        dt_info(TAG, "[%s:%d] port start init audio queeu has_audio:%d\n", __FUNCTION__,
                __LINE__, pctx->param.has_audio);

    }
    if (para->has_video) {
        dt_info(TAG, "[%s:%d] port start init video queeu has_video:%d\n", __FUNCTION__,
                __LINE__, pctx->param.has_video);
        packet_queue_init(&(pctx->queue_video));
    }
    if (para->has_subtitle) {
        dt_info(TAG, "[%s:%d] port start init sub queeu has_sub:%d\n", __FUNCTION__,
                __LINE__, pctx->param.has_subtitle);
        packet_queue_init(&(pctx->queue_subtitle));
    }

    // update setting
    if (dtp_setting.audio_max_num <= 0) {
        dtp_setting.audio_max_num = QUEUE_MAX_PACK_NUM;
    }
    if (dtp_setting.audio_max_size <= 0) {
        dtp_setting.audio_max_size = QUEUE_MAX_ABUF_SIZE;
    }
    if (dtp_setting.video_max_num <= 0) {
        dtp_setting.video_max_num = QUEUE_MAX_PACK_NUM;
    }
    if (dtp_setting.video_max_size <= 0) {
        dtp_setting.video_max_size = QUEUE_MAX_VBUF_SIZE;
    }
    return ret;
ERR0:
    return ret;
}

//==Part2:Data IO Relative
int port_write_frame(dtport_context_t * pctx, dt_av_pkt_t * frame, int type)
{
    int audio_write_enable = 1;
    int video_write_enable = 1;

    if (pctx->param.has_audio) {
        audio_write_enable = (pctx->queue_audio.nb_packets < dtp_setting.audio_max_num);
    } else {
        audio_write_enable = 0;
    }
    if (pctx->param.has_video) {
        video_write_enable = (pctx->queue_video.nb_packets < dtp_setting.video_max_size);
    } else {
        video_write_enable = 0;
    }

    if (!audio_write_enable && !video_write_enable) {
        dt_debug(TAG, "A-V HAVE ENOUGH BUFFER,DO NOT WRITE\n");
        return -1;
    }

    dt_packet_queue_t *queue;
    switch (type) {
    case DTP_MEDIA_TYPE_AUDIO:
        queue = &pctx->queue_audio;
        break;
    case DTP_MEDIA_TYPE_VIDEO:
        queue = &pctx->queue_video;
        break;
    case DTP_MEDIA_TYPE_SUBTITLE:
        queue = &pctx->queue_subtitle;
        break;
    default:
        dt_warning(TAG, "[%s] unkown frame type audio or video \n", __FUNCTION__);
        return -1;
    }

    dt_debug(TAG, "[%s:%d] start write frame type:%d NB_packet:%d queue->size:%d\n",
             __FUNCTION__, __LINE__, type, queue->nb_packets, queue->size);

    return packet_queue_put(queue, frame);
}

int port_read_frame(dtport_context_t * pctx, dt_av_pkt_t **frame, int type)
{
    dt_packet_queue_t *queue;
    switch (type) {
    case DTP_MEDIA_TYPE_AUDIO:
        queue = &pctx->queue_audio;
        break;
    case DTP_MEDIA_TYPE_VIDEO:
        queue = &pctx->queue_video;
        break;
    case DTP_MEDIA_TYPE_SUBTITLE:
        queue = &pctx->queue_subtitle;
        break;
    default:
        dt_warning(TAG, "[%s] unkown frame type audio or video \n", __FUNCTION__);
        return -1;
    }
    dt_debug(TAG, "[%s:%d]READ FRAME START, start read frame type:%d nb:%d\n",
             __FUNCTION__, __LINE__, type, queue->nb_packets);
    int ret = packet_queue_get(queue, frame);
    dt_debug(TAG, "[%s:%d]READ FRAME END \n", __FUNCTION__, __LINE__);
    return ret;
}

//==Part3:Status Relative
int port_get_state(dtport_context_t * pctx, buf_state_t * buf_state, int type)
{
    dt_packet_queue_t *queue;
    switch (type) {
    case DTP_MEDIA_TYPE_AUDIO:
        queue = &pctx->queue_audio;
        break;
    case DTP_MEDIA_TYPE_VIDEO:
        queue = &pctx->queue_video;
        break;
    case DTP_MEDIA_TYPE_SUBTITLE:
        queue = &pctx->queue_subtitle;
        break;
    default:
        dt_warning(TAG, "[%s] unkown frame type audio or video \n", __FUNCTION__);
        return -1;
    }
    dt_debug(TAG, "[%s:%d] start read status type:%d nb packets:%d\n", __FUNCTION__,
             __LINE__, type, queue->nb_packets);
    buf_state->data_len = queue->size;
    buf_state->size = queue->nb_packets;
    return 0;

}
