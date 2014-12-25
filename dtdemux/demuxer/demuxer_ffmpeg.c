#include "../dtdemuxer.h"
#include "dt_error.h"

#include "libavformat/avformat.h"
#include "libavformat/avio.h"

#include <string.h>

#define TAG "DEMUX-FFMPEG"

#define FFMPEG_BUF_SIZE 32768 // equal to ffmpeg default value - IO_BUFFER_SIZE

typedef struct
{
    char *key;
    int value;
} type_map_t;

static const type_map_t media_map[] = {
    {"mpegts", DT_MEDIA_FORMAT_MPEGTS},
    {"mpeg", DT_MEDIA_FORMAT_MPEGPS},
    {"rm", DT_MEDIA_FORMAT_RM},
    {"avi", DT_MEDIA_FORMAT_AVI},
    {"mkv", DT_MEDIA_FORMAT_MKV},
    {"matroska", DT_MEDIA_FORMAT_MKV},
    {"mov", DT_MEDIA_FORMAT_MOV},
    {"mp4", DT_MEDIA_FORMAT_MP4},
    {"flv", DT_MEDIA_FORMAT_FLV},
    {"aac", DT_MEDIA_FORMAT_AAC},
    {"ac3", DT_MEDIA_FORMAT_AC3},
    {"mp3", DT_MEDIA_FORMAT_MP3},
    {"mp2", DT_MEDIA_FORMAT_MP3},
    {"wav", DT_MEDIA_FORMAT_WAV},
    {"dts", DT_MEDIA_FORMAT_DTS},
    {"flac", DT_MEDIA_FORMAT_FLAC},
    {"h264", DT_MEDIA_FORMAT_H264},
    {"cavs", DT_MEDIA_FORMAT_AVS},
    {"mpegvideo", DT_MEDIA_FORMAT_M2V},
    {"p2p", DT_MEDIA_FORMAT_P2P},
    {"asf", DT_MEDIA_FORMAT_ASF},
    {"m4a", DT_MEDIA_FORMAT_MP4},
    {"m4v", DT_MEDIA_FORMAT_MP4},
    {"rtsp", DT_MEDIA_FORMAT_RTSP},
    {"ape", DT_MEDIA_FORMAT_APE},
    {"amr", DT_MEDIA_FORMAT_AMR},
};

typedef struct{
    struct AVFormatContext *ic;
    void *stream_ext;
    uint8_t *buf;
    AVBitStreamFilterContext *bsfc;
}ffmpeg_ctx_t;


static int read_packet(void *opaque,uint8_t *buf,int size)
{
    return dtstream_read(opaque,buf,size);
}

static int64_t seek_packet(void *opaque,int64_t offset,int whence)
{
    return dtstream_seek(opaque,offset,whence);
}

/* 1 select 0 non select*/
static int demuxer_ffmpeg_probe(demuxer_wrapper_t *wrapper,dt_buffer_t *buf)
{
    return 1;
}

