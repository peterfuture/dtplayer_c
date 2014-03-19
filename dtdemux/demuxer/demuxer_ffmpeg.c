#include "../dtdemuxer.h"
#include "dt_error.h"

#include "libavformat/avformat.h"
#define TAG "DEMUX-FFMPEG"

#include <string.h>

typedef struct{
    char *key;
    int value;
}type_map_t;

static const type_map_t media_map[] = {
	{"mpegts", MEDIA_FORMAT_MPEGTS},
	{"mpeg", MEDIA_FORMAT_MPEGPS},
	{"rm", MEDIA_FORMAT_RM},
	{"avi", MEDIA_FORMAT_AVI},
	{"mkv", MEDIA_FORMAT_MKV},
	{"matroska", MEDIA_FORMAT_MKV},
	{"mov", MEDIA_FORMAT_MOV},
	{"mp4", MEDIA_FORMAT_MP4},
	{"flv", MEDIA_FORMAT_FLV},
	{"aac", MEDIA_FORMAT_AAC},
	{"ac3", MEDIA_FORMAT_AC3},
	{"mp3", MEDIA_FORMAT_MP3},
	{"mp2", MEDIA_FORMAT_MP3},
	{"wav", MEDIA_FORMAT_WAV},
	{"dts", MEDIA_FORMAT_DTS},
	{"flac", MEDIA_FORMAT_FLAC},
	{"h264", MEDIA_FORMAT_H264},
	{"cavs", MEDIA_FORMAT_AVS},
	{"mpegvideo", MEDIA_FORMAT_M2V},
	{"p2p", MEDIA_FORMAT_P2P},
	{"asf", MEDIA_FORMAT_ASF},
	{"m4a", MEDIA_FORMAT_MP4},
	{"m4v", MEDIA_FORMAT_MP4},
	{"rtsp", MEDIA_FORMAT_RTSP},
	{"ape", MEDIA_FORMAT_APE},
	{"amr", MEDIA_FORMAT_AMR},
};

static int demuxer_ffmpeg_open(demuxer_wrapper *wrapper,char *file_name, void *parent)
{
	AVFormatContext *ic = NULL;
	int err,ret;

	av_register_all();
    err = avformat_open_input(&ic,file_name,NULL, NULL);
	if (err < 0) 
        return -1;
	
	dt_info(TAG,"[%s:%d] avformat_open_input ok\n", __FUNCTION__, __LINE__);
    wrapper->demuxer_priv = (void *)ic;

	err = avformat_find_stream_info(ic, NULL);
	if (err < 0) {
		dt_error(TAG, "%s: could not find codec parameters\n",file_name);
		ret = -1;
		goto FAIL;
	}

	dt_info(TAG,"[%s:%d] start_time:%lld \n", __FUNCTION__, __LINE__,ic->start_time);
    wrapper->parent = parent;
    return 0;
FAIL:
    avformat_close_input(&ic);
    return ret;
}

static int64_t pts_exchange(AVPacket *avpkt, dt_media_info_t *media_info)
{
    double exchange = 1;
    int num,den;
    int has_video = media_info->has_video;
	int has_audio = media_info->has_audio;
	//int has_sub = media_info->has_sub;

    int cur_vidx = (has_video)?media_info->vstreams[media_info->cur_vst_index]->index:-1;
	int cur_aidx = (has_audio)?media_info->astreams[media_info->cur_ast_index]->index:-1;
	//int cur_sidx = (has_sub)?media_info->sstreams[media_info->cur_sst_index]->index:-1;

    if (has_video && cur_vidx == avpkt->stream_index) 
    {
        num = media_info->vstreams[media_info->cur_vst_index]->time_base.num;
        den = media_info->vstreams[media_info->cur_vst_index]->time_base.den;
		exchange = 90000 * num/den;
    }
    else if (has_audio && cur_aidx == avpkt->stream_index)
    {
        num = media_info->astreams[media_info->cur_ast_index]->time_base.num;
        den = media_info->astreams[media_info->cur_ast_index]->time_base.den;
		exchange = 90000 * (double)num/den;
    }
   
    return (int64_t)(avpkt->pts * exchange);
}

