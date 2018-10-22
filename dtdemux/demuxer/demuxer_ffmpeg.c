#include <string.h>

#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavutil/dict.h"

#include "dt_error.h"
#include "dt_setting.h"

#include "dtdemuxer.h"
#include "dtstream_api.h"

#define TAG "DEMUX-FFMPEG"

#define FFMPEG_BUF_SIZE 32768 // equal to ffmpeg default value - IO_BUFFER_SIZE

typedef struct {
    char *key;
    int value;
} type_map_t;

static const type_map_t media_map[] = {
    {"mpegts", DTP_MEDIA_FORMAT_MPEGTS},
    {"mpeg", DTP_MEDIA_FORMAT_MPEGPS},
    {"rm", DTP_MEDIA_FORMAT_RM},
    {"avi", DTP_MEDIA_FORMAT_AVI},
    {"mkv", DTP_MEDIA_FORMAT_MKV},
    {"matroska", DTP_MEDIA_FORMAT_MKV},
    {"mov", DTP_MEDIA_FORMAT_MOV},
    {"mp4", DTP_MEDIA_FORMAT_MP4},
    {"flv", DTP_MEDIA_FORMAT_FLV},
    {"aac", DTP_MEDIA_FORMAT_AAC},
    {"ac3", DTP_MEDIA_FORMAT_AC3},
    {"mp3", DTP_MEDIA_FORMAT_MP3},
    {"mp2", DTP_MEDIA_FORMAT_MP3},
    {"wav", DTP_MEDIA_FORMAT_WAV},
    {"dts", DTP_MEDIA_FORMAT_DTS},
    {"flac", DTP_MEDIA_FORMAT_FLAC},
    {"h264", DTP_MEDIA_FORMAT_H264},
    {"cavs", DTP_MEDIA_FORMAT_AVS},
    {"mpegvideo", DTP_MEDIA_FORMAT_M2V},
    {"p2p", DTP_MEDIA_FORMAT_P2P},
    {"asf", DTP_MEDIA_FORMAT_ASF},
    {"m4a", DTP_MEDIA_FORMAT_MP4},
    {"m4v", DTP_MEDIA_FORMAT_MP4},
    {"rtsp", DTP_MEDIA_FORMAT_RTSP},
    {"ape", DTP_MEDIA_FORMAT_APE},
    {"amr", DTP_MEDIA_FORMAT_AMR},
};

typedef struct {
    struct AVFormatContext *ic;
    void *stream_ext;
    uint8_t *buf;
    AVBitStreamFilterContext *bsfc;
} ffmpeg_ctx_t;


static int read_packet(void *opaque, uint8_t *buf, int size)
{
    return dtstream_read(opaque, buf, size);
}

static int64_t seek_packet(void *opaque, int64_t offset, int whence)
{
    return dtstream_seek(opaque, offset, whence);
}

/* 1 select 0 non select*/
static int demuxer_ffmpeg_probe(demuxer_wrapper_t *wrapper, dt_buffer_t *buf)
{
    return 1;
}

static void my_log_callback(void *ptr, int level, const char *fmt, va_list vl)
{
    switch (level) {
    case AV_LOG_DEBUG:
#if ENABLE_ANDROID
        __android_log_vprint(ANDROID_LOG_DEBUG, "FFMPEG", fmt, vl);
#else
        vprintf(fmt, vl);
#endif
        break;
    case AV_LOG_VERBOSE:
#if ENABLE_ANDROID
        __android_log_vprint(ANDROID_LOG_VERBOSE, "FFMPEG", fmt, vl);
#else
        vprintf(fmt, vl);
#endif
        break;
    case AV_LOG_INFO:
#if ENABLE_ANDROID
        __android_log_vprint(ANDROID_LOG_INFO, "FFMPEG", fmt, vl);
#else
        vprintf(fmt, vl);
#endif
        break;
    case AV_LOG_WARNING:
#if ENABLE_ANDROID
        __android_log_vprint(ANDROID_LOG_WARN, "FFMPEG", fmt, vl);
#else
        vprintf(fmt, vl);
#endif
        break;
    case AV_LOG_ERROR:
#if ENABLE_ANDROID
        __android_log_vprint(ANDROID_LOG_ERROR, "FFMPEG", fmt, vl);
#else
        vprintf(fmt, vl);
#endif
        break;
    default:
#if ENABLE_ANDROID
        __android_log_vprint(ANDROID_LOG_DEBUG, "FFMPEG", fmt, vl);
#else
        vprintf(fmt, vl);
#endif
    }
}