static int demuxer_ffmpeg_open(demuxer_wrapper_t * wrapper)
{
    int err, ret;
    
    dtdemuxer_context_t *ctx =(dtdemuxer_context_t *)wrapper->parent;
    char *file_name = ctx->file_name;

    ffmpeg_ctx_t *ffmpeg_ctx =(ffmpeg_ctx_t *)malloc(sizeof(*ffmpeg_ctx));
    memset(ffmpeg_ctx, 0, sizeof(ffmpeg_ctx_t));
    av_register_all();
    AVFormatContext *ic = avformat_alloc_context();
    AVInputFormat *iformat = NULL;
    /*register ext stream, ffmpeg will use */
    //==================================================
    ffmpeg_ctx->stream_ext = ctx->stream_priv;
    ffmpeg_ctx->buf =(uint8_t *)malloc(FFMPEG_BUF_SIZE);
    if(!ffmpeg_ctx->buf)
    {
        dt_error(TAG,"[%s:%d] buf malloc failed\n",__FUNCTION__,__LINE__);
        ffmpeg_ctx->buf = NULL;
        return -1;
    }
    AVIOContext *io_ctx = avio_alloc_context(ffmpeg_ctx->buf,FFMPEG_BUF_SIZE,0,ffmpeg_ctx->stream_ext,read_packet,NULL,seek_packet);
    ic->pb = io_ctx; // Here to replace AVIOContext
    
    err = avformat_open_input(&ic, file_name, ic->iformat, NULL);
    if(err < 0)
    {
        dt_error(TAG,"avformat_open_input failed, err:%x - %x \n", err, AVUNERROR(err));
        return -1;
    }
    dt_info(TAG, "[%s:%d] avformat_open_input ok\n", __FUNCTION__, __LINE__);

    err = avformat_find_stream_info(ic, NULL);
    if(err < 0)
    {
        dt_error(TAG, "%s: could not find codec parameters\n", file_name);
        ret = -1;
        goto FAIL;
    }
    
    av_dump_format(ic,0,NULL,0);
    ffmpeg_ctx->ic = ic;
    wrapper->demuxer_priv =(void *) ffmpeg_ctx;

    dt_info(TAG, "[%s:%d] start_time:%lld \n", __FUNCTION__, __LINE__, ic->start_time);
    return 0;
  FAIL:
    avformat_close_input(&ic);
    return ret;
}

static int64_t pts_exchange(AVPacket * avpkt, dt_media_info_t * media_info)
{
    double exchange = 1;
    int64_t result;
    int num, den;
    int has_video = media_info->has_video;
    int has_audio = media_info->has_audio;
    int has_sub = media_info->has_sub;

    int cur_vidx =(has_video) ? media_info->vstreams[media_info->cur_vst_index]->index : -1;
    int cur_aidx =(has_audio) ? media_info->astreams[media_info->cur_ast_index]->index : -1;
    int cur_sidx =(has_sub)?media_info->sstreams[media_info->cur_sst_index]->index:-1;

    if(has_video && cur_vidx == avpkt->stream_index)
    {
        num = media_info->vstreams[media_info->cur_vst_index]->time_base.num;
        den = media_info->vstreams[media_info->cur_vst_index]->time_base.den;
        exchange = DT_PTS_FREQ * num /(double)den;
    }
    else if(has_audio && cur_aidx == avpkt->stream_index)
    {
        num = media_info->astreams[media_info->cur_ast_index]->time_base.num;
        den = media_info->astreams[media_info->cur_ast_index]->time_base.den;
        exchange = DT_PTS_FREQ *(double) num / den;
    }
    else if(has_sub && cur_sidx == avpkt->stream_index)
    {
        //num = media_info->sstreams[media_info->cur_sst_index]->time_base.num;
        //den = media_info->sstreams[media_info->cur_sst_index]->time_base.den;
        num = den = 1;
    }
    else // err
    {
        num = den = 1;
    }

    //case 1: pts valid
    if(avpkt->pts != AV_NOPTS_VALUE)
        return(int64_t)(avpkt->pts * exchange);

    //case 2: pts invalid
    
    //dts invalid case - following pts will increase with in decoder
    if(avpkt->dts == AV_NOPTS_VALUE)
    {
        return 0;
    }
    
    //dts valid case
    result =(int64_t)(exchange * avpkt->dts);
    dt_debug(TAG, "pts:%llx setup from dts: %llx  exchange:%f \n", result, avpkt->dts,(float)exchange);
    return result;
}