static int demuxer_ffmpeg_read_frame(demuxer_wrapper *wrapper,dt_av_frame_t *frame)
{
    dtdemuxer_context_t *dem_ctx = (dtdemuxer_context_t *)wrapper->parent;
    dt_media_info_t *media_info = &dem_ctx->media_info;
	
    int has_audio = (media_info->no_audio)?0:media_info->has_audio;
    int has_video = (media_info->no_video)?0:media_info->has_video;
	int has_sub = (media_info->no_sub)?0:media_info->has_sub;

	int cur_aidx = (has_audio)?media_info->astreams[media_info->cur_ast_index]->index:-1;
    int cur_vidx = (has_video)?media_info->vstreams[media_info->cur_vst_index]->index:-1;
	int cur_sidx = (has_sub)?media_info->sstreams[media_info->cur_sst_index]->index:-1;

    AVFormatContext *ic = (AVFormatContext *)wrapper->demuxer_priv;
	AVPacket avpkt;
	int ret = av_read_frame(ic, &avpkt);
	if (ret < 0) 
    {
		if (AVERROR(EAGAIN) != ret) {
			/*if the return is EAGAIN,we need to try more times */
			dt_warning(TAG,"[%s:%d]av_read_frame return (%d)\n", __FUNCTION__, __LINE__, ret);
			
            if (AVERROR_EOF != ret) 
				return DTERROR_READ_FAILED;
            else
				return DTERROR_READ_EOF;
		} 
		return DTERROR_READ_AGAIN;
    }   
    //read frame ok
	if (has_video && cur_vidx == avpkt.stream_index) 
		frame->type = AVMEDIA_TYPE_VIDEO;
    else if (has_audio && cur_aidx == avpkt.stream_index)
		frame->type = AVMEDIA_TYPE_AUDIO;
	else if (has_sub && cur_sidx == avpkt.stream_index) 
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
	frame->pts = pts_exchange(&avpkt,media_info);
	frame->dts = avpkt.dts;
	frame->duration = avpkt.duration;
	dt_debug(TAG,"read ok,frame size:%d %02x %02x %02x %02x addr:%p\n",frame->size,frame->data[0],frame->data[1],frame->data[2],frame->data[3],frame->data);	
    dt_debug(TAG,"SIDE_DATA_ELEMENT:%d \n",avpkt.side_data_elems);

	return DTERROR_NONE;
}

static int media_format_convert(const char * name)
{
    int i, j;
    int type = MEDIA_FORMAT_INVALID;
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
				return MEDIA_FORMAT_INVALID;
			}
		}
	}
    type = media_map[i].value;
	dt_info(TAG,"name:%s media_type=%d \n",name,type);
	return type;

}

audio_format_t audio_format_convert(enum AVCodecID id)
{
	audio_format_t format = AUDIO_FORMAT_INVALID;
    switch (id) {
	default:
		format = AUDIO_FORMAT_UNKOWN;
        break;
	}
	dt_info(TAG,"[audio_format_convert]audio codec_id=0x%x format=%d\n",id,format);
	return format;
}


static int video_format_convert(enum AVCodecID id)
{
	video_format_t format;
    switch (id) {
	    default:
		format = VIDEO_FORMAT_UNKOWN;
	}
	dt_info(TAG,"[video_format_convert]video codec_id=0x%x format=%d\n",id,format);
	return format;
}

static int subtitle_format_convert(int id)
{
    return SUBTITLE_FORMAT_UNKOWN;

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
    dt_info(TAG,"FORMAT2BPS FMT:%d bps:%d \n",fmt,ret);
    return ret;
}