static void syslog_init()
{
    av_log_set_level(AV_LOG_DEBUG);
    av_log_set_callback(my_log_callback);
}


static int ff_interrupt_cb(void *arg)
{
    dtdemuxer_context_t *ctx = (dtdemuxer_context_t *)arg;
    return dt_check_interrupt(ctx->para.cb);
}

static int demuxer_ffmpeg_open(demuxer_wrapper_t * wrapper)
{
    int err, ret;

    dtdemuxer_context_t *ctx = (dtdemuxer_context_t *)wrapper->parent;
    char *file_name = ctx->para.file_name;

    //syslog_init();
    ffmpeg_ctx_t *ffmpeg_ctx = (ffmpeg_ctx_t *)malloc(sizeof(*ffmpeg_ctx));
    memset(ffmpeg_ctx, 0, sizeof(ffmpeg_ctx_t));
    av_register_all();
    avformat_network_init();
    AVFormatContext *ic = avformat_alloc_context();
    /*register ext stream, ffmpeg will use */
    //==================================================
    if (ctx->stream_priv) {
        ffmpeg_ctx->stream_ext = ctx->stream_priv;
        ffmpeg_ctx->buf = (uint8_t *)malloc(FFMPEG_BUF_SIZE);
        if (!ffmpeg_ctx->buf) {
            dt_error(TAG, "[%s:%d] buf malloc failed\n", __FUNCTION__, __LINE__);
            ffmpeg_ctx->buf = NULL;
            return -1;
        }
        AVIOContext *io_ctx = avio_alloc_context(ffmpeg_ctx->buf, FFMPEG_BUF_SIZE, 0,
                              ffmpeg_ctx->stream_ext, read_packet, NULL, seek_packet);
        ic->pb = io_ctx; // Here to replace AVIOContext
    } else {
        dt_info(TAG, "dtstream null, use ffmpeg instead \n");
    }

    //AVDictionary *d = *(AVDictionary **)wrapper->para->options;
    AVDictionary *d = NULL;
    AVDictionaryEntry *t = NULL;
    // set options
    char buf[20];
    av_dict_set(&d, "protocol_whitelist", "file,http,hls,udp,rtp,rtsp,tcp,sdp", 0);
    sprintf(buf, "%d", dtp_setting.player_live_timeout * 1000); // us
    av_dict_set(&d, "timeout", buf, 0);
    // dump key
    dt_info(TAG, "dict count:%d \n", av_dict_count(d));
    while (t = av_dict_get(d, "", t, AV_DICT_IGNORE_SUFFIX)) {
        dt_info(TAG, "key:%s value:%s \n", t->key, t->value);
    }

    // register interrupt
    ic->interrupt_callback.callback = ff_interrupt_cb;
    ic->interrupt_callback.opaque = ctx;
    //ic->flags |= AVFMT_FLAG_NONBLOCK;

    dt_info(TAG, "Interrupt: %p\n", ffmpeg_ctx);

    // rtp - udp - sdp specify h264
    if (strstr(file_name, ".sdp") != NULL) {
        ic->iformat = av_find_input_format("sdp");
        dt_info(TAG, "specify sdp protocal.\n");
    }

    err = avformat_open_input(&ic, file_name, ic->iformat, &d);
    av_dict_free(&d);
    if (err < 0) {
        dt_error(TAG, "avformat_open_input failed, err:%x - %x \n", err,
                 AVUNERROR(err));
        return -1;
    }
    dt_info(TAG, "[%s:%d] avformat_open_input ok\n", __FUNCTION__, __LINE__);

    err = avformat_find_stream_info(ic, NULL);
    if (err < 0) {
        dt_error(TAG, "%s: could not find codec parameters\n", file_name);
        ret = -1;
        goto FAIL;
    }

    av_dump_format(ic, 0, NULL, 0);
    ffmpeg_ctx->ic = ic;
    wrapper->demuxer_priv = (void *) ffmpeg_ctx;

    dt_info(TAG, "[%s:%d] start_time:%lld \n", __FUNCTION__, __LINE__,
            ic->start_time);
    return 0;
FAIL:
    avformat_close_input(&ic);
    return ret;
}