#ifdef ENABLE_ANDROID
static int update_video_frame(demuxer_wrapper_t *wrapper, AVPacket *avpkt)
{
    dtdemuxer_context_t *dem_ctx =(dtdemuxer_context_t *) wrapper->parent;
    dt_media_info_t *media_info = &dem_ctx->media_info;

    int has_video =(media_info->disable_video) ? 0 : media_info->has_video;

    int cur_vidx =(has_video) ? media_info->vstreams[media_info->cur_vst_index]->index : -1;

    if(cur_vidx == -1)
    {
        dt_info(TAG, "cur vidx invalid\n");
        return 0;
    }

    ffmpeg_ctx_t *ctx =(ffmpeg_ctx_t *)wrapper->demuxer_priv;
    AVFormatContext *ic = ctx->ic;

    AVStream *pStream = ic->streams[cur_vidx];
    AVCodecContext *pCodec = pStream->codec;

    if(pCodec->codec_id != AV_CODEC_ID_H264)
    {
        dt_info(TAG, "not H264, no need to filter\n");
        return 0;
    }
    AVPacket pkt = *avpkt;
    //init
    if(!ctx->bsfc)
    {
        ctx->bsfc = av_bitstream_filter_init("h264_mp4toannexb");
        if(!ctx->bsfc)
        {
            dt_info(TAG, "filter init failed \n");
            return 0;
        }
    }

    int ret = av_bitstream_filter_filter(ctx->bsfc, pCodec, NULL, &pkt.data, &pkt.size, avpkt->data, avpkt->size, avpkt->flags & AV_PKT_FLAG_KEY);
    if(ret <= 0)
        return 0;
    free(avpkt->data); // replace with pkt data, Here need to free
    *avpkt = pkt;
    return 0;
}

#endif

static int demuxer_ffmpeg_read_frame(demuxer_wrapper_t * wrapper, dt_av_pkt_t * frame)
{
    dtdemuxer_context_t *dem_ctx =(dtdemuxer_context_t *) wrapper->parent;
    dt_media_info_t *media_info = &dem_ctx->media_info;

    int has_audio =(media_info->disable_audio) ? 0 : media_info->has_audio;
    int has_video =(media_info->disable_video) ? 0 : media_info->has_video;
    int has_sub =(media_info->disable_sub) ? 0 : media_info->has_sub;

    int cur_aidx =(has_audio) ? media_info->astreams[media_info->cur_ast_index]->index : -1;
    int cur_vidx =(has_video) ? media_info->vstreams[media_info->cur_vst_index]->index : -1;
    int cur_sidx =(has_sub) ? media_info->sstreams[media_info->cur_sst_index]->index : -1;

    ffmpeg_ctx_t *ctx =(ffmpeg_ctx_t *)wrapper->demuxer_priv;
    AVFormatContext *ic = ctx->ic;
    AVPacket avpkt;
    int ret = av_read_frame(ic, &avpkt);
    if(ret < 0)
    {
        if(AVERROR(EAGAIN) != ret)
        {
            /*if the return is EAGAIN,we need to try more times */
            dt_info(TAG, "[%s:%d]av_read_frame return(%d)\n", __FUNCTION__, __LINE__, ret);

            if(AVERROR_EOF == ret || ret == -1)
                return DTERROR_READ_EOF;
            else
                return DTERROR_READ_FAILED;
        }
        return DTERROR_READ_AGAIN;
    }
    //read frame ok
    if(has_video && cur_vidx == avpkt.stream_index)
    {
#ifdef ENABLE_ANDROID
        dt_debug(TAG, "befor filter, %02x %02x %02x %02x \n", avpkt.data[0], avpkt.data[1], avpkt.data[2], avpkt.data[3]);
        update_video_frame(wrapper, &avpkt); // update video frame, header etc...
        dt_debug(TAG, "after filter, %02x %02x %02x %02x \n", avpkt.data[0], avpkt.data[1], avpkt.data[2], avpkt.data[3]);
#endif
        frame->type = AVMEDIA_TYPE_VIDEO;
    }
    else if(has_audio && cur_aidx == avpkt.stream_index)
        frame->type = AVMEDIA_TYPE_AUDIO;
    else if(has_sub && cur_sidx == avpkt.stream_index)
        frame->type = AVMEDIA_TYPE_SUBTITLE;
    else
    {
        frame->type = AVMEDIA_TYPE_UNKNOWN;
        av_free_packet(&avpkt);
        return DTERROR_READ_AGAIN; // need to read again
    }

    //setup frame
    frame->data = avpkt.data;
    frame->size = avpkt.size;
    frame->pts = pts_exchange(&avpkt, media_info);
    frame->dts = avpkt.dts;
    frame->duration = avpkt.duration;
    frame->key_frame = avpkt.flags & AV_PKT_FLAG_KEY;
    if(frame->type ==(int)AVMEDIA_TYPE_AUDIO)
    {
        dt_debug(TAG,"GET AUDIO FRAME, pts:%llx dts:%llx size:%d \n", frame->pts, frame->dts, frame->size);
    }
    if(frame->type ==(int)AVMEDIA_TYPE_VIDEO)
    {
        dt_debug(TAG,"GET VIDEO FRAME, pts:%llx dts:%llx size:%d key:%d\n",frame->pts, frame->dts,frame->size,frame->key_frame);
    }
    if(frame->type == (int)AVMEDIA_TYPE_SUBTITLE)
    {
        dt_debug(TAG,"GET SUB FRAME, pts:%llx dts:%llx size:%d \n",frame->pts, frame->dts,frame->size);
    }
    //dt_info(TAG, "read ok,frame size:%d %02x %02x %02x %02x addr:%p type:%d\n", frame->size, frame->data[0], frame->data[1], frame->data[2], frame->data[3], frame->data,frame->type);
    //dt_debug(TAG, "SIDE_DATA_ELEMENT:%d \n", avpkt.side_data_elems);

    return DTERROR_NONE;
}