static int demuxer_ffmpeg_setup_info(demuxer_wrapper * wrapper,dt_media_info_t * info)
{
	AVFormatContext *ic = (AVFormatContext *)wrapper->demuxer_priv;
    AVStream *pStream;
    AVCodecContext *pCodec;
    vstream_info_t *vst_info; 
    astream_info_t *ast_info; 
    sstream_info_t *sst_info; 
    int i;

    /*reset vars*/
    memset(info,0,sizeof(*info));
    //set cur stream index -1 ,other vars have been reset to 0
    info->cur_ast_index = -1;
    info->cur_vst_index = -1;
    info->cur_sst_index = -1;

    /*get media info*/
    info->format = MEDIA_FORMAT_INVALID;
    info->bit_rate = ic->bit_rate;
    double duration = ((double)ic->duration/AV_TIME_BASE);
    info->duration = (int)(ic->duration/AV_TIME_BASE);
    strcpy(info->file_name,ic->filename);
    dt_info(TAG,"file name:%s duration:%lf\n",info->file_name,duration);
    info->file_size = avio_size(ic->pb);

    info->format = media_format_convert(ic->iformat->name);
    if(info->format == MEDIA_FORMAT_INVALID)
        dt_warning(TAG,"get wrong media format\n");
    
    //get stream info
    for (i = 0; i < ic->nb_streams; i++) {
		pStream = ic->streams[i];
		pCodec = pStream->codec;
		if (pCodec->codec_type == AVMEDIA_TYPE_VIDEO) {
            vst_info = (vstream_info_t *)malloc(sizeof(vstream_info_t));
            memset(vst_info,0,sizeof(vstream_info_t));
            vst_info->index = pStream->index;
            vst_info->id = pStream->id;
            vst_info->width = pCodec->width;
            vst_info->height = pCodec->height;
            vst_info->pix_fmt = pCodec->pix_fmt;
            vst_info->duration = (int64_t)(pStream->duration * pStream->time_base.num / pStream->time_base.den);
			vst_info->bit_rate = pCodec->bit_rate;
            vst_info->format = video_format_convert(pCodec->codec_id);
		    vst_info->sample_aspect_ratio.num = pStream->sample_aspect_ratio.num; 
		    vst_info->sample_aspect_ratio.den = pStream->sample_aspect_ratio.den;
		    vst_info->frame_rate_ratio.num = pStream->r_frame_rate.num; 
		    vst_info->frame_rate_ratio.den = pStream->r_frame_rate.den; 
            vst_info->time_base.num = pStream->time_base.num;
            vst_info->time_base.den = pStream->time_base.den;
            vst_info->codec_priv = (void *)pCodec;
            info->vstreams[info->vst_num] = vst_info;    
			info->vst_num++;
        } else if (pCodec->codec_type == AVMEDIA_TYPE_AUDIO) {
		    ast_info = (astream_info_t *)malloc(sizeof(astream_info_t));
            memset(ast_info,0,sizeof(astream_info_t));
            ast_info->index = pStream->index;
            ast_info->id = pStream->id;
            ast_info->channels = pCodec->channels;
            ast_info->sample_rate = pCodec->sample_rate;
            ast_info->bps = format2bps(pCodec->sample_fmt);
            ast_info->duration = (int64_t)(pStream->duration * pStream->time_base.num / pStream->time_base.den);
            ast_info->time_base.num = pStream->time_base.num;
            ast_info->time_base.den = pStream->time_base.den;
            ast_info->bit_rate = pCodec->bit_rate;
            ast_info->format = audio_format_convert(pCodec->codec_id);
            ast_info->codec_priv = (void *)pCodec;
            info->astreams[info->ast_num] = ast_info;    
            info->ast_num++;
		} else if (pCodec->codec_type == AVMEDIA_TYPE_SUBTITLE) {
		    sst_info = (sstream_info_t *)malloc(sizeof(sstream_info_t));
            memset(sst_info,0,sizeof(sstream_info_t));
            sst_info->index = pStream->index;
            sst_info->id = pStream->id;
            sst_info->width = pCodec->width;
            sst_info->height = pCodec->height;
            sst_info->format = subtitle_format_convert(pCodec->codec_id);
            sst_info->codec_priv = (void *)pCodec;
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

static int demuxer_ffmpeg_seek_frame(demuxer_wrapper *wrapper,int timestamp)
{
    AVFormatContext *ic = (AVFormatContext *)wrapper->demuxer_priv;
    int seek_flags = AVSEEK_FLAG_ANY; 

    int64_t seek_target = timestamp * 1000000;
    int64_t seek_min = (seek_target > 0)?seek_target - timestamp +2 :INT64_MIN; 
    int64_t seek_max = (seek_target < 0)?seek_target - timestamp -2 :INT64_MAX; 
    int64_t ret = avformat_seek_file(ic,-1,seek_min,seek_target,seek_max,seek_flags);

    if(ret >= 0)
    {
        dt_info(TAG,"AV_FORMAT_SEEK_FILE OK \n");
        return 0;
    }

    dt_info(TAG,"AV_FORMAT_SEEK_FILE FAIL \n");
    return -1;
}


static int demuxer_ffmpeg_close(demuxer_wrapper *wrapper)
{
    AVFormatContext *ic = (AVFormatContext *)wrapper->demuxer_priv;
    avformat_close_input(&ic);
    return 0;
}

demuxer_wrapper demuxer_ffmpeg = {
    .name = "ffmpeg demuxer",
    .open = demuxer_ffmpeg_open,
    .read_frame = demuxer_ffmpeg_read_frame,
    .setup_info = demuxer_ffmpeg_setup_info,
    .seek_frame = demuxer_ffmpeg_seek_frame,
    .close = demuxer_ffmpeg_close
};