static int64_t pts_exchange(AVPacket * avpkt, dtp_media_info_t * media_info)
{
    double exchange = 1;
    int64_t result;
    int num, den;
    track_info_t *tracks = &(media_info->tracks);

    int has_video = media_info->has_video;
    int has_audio = media_info->has_audio;
    int has_sub = media_info->has_sub;

    int cur_vidx = (has_video
                    && !media_info->disable_video) ?
                   tracks->vstreams[media_info->cur_vst_index]->index : -1;
    int cur_aidx = (has_audio
                    && !media_info->disable_audio) ?
                   tracks->astreams[media_info->cur_ast_index]->index : -1;
    int cur_sidx = (has_sub
                    && !media_info->disable_sub) ?
                   tracks->sstreams[media_info->cur_sst_index]->index : -1;

    if (has_video && cur_vidx == avpkt->stream_index) {
        num = tracks->vstreams[media_info->cur_vst_index]->time_base.num;
        den = tracks->vstreams[media_info->cur_vst_index]->time_base.den;
        exchange = DT_PTS_FREQ * ((double)num / (double)den);
    } else if (has_audio && cur_aidx == avpkt->stream_index) {
        num = tracks->astreams[media_info->cur_ast_index]->time_base.num;
        den = tracks->astreams[media_info->cur_ast_index]->time_base.den;
        exchange = DT_PTS_FREQ * ((double)num / (double)den);
    } else if (has_sub && cur_sidx == avpkt->stream_index) {
        //num = tracks->sstreams[media_info->cur_sst_index]->time_base.num;
        //den = tracks->sstreams[media_info->cur_sst_index]->time_base.den;
        num = den = 1;
    } else { // err
        num = den = 1;
    }

    //case 1: pts valid
    if (avpkt->pts != AV_NOPTS_VALUE) {
        return (int64_t)(avpkt->pts * exchange);
    }

    //case 2: pts invalid

    //dts invalid case - following pts will increase with in decoder
    if (avpkt->dts == AV_NOPTS_VALUE) {
        return 0;
    }

    //dts valid case
    result = (int64_t)(exchange * avpkt->dts);
    dt_debug(TAG, "pts:%llx dts: %llx  exchange:%f -> %llx\n", avpkt->pts,
             avpkt->dts, (float)exchange, result);
    return result;
}

