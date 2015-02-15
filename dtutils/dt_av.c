#include "dt_av.h"

void *dt_malloc(size_t size)
{
    void *ptr = malloc(size);
    if(!ptr) return ptr;
    memset(ptr, 0, size);
    return ptr;
}

void dt_free(void *ptr)
{
    free(ptr);
}

// structure

dt_av_frame_t *dtav_new_frame()
{
    dt_av_frame_t *frame = (dt_av_frame_t *)malloc(sizeof(dt_av_frame_t));
    return frame;
}

int dtav_unref_frame(dt_av_frame_t *frame)
{
    if(!frame)
        return 0;
    if (frame->data)
        free (frame->data[0]);
    return 0;
}

int dtav_free_frame(dt_av_frame_t *frame)
{
    if(!frame)
        return 0;
    if (frame->data)
        free (frame->data[0]);
    free(frame);
    return 0;
}

void dtav_clear_frame(void *pic)
{
    dt_av_frame_t *picture = (dt_av_frame_t *) (pic);
    if (picture->data)
        free (picture->data[0]);
    return;
}

// av convert
const char *dt_mediafmt2str(dtmedia_format_t format)
{
    switch(format){
    case DT_MEDIA_FORMAT_MPEGTS:
        return "mpegts";
    case DT_MEDIA_FORMAT_MPEGPS:
        return "mpegps";
    case DT_MEDIA_FORMAT_RM:
        return "rmvb";
    case DT_MEDIA_FORMAT_AVI:
        return "avi";
    case DT_MEDIA_FORMAT_MKV:
        return "mkv";
    case DT_MEDIA_FORMAT_MOV:
        return "mov,mp4,etc";
    case DT_MEDIA_FORMAT_FLV:
        return "flv";
    case DT_MEDIA_FORMAT_AAC:
        return "audio-aac";
    case DT_MEDIA_FORMAT_AC3:
        return "audio-ac3";
    case DT_MEDIA_FORMAT_MP3:
        return "audio-mp3";
    case DT_MEDIA_FORMAT_WAV:
        return "audio-wav";
    case DT_MEDIA_FORMAT_DTS:
        return "audio-dts";
    case DT_MEDIA_FORMAT_FLAC:
        return "audio-flac";
    case DT_MEDIA_FORMAT_H264:
        return "video-h264";
    case DT_MEDIA_FORMAT_AVS:
        return "avs";
    case DT_MEDIA_FORMAT_M2V:
        return "m2v";
    case DT_MEDIA_FORMAT_ASF:
        return "asf";
    case DT_MEDIA_FORMAT_APE:
        return "audio-ape";
    case DT_MEDIA_FORMAT_AMR:
        return "audio-amr";
    default:
        return "unkown";
    }
}

const char *dt_afmt2str(dtaudio_format_t format)
{
    switch(format){
    case DT_AUDIO_FORMAT_MP2:
        return "mp2";
    case DT_AUDIO_FORMAT_MP3:
        return "mp3";
    case DT_AUDIO_FORMAT_AAC:
        return "aac";
    case DT_AUDIO_FORMAT_AC3:
        return "ac3";
    default:
        return "und";
    }

    return "und";
}

const char *dt_vfmt2str(dtvideo_format_t format)
{
    switch(format){
    case DT_VIDEO_FORMAT_H264:
        return "h264";
    default:
        return "und";
    }

    return "und";
}

const char *dt_sfmt2str(dtsub_format_t format)
{
    switch(format){
    case DT_SUB_FORMAT_DVD_SUB:
        return "dvd-sub";
    case DT_SUB_FORMAT_DVB_SUB:
        return "dvb-sub";
    default:
        return "und";
    }

    return "und";
}