static int media_format_convert(const char *name)
{
    int i, j;
    int type = DT_MEDIA_FORMAT_INVALID;
    j = sizeof(media_map) / sizeof(type_map_t);
    for(i = 0; i < j; i++)
    {
        if(strcmp(name, media_map[i].key) == 0)
        {
            break;
        }
    }
    if(i == j)
    {
        for(i = 0; i < j; i++)
        {
            if(strstr(name, media_map[i].key) != NULL)
            {
                break;
            }
            if(i == j)
            {
                dt_error("Unsupport media type %s\n", name);
                return DT_MEDIA_FORMAT_INVALID;
            }
        }
    }
    type = media_map[i].value;
    dt_info(TAG, "name:%s media_type=%d \n", name, type);
    return type;

}

dtaudio_format_t audio_format_convert(enum AVCodecID id)
{
    dtaudio_format_t format = DT_AUDIO_FORMAT_INVALID;
    switch(id)
    {
    default:
        format = DT_AUDIO_FORMAT_UNKOWN;
        break;
    }
    dt_info(TAG, "[audio_format_convert]audio codec_id=0x%x format=%d\n", id, format);
    return format;
}

static int video_format_convert(enum AVCodecID id)
{
    dtvideo_format_t format;
    switch(id)
    {
    case AV_CODEC_ID_H264:
        format = DT_VIDEO_FORMAT_H264;
        break;
    default:
            format = DT_VIDEO_FORMAT_UNKOWN;
            break;
    }
    dt_info(TAG, "[video_format_convert]video codec_id=0x%x format=%d\n", id, format);
    return format;
}

static int subtitle_format_convert(int id)
{
    return DT_SUBTITLE_FORMAT_UNKOWN;

}

static int format2bps(int fmt)
{
    int ret;
    switch(fmt)
    {
    case AV_SAMPLE_FMT_U8:
        ret = 8;
        break;
    case AV_SAMPLE_FMT_S16:
        ret = 16;
        break;
    case AV_SAMPLE_FMT_S32:
        ret = 32;
        break;
    default:
        ret = 16;
    }
    dt_info(TAG, "FORMAT2BPS FMT:%d bps:%d \n", fmt, ret);
    return ret;
}