static int demuxer_ffmpeg_read_frame(demuxer_wrapper_t * wrapper,
                                     dt_av_pkt_t **pkt)
{
    dtdemuxer_context_t *dem_ctx = (dtdemuxer_context_t *) wrapper->parent;
    dtp_media_info_t *media_info = &dem_ctx->media_info;
    track_info_t *tracks = &(media_info->tracks);
    demuxer_statistics_info_t *p_statistics_info = &dem_ctx->statistics_info;

    int has_audio = (media_info->disable_audio) ? 0 : media_info->has_audio;
    int has_video = (media_info->disable_video) ? 0 : media_info->has_video;
    int has_sub = (media_info->disable_sub) ? 0 : media_info->has_sub;

    int cur_aidx = (has_audio) ?
                   tracks->astreams[media_info->cur_ast_index]->index : -1;
    int cur_vidx = (has_video) ?
                   tracks->vstreams[media_info->cur_vst_index]->index : -1;
    int cur_sidx = (has_sub) ? tracks->sstreams[media_info->cur_sst_index]->index :
                   -1;

    ffmpeg_ctx_t *ctx = (ffmpeg_ctx_t *)wrapper->demuxer_priv;
    AVFormatContext *ic = ctx->ic;
    AVPacket avpkt;
    int64_t exchange_pts;
    int ret = av_read_frame(ic, &avpkt);
    if (ret < 0) {
        if (AVERROR(EAGAIN) != ret) {
            /*if the return is EAGAIN,we need to try more times */
            dt_info(TAG, "[%s:%d]av_read_frame return(%d)\n", __FUNCTION__, __LINE__, ret);

            if (AVERROR_EOF == ret || ret == -1) {
                return DTERROR_READ_EOF;
            } else {
                return DTERROR_READ_FAILED;
            }
        }
        return DTERROR_READ_AGAIN;
    }
    dt_av_pkt_t *frame = dtp_packet_alloc();
    if (!frame) {
        dt_error(TAG, "[%s:%d] out of memory\n", __FUNCTION__, __LINE__);
        return DTERROR_READ_FAILED;
    }
    //read frame ok
    if (has_video && cur_vidx == avpkt.stream_index) {
        frame->type = AVMEDIA_TYPE_VIDEO;
    } else if (has_audio && cur_aidx == avpkt.stream_index) {
        frame->type = AVMEDIA_TYPE_AUDIO;
    } else if (has_sub && cur_sidx == avpkt.stream_index) {
        frame->type = AVMEDIA_TYPE_SUBTITLE;
    } else {
        frame->type = AVMEDIA_TYPE_UNKNOWN;
        av_free_packet(&avpkt);
        return DTERROR_READ_AGAIN; // need to read again
    }

    //setup frame
    frame->data = avpkt.data;
    frame->size = avpkt.size;
    exchange_pts = pts_exchange(&avpkt, media_info);
    frame->pts = avpkt.pts;
    frame->dts = avpkt.dts;
    frame->duration = avpkt.duration;
    frame->key_frame = avpkt.flags & AV_PKT_FLAG_KEY;
    if (frame->type == (int)AVMEDIA_TYPE_AUDIO) {
        p_statistics_info->audio_frame_count++;
        dt_debug(TAG,
                 "GET AUDIO FRAME, pts:0x%llx dts:0x%llx time:%lld exchange_pts:%llx time_ex:%lld offset:0x%llx size:%d stream_index:%d\n",
                 frame->pts, frame->dts, frame->pts / 90000, exchange_pts,
                 exchange_pts / 90000, p_statistics_info->a_offset, frame->size,
                 avpkt.stream_index);
        p_statistics_info->a_offset += frame->size;
    }
    if (frame->type == (int)AVMEDIA_TYPE_VIDEO) {
        p_statistics_info->video_frame_count++;
        if (frame->key_frame) {
            p_statistics_info->video_keyframe_count++;
        }
        dt_debug(TAG,
                 "GET VIDEO FRAME, pts:0x%llx dts:0x%llx time:%lld exchange_pts:%llx time_ex:%lld offset:0x%llx size:%d key:%d stream_index:%d \n",
                 frame->pts, frame->dts, frame->pts / 90000, exchange_pts,
                 exchange_pts / 90000, p_statistics_info->v_offset, frame->size,
                 frame->key_frame, avpkt.stream_index);
        p_statistics_info->v_offset += frame->size;
    }
    if (frame->type == (int)AVMEDIA_TYPE_SUBTITLE) {
        p_statistics_info->sub_frame_count++;
        dt_debug(TAG,
                 "GET SUB FRAME, pts:0x%llx dts:0x%llx size:%d time:%lld exchange_pts:%llx time_ex:%lld offset:0x%llx \n",
                 frame->pts, frame->dts, frame->size, frame->pts / 90000, \
                 exchange_pts, exchange_pts / 90000, p_statistics_info->s_offset);
        p_statistics_info->s_offset += frame->size;
    }

    frame->opaque = (void *)av_packet_clone(&avpkt);
    if (frame->opaque != NULL) {
        frame->flags |= DTP_PACKET_FLAG_FFMPEG;
    }

    av_packet_unref(&avpkt);

    *pkt = frame;
    //dt_info(TAG, "read ok,frame size:%d %02x %02x %02x %02x addr:%p type:%d\n", frame->size, frame->data[0], frame->data[1], frame->data[2], frame->data[3], frame->data,frame->type);
    //dt_debug(TAG, "SIDE_DATA_ELEMENT:%d \n", avpkt.side_data_elems);

    return DTERROR_NONE;
}