static int demuxer_ffmpeg_setup_info(demuxer_wrapper_t * wrapper, dt_media_info_t * info)
{
    ffmpeg_ctx_t *ctx =(ffmpeg_ctx_t *)wrapper->demuxer_priv;
    AVFormatContext *ic = ctx->ic;
    AVStream *pStream;
    AVCodecContext *pCodec;
    vstream_info_t *vst_info;
    astream_info_t *ast_info;
    sstream_info_t *sst_info;
    int i;

    /*reset vars */
    memset(info, 0, sizeof(*info));
    //set cur stream index -1 ,other vars have been reset to 0
    info->cur_ast_index = -1;
    info->cur_vst_index = -1;
    info->cur_sst_index = -1;

    /*get media info */
    info->format = DT_MEDIA_FORMAT_INVALID;
    info->bit_rate = ic->bit_rate;
    double duration =((double) ic->duration / AV_TIME_BASE);
    info->duration =(int)(ic->duration / AV_TIME_BASE);
    strcpy(info->file_name, ic->filename);
    dt_info(TAG, "file name:%s duration:%lf\n", info->file_name, duration);
    info->file_size = avio_size(ic->pb);

    info->format = media_format_convert(ic->iformat->name);
    if(info->format == DT_MEDIA_FORMAT_INVALID)
        dt_warning(TAG, "get wrong media format\n");

    //get stream info
    for(i = 0; i < ic->nb_streams; i++)
    {
        pStream = ic->streams[i];
        pCodec = pStream->codec;
        if(pCodec->codec_type == AVMEDIA_TYPE_VIDEO)
        {

            //for some mp3 have video, just skip
            if(info->format == DT_MEDIA_FORMAT_MP3)
                continue;

            vst_info =(vstream_info_t *) malloc(sizeof(vstream_info_t));
            memset(vst_info, 0, sizeof(vstream_info_t));
            vst_info->index = pStream->index;
            vst_info->id = pStream->id;
            vst_info->width = pCodec->width;
            vst_info->height = pCodec->height;
            vst_info->pix_fmt = pCodec->pix_fmt;
            vst_info->duration =(int64_t)(pStream->duration * pStream->time_base.num / pStream->time_base.den);
            vst_info->bit_rate = pCodec->bit_rate;
            vst_info->format = video_format_convert(pCodec->codec_id);
            vst_info->sample_aspect_ratio.num = pStream->sample_aspect_ratio.num;
            vst_info->sample_aspect_ratio.den = pStream->sample_aspect_ratio.den;
            vst_info->frame_rate_ratio.num = pStream->r_frame_rate.num;
            vst_info->frame_rate_ratio.den = pStream->r_frame_rate.den;
            vst_info->time_base.num = pStream->time_base.num;
            vst_info->time_base.den = pStream->time_base.den;
            vst_info->codec_priv =(void *) pCodec;
            info->vstreams[info->vst_num] = vst_info;
            info->vst_num++;
            if(pCodec->extradata_size)
            {
                vst_info->extradata_size = pCodec->extradata_size;
                memcpy(vst_info->extradata, pCodec->extradata, pCodec->extradata_size);
            }
            dt_info(TAG, "VIDEO EXTRA DATA SIZE:%d - %d \n", pCodec->extradata_size, vst_info->extradata_size);
            if(pCodec->extradata_size > 0)
            dt_info(TAG, "data: %02x %02x %02x %02x %02x %02x %02x %02x \n", 
                    pCodec->extradata[0],
                    pCodec->extradata[1],
                    pCodec->extradata[2],
                    pCodec->extradata[3],
                    pCodec->extradata[4],
                    pCodec->extradata[5],
                    pCodec->extradata[6],
                    pCodec->extradata[7]
                    );
#if ENABLE_ANDROID
            //if(info->format == DT_MEDIA_FORMAT_MPEGTS)
            // android no need extradata for H264
            if(pCodec->codec_id == AV_CODEC_ID_H264)
            {
                vst_info->extradata_size = 0;
                dt_info(TAG, "TS no need extradata \n");
            }
#endif
        }
        else if(pCodec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            ast_info =(astream_info_t *) malloc(sizeof(astream_info_t));
            memset(ast_info, 0, sizeof(astream_info_t));
            ast_info->index = pStream->index;
            ast_info->id = pStream->id;
            ast_info->channels = pCodec->channels;
            ast_info->sample_rate = pCodec->sample_rate;
            ast_info->bps = format2bps(pCodec->sample_fmt);
            ast_info->duration =(int64_t)(pStream->duration * pStream->time_base.num / pStream->time_base.den);
            ast_info->time_base.num = pStream->time_base.num;
            ast_info->time_base.den = pStream->time_base.den;
            ast_info->bit_rate = pCodec->bit_rate;
            ast_info->format = audio_format_convert(pCodec->codec_id);
            ast_info->codec_priv = (void *)pCodec;
            info->astreams[info->ast_num] = ast_info;
            info->ast_num++;
        }
        else if(pCodec->codec_type == AVMEDIA_TYPE_SUBTITLE)
        {
            sst_info =(sstream_info_t *) malloc(sizeof(sstream_info_t));
            memset(sst_info, 0, sizeof(sstream_info_t));
            sst_info->index = pStream->index;
            sst_info->id = pStream->id;
            sst_info->width = pCodec->width;
            sst_info->height = pCodec->height;
            sst_info->format = subtitle_format_convert(pCodec->codec_id);
            sst_info->codec_priv =(void *) pCodec;
            info->sstreams[info->sst_num] = sst_info;
            info->sst_num++;
        }
    }
    if(info->vst_num > 0)
    {
        info->has_video = 1;
        info->cur_vst_index = 0;
    }
    if(info->ast_num > 0)
    {
        info->has_audio = 1;
        info->cur_ast_index = 0;
    }
    if(info->sst_num > 0)
    {
        info->has_sub = 1;
        info->cur_sst_index = 0;
    }
    return 0;
}