static int media_format_convert(const char *name)
{
    int i, j;
    int type = DTP_MEDIA_FORMAT_INVALID;
    j = sizeof(media_map) / sizeof(type_map_t);
    for (i = 0; i < j; i++) {
        if (strcmp(name, media_map[i].key) == 0) {
            break;
        }
    }
    if (i == j) {
        for (i = 0; i < j; i++) {
            if (strstr(name, media_map[i].key) != NULL) {
                break;
            }
            if (i == j) {
                dt_error("Unsupport media type %s\n", name);
                return DTP_MEDIA_FORMAT_INVALID;
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
    switch (id) {
    case AV_CODEC_ID_AAC:
        format = DT_AUDIO_FORMAT_AAC;
        break;
    case AV_CODEC_ID_AC3:
        format = DT_AUDIO_FORMAT_AC3;
        break;
    case AV_CODEC_ID_MP2:
        format = DT_AUDIO_FORMAT_MP2;
        break;
    case AV_CODEC_ID_MP3:
        format = DT_AUDIO_FORMAT_MP3;
        break;
    default:
        format = DT_AUDIO_FORMAT_UNKOWN;
        break;
    }
    dt_info(TAG, "[audio_format_convert]audio codec_id=0x%x format=%d\n", id,
            format);
    return format;
}

static int video_format_convert(enum AVCodecID id)
{
    dtvideo_format_t format;
    switch (id) {
    case AV_CODEC_ID_MPEG2VIDEO:
        format = DT_VIDEO_FORMAT_MPEG2;
        break;
    case AV_CODEC_ID_MPEG4:
        format = DT_VIDEO_FORMAT_MPEG4;
        break;
    case AV_CODEC_ID_H264:
        format = DT_VIDEO_FORMAT_H264;
        break;
    case AV_CODEC_ID_HEVC:
        format = DT_VIDEO_FORMAT_HEVC;
        break;
    default:
        format = DT_VIDEO_FORMAT_UNKOWN;
        break;
    }
    dt_info(TAG, "[video_format_convert]video codec_id=0x%x format=%d\n", id,
            format);
    return format;
}

static int subtitle_format_convert(enum AVCodecID id)
{

    dtsub_format_t format = DT_SUB_FORMAT_INVALID;
    switch (id) {
    case AV_CODEC_ID_DVD_SUBTITLE:
        format = DT_SUB_FORMAT_DVD_SUB;
        break;
    case AV_CODEC_ID_DVB_SUBTITLE:
        format = DT_SUB_FORMAT_DVB_SUB;
        break;
    default:
        format = DT_VIDEO_FORMAT_UNKOWN;
        break;
    }

    return format;
}

static int format2bps(int fmt)
{
    int ret;
    switch (fmt) {
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

static int is_live(char *url)
{
    if (strstr(url, "udp:") != NULL) {
        return 1;
    }
    if (strstr(url, "rtp:") != NULL) {
        return 1;
    }
    if (strstr(url, ".sdp") != NULL) {
        return 1;
    }
    return 0;
}

static int demuxer_ffmpeg_setup_info(demuxer_wrapper_t * wrapper,
                                     dtp_media_info_t * info)
{
    ffmpeg_ctx_t *ctx = (ffmpeg_ctx_t *)wrapper->demuxer_priv;
    AVFormatContext *ic = ctx->ic;
    AVStream *pStream;
    AVCodecContext *pCodec;
    vstream_info_t *vst_info;
    astream_info_t *ast_info;
    sstream_info_t *sst_info;
    int i, j;
    AVDictionaryEntry *t;
    track_info_t *tracks = &info->tracks;

    /*reset vars */
    memset(info, 0, sizeof(*info));
    //set cur stream index -1 ,other vars have been reset to 0
    info->cur_ast_index = -1;
    info->cur_vst_index = -1;
    info->cur_sst_index = -1;

    /*get media info */
    info->format = DTP_MEDIA_FORMAT_INVALID;
    info->bit_rate = ic->bit_rate;
    double duration = ((double) ic->duration / AV_TIME_BASE);
    info->duration = (int)(ic->duration / AV_TIME_BASE);
    info->file = ic->filename;
    info->livemode = is_live(info->file);

    dt_info(TAG, "file name:%s duration:%lf islive:%d\n", info->file, duration,
            info->livemode);
    info->file_size = avio_size(ic->pb);

    info->format = media_format_convert(ic->iformat->name);
    if (info->format == DTP_MEDIA_FORMAT_INVALID) {
        dt_warning(TAG, "get wrong media format\n");
    }
    info->start_time = ic->start_time * 9 / 100; // Need to convert from us->pts
    //get stream info
    for (i = 0; i < ic->nb_streams; i++) {
        pStream = ic->streams[i];
        pCodec = pStream->codec;
        if (pCodec->codec_type == AVMEDIA_TYPE_VIDEO) {

            //for some mp3 have video, just skip
            if (info->format == DTP_MEDIA_FORMAT_MP3) {
                continue;
            }

            vst_info = (vstream_info_t *) malloc(sizeof(vstream_info_t));
            memset(vst_info, 0, sizeof(vstream_info_t));
            vst_info->index = pStream->index;
            vst_info->id = pStream->id;
            vst_info->width = pCodec->width;
            vst_info->height = pCodec->height;
            vst_info->pix_fmt = pCodec->pix_fmt;
            vst_info->duration = (int64_t)(pStream->duration * pStream->time_base.num /
                                           pStream->time_base.den);
            vst_info->bit_rate = pCodec->bit_rate;
            vst_info->format = video_format_convert(pCodec->codec_id);

            AVRational ratio = av_guess_sample_aspect_ratio(ic, pStream, NULL);
            AVRational display = {0};

            av_reduce(&display.num, &display.den,
                      pCodec->width  * (int64_t)ratio.num,
                      pCodec->height * (int64_t)ratio.den,
                      1024 * 1024);
            vst_info->sample_aspect_ratio.num = display.num;
            vst_info->sample_aspect_ratio.den = display.den;

            ratio = av_guess_frame_rate(ic, pStream, NULL);
            vst_info->frame_rate_ratio.num = ratio.num;
            vst_info->frame_rate_ratio.den = ratio.den;

            vst_info->time_base.num = pStream->time_base.num;
            vst_info->time_base.den = pStream->time_base.den;

            vst_info->codec_priv = (void *) pCodec;
            tracks->vstreams[tracks->vst_num] = vst_info;
            tracks->vst_num++;
            t = av_dict_get(pStream->metadata, "language", NULL, 0);
            if (t) {
                strcpy(vst_info->language, t->value);
            } else {
                strcpy(vst_info->language, "und");
            }
            if (pCodec->extradata_size) {
                vst_info->extradata_size = pCodec->extradata_size;
                memcpy(vst_info->extradata, pCodec->extradata, pCodec->extradata_size);
            }
            // Extra data details
            dt_info(TAG, "VIDEO EXTRA DATA SIZE:%d - %d \n", pCodec->extradata_size,
                    vst_info->extradata_size);
            dt_info(TAG, "Extradata content:\n");
            if (pCodec->extradata_size > 0) {
                for (j = 0; j < pCodec->extradata_size && pCodec->extradata_size > 10; j += 10)
                    dt_info(TAG, "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                            pCodec->extradata[j],
                            pCodec->extradata[j + 1],
                            pCodec->extradata[j + 2],
                            pCodec->extradata[j + 3],
                            pCodec->extradata[j + 4],
                            pCodec->extradata[j + 5],
                            pCodec->extradata[j + 6],
                            pCodec->extradata[j + 7],
                            pCodec->extradata[j + 8],
                            pCodec->extradata[j + 9]
                           );
            }
            dt_info(TAG, "End\n");
#if 0
#if ENABLE_ANDROID
            //if(info->format == DTP_MEDIA_FORMAT_MPEGTS)
            // android no need extradata for H264
            if (pCodec->codec_id == AV_CODEC_ID_H264) {
                vst_info->extradata_size = 0;
                dt_info(TAG, "TS no need extradata \n");
            }
#endif
#endif
        } else if (pCodec->codec_type == AVMEDIA_TYPE_AUDIO) {
            if (pCodec->channels <= 0 || pCodec->sample_rate <= 0) {
                dt_info(TAG,
                        "Audio info invalid, channel:%d samplerate:%d \n",
                        pCodec->channels, pCodec->sample_rate);
            }
            ast_info = (astream_info_t *) malloc(sizeof(astream_info_t));
            memset(ast_info, 0, sizeof(astream_info_t));
            ast_info->index = pStream->index;
            ast_info->id = pStream->id;
            ast_info->channels = pCodec->channels;
            ast_info->sample_rate = pCodec->sample_rate;
            ast_info->bps = format2bps(pCodec->sample_fmt);
            ast_info->duration = (int64_t)(pStream->duration * pStream->time_base.num /
                                           pStream->time_base.den);
            ast_info->time_base.num = pStream->time_base.num;
            ast_info->time_base.den = pStream->time_base.den;
            ast_info->bit_rate = pCodec->bit_rate;
            t = av_dict_get(pStream->metadata, "language", NULL, 0);
            if (t) {
                strcpy(ast_info->language, t->value);
            } else {
                strcpy(ast_info->language, "und");
            }
            ast_info->format = audio_format_convert(pCodec->codec_id);
            ast_info->codec_priv = (void *)pCodec;
            tracks->astreams[tracks->ast_num] = ast_info;
            tracks->ast_num++;
        } else if (pCodec->codec_type == AVMEDIA_TYPE_SUBTITLE) {
            sst_info = (sstream_info_t *) malloc(sizeof(sstream_info_t));
            memset(sst_info, 0, sizeof(sstream_info_t));
            sst_info->index = pStream->index;
            sst_info->id = pStream->id;
            sst_info->width = pCodec->width;
            sst_info->height = pCodec->height;
            t = av_dict_get(pStream->metadata, "language", NULL, 0);
            if (t) {
                strcpy(sst_info->language, t->value);
            } else {
                strcpy(sst_info->language, "und");
            }
            sst_info->format = subtitle_format_convert(pCodec->codec_id);
            sst_info->codec_priv = (void *) pCodec;
            tracks->sstreams[tracks->sst_num] = sst_info;
            tracks->sst_num++;
            dt_info(TAG, "Extradata content:\n");
            if (pCodec->extradata_size > 0) {
                for (j = 0; j < pCodec->extradata_size && pCodec->extradata_size > 10; j += 10)
                    dt_info(TAG, "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                            pCodec->extradata[j],
                            pCodec->extradata[j + 1],
                            pCodec->extradata[j + 2],
                            pCodec->extradata[j + 3],
                            pCodec->extradata[j + 4],
                            pCodec->extradata[j + 5],
                            pCodec->extradata[j + 6],
                            pCodec->extradata[j + 7],
                            pCodec->extradata[j + 8],
                            pCodec->extradata[j + 9]
                           );
            }
            dt_info(TAG, "End\n");


        }
    }
    if (tracks->vst_num > 0) {
        info->has_video = 1;
        info->cur_vst_index = 0;
    }
    if (tracks->ast_num > 0) {
        info->has_audio = 1;
        info->cur_ast_index = 0;
    }
    if (tracks->sst_num > 0) {
        info->has_sub = 1;
        info->cur_sst_index = 0;
    }
    return 0;
}

static int demuxer_ffmpeg_seek_frame(demuxer_wrapper_t * wrapper,
                                     int64_t timestamp)
{
    ffmpeg_ctx_t *ctx = (ffmpeg_ctx_t *)wrapper->demuxer_priv;
    AVFormatContext *ic = ctx->ic;
    dtdemuxer_context_t *dem_ctx = (dtdemuxer_context_t *) wrapper->parent;
    dtp_media_info_t *media_info = &dem_ctx->media_info;

    int seek_flags = 0; //seek key frame as default

    seek_flags = (dtp_setting.demuxer_seek_keyframe == 0) ? AVSEEK_FLAG_ANY :
                 AVSEEK_FLAG_BACKWARD;
    dt_info(TAG, "seek keyframe enabled:%d \n", dtp_setting.demuxer_seek_keyframe);

    // when seek to 0 or end, set backward flag
    int64_t duration = ((double) ic->duration / AV_TIME_BASE);
    int64_t s_time = timestamp;
    if (duration > 0 && (s_time <= 1 || s_time >= duration - 10)) {
        seek_flags = AVSEEK_FLAG_BACKWARD;
        dt_info(TAG, "seek to head or tail, set backward flag\n");
    }
#define MAX_DURATION_S 10*3600

    int seek_mode = 0; // default by time

    if (duration >= MAX_DURATION_S) {
        seek_mode = 1;
    }
    if (dtp_setting.player_seekmode != -1) {
        seek_mode = dtp_setting.player_seekmode;
    } else {
        // some dynamic control
        if (media_info->format == DTP_MEDIA_FORMAT_MPEGTS) {
            seek_mode = 1;
        }
    }

    // fix seek by bytes
    if (seek_mode == 1) {
        seek_flags = AVSEEK_FLAG_BYTE;
        timestamp = (int64_t)(media_info->file_size * (double)s_time / duration);
        dt_info(TAG, "Fixed seek by bytes. Duration(%lld s) seektopos:%lld\n", duration,
                timestamp);
    } else {
        // seek by time
        timestamp = timestamp * AV_TIME_BASE + ic->start_time;
    }

#if 0
    int64_t seek_target = timestamp;
    int64_t seek_min = (seek_target > 0) ? seek_target - timestamp + 2 : INT64_MIN;
    int64_t seek_max = (seek_target < 0) ? seek_target - timestamp - 2 : INT64_MAX;
    int64_t ret = avformat_seek_file(ic, -1, seek_min, seek_target, seek_max,
                                     seek_flags);
#else
    dt_info(TAG, "seekto: %lld (%lld s) duration:%lld seek_flags:%d \n", timestamp,
            s_time, duration, seek_flags);
    int64_t ret = av_seek_frame(ic, -1, timestamp, seek_flags);

#endif
    if (ret >= 0) {
        dt_info(TAG, "AV_FORMAT_SEEK_FILE OK \n");
        return 0;
    }

    dt_info(TAG, "AV_FORMAT_SEEK_FILE FAIL \n");
    return -1;
}

static void dump_demuxer_statics_info(dtdemuxer_context_t *dem_ctx)
{
    demuxer_statistics_info_t *p_info = &dem_ctx->statistics_info;
    dt_info(TAG, "==============demuxer statistics info============== \n");
    dt_info(TAG, "AudioFrameCount: %d  size:%lld \n", p_info->audio_frame_count,
            p_info->a_offset);
    dt_info(TAG, "VideoFrameCount: %d Key:%d size:%lld \n",
            p_info->video_frame_count, p_info->video_keyframe_count, p_info->v_offset);
    dt_info(TAG, "SubFrameCount: %d size:%lld \n", p_info->sub_frame_count,
            p_info->s_offset);
    dt_info(TAG, "=================================================== \n");
}

static int demuxer_ffmpeg_close(demuxer_wrapper_t * wrapper)
{
    dtdemuxer_context_t *dem_ctx = (dtdemuxer_context_t *) wrapper->parent;
    ffmpeg_ctx_t *ctx = (ffmpeg_ctx_t *)wrapper->demuxer_priv;
    AVFormatContext *ic = ctx->ic;

    dump_demuxer_statics_info(dem_ctx);
    if (ic) {
        avformat_close_input(&ic);
    }
    if (ctx->bsfc) {
        av_bitstream_filter_close(ctx->bsfc);
    }
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