static int demuxer_ffmpeg_seek_frame(demuxer_wrapper_t * wrapper, int64_t timestamp)
{
    ffmpeg_ctx_t *ctx =(ffmpeg_ctx_t *)wrapper->demuxer_priv;
    AVFormatContext *ic = ctx->ic;

    int seek_flags = 0; //seek key frame as default

    seek_flags =(dtp_setting.demuxer_seek_keyframe == 0)?AVSEEK_FLAG_ANY:AVSEEK_FLAG_BACKWARD;
    dt_info(TAG,"seek keyframe enabled:%d \n", dtp_setting.demuxer_seek_keyframe);
    
    int64_t seek_target = timestamp;
    int64_t seek_min =(seek_target > 0) ? seek_target - timestamp + 2 : INT64_MIN;
    int64_t seek_max =(seek_target < 0) ? seek_target - timestamp - 2 : INT64_MAX;
    int64_t ret = avformat_seek_file(ic, -1, seek_min, seek_target, seek_max, seek_flags);

    if(ret >= 0)
    {
        dt_info(TAG, "AV_FORMAT_SEEK_FILE OK \n");
        return 0;
    }

    dt_info(TAG, "AV_FORMAT_SEEK_FILE FAIL \n");
    return -1;
}

static int demuxer_ffmpeg_close(demuxer_wrapper_t * wrapper)
{
    ffmpeg_ctx_t *ctx =(ffmpeg_ctx_t *)wrapper->demuxer_priv;
    AVFormatContext *ic = ctx->ic;
    if(ic) avformat_close_input(&ic);
    if(ctx->bsfc)
        av_bitstream_filter_close(ctx->bsfc);
    ctx->bsfc = NULL;
    ctx->buf = NULL; // no need to free ctx->buf, will free in avformat_close_input
    free(ctx);
    wrapper->demuxer_priv = NULL;
    return 0;
}

demuxer_wrapper_t demuxer_ffmpeg = {
    .name = "ffmpeg demuxer",
    .id = DEMUXER_FFMPEG, 
    .probe = demuxer_ffmpeg_probe,
    .open = demuxer_ffmpeg_open,
    .read_frame = demuxer_ffmpeg_read_frame,
    .setup_info = demuxer_ffmpeg_setup_info,
    .seek_frame = demuxer_ffmpeg_seek_frame,
    .close = demuxer_ffmpeg_close
};
