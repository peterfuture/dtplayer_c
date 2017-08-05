#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dt_error.h"
#include "dt_macro.h"
#include "dt_rw.h"

#include "dtstream_api.h"
#include "dtdemuxer.h"
#include "demuxer_wrapper.h"

#define TAG "DEMUXER-AAC"

/*
 * adts fixed header:
 * syncword:                 12bit    FFF
 * ID:                       01bit
 * layer:                    02bit
 * protection_absent:        01bit
 * profile:                  02bit
 * sampling_frequency_index: 04bit
 * private_bit:              01bit
 * channel_configuration:    03bit
 * original_copy:            01bit
 * home:                     01bit
 *
 * */

/*
 *index  profile
 * 0     Main Profile
 * 1     Low Complexity profile(LC)
 * 2     Scalable Sampling Rate Profile(SSR)
 * 3     reserve
 *
 * */
static char *aac_profile[4] = {
    "MainProfile",
    "LC",
    "SSR",
    "Reserve"
};

/*
 *
 * Sampling_frequency_index   samplerate
 * 0                          96000
 * 1                          88200
 * 2                          64000
 * 3                          48000
 * 4                          44100
 * 5                          32000
 * 6                          24000
 * 7                          22050
 * 8                          16000
 * 9                          12000
 * 10                         11025
 * 11                         8000
 * 12                         7350
 * 13                         Reserved
 * 14                         Reserved
 * 15                         Frequency is written explicity
 *
 * */

int sr_index[16] = {
    96000,
    88200,
    64000,
    48000,
    44100,
    32000,
    24000,
    22050,
    16000,
    12000,
    11025,
    8000,
    7350,
    0,
    0,
    0
};


typedef struct {
    uint8_t buf[8];
    uint64_t size;
    float time;
    float last_pts;

    int bitrate;
    int channels;
    int samplerate;
    int profile;
    int bps;

    int64_t file_size;
    int duration;
} aac_ctx_t;

static int aac_parse_frame(uint8_t *buf, int *srate, int *num)
{
    int i = 0, sr, fl = 0;
    static const int srates[] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 0, 0, 0};

    if ((buf[i] != 0xFF) || ((buf[i + 1] & 0xF6) != 0xF0)) {
        return 0;
    }

    sr = (buf[i + 2] >> 2)  & 0x0F;
    if (sr > 11) {
        return 0;
    }
    *srate = srates[sr];

    fl = ((buf[i + 3] & 0x03) << 11) | (buf[i + 4] << 3) | ((
                buf[i + 5] >> 5) & 0x07);
    *num = (buf[i + 6] & 0x02) + 1;

    return fl;
}

static int demuxer_aac_probe(demuxer_wrapper_t *wrapper, dt_buffer_t *probe_buf)
{
    int score;
    int max_frames = 0, first_frames = 0;
    int fsize, frames;
    const uint8_t *buf0 = probe_buf->data;
    const uint8_t *buf2;
    const uint8_t *buf;
    const uint8_t *end = buf0 + probe_buf->level - 7;
    dt_info(TAG, "AAC Demuxer probe enter \n");
    buf = buf0;
    if (probe_buf->level < 10) {
        dt_info(TAG, "AAC Demuxer. level:%d \n", probe_buf->level);
        return 0;
    }

    for (; buf < end; buf = buf2 + 1) {
        buf2 = buf;

        for (frames = 0; buf2 < end; frames++) {
            uint32_t header = DT_RB16(buf2);
            if ((header & 0xFFF6) != 0xFFF0) {
                break;
            }
            fsize = (DT_RB32(buf2 + 3) >> 13) & 0x1FFF;
            if (fsize < 7) {
                break;
            }
            dt_debug(TAG, "FRAME SIZE:%d data:%x  \n", fsize, header);
            fsize = MIN(fsize, end - buf2);
            buf2 += fsize;
        }
        max_frames = MAX(max_frames, frames);
        if (buf == buf0) {
            first_frames = frames;
        }
    }
    if (first_frames >= 3) {
        score = 100 / 2 + 1;
    } else if (max_frames > 500) {
        score = 100 / 2;
    } else if (max_frames >= 3) {
        score = 100 / 4;
    } else if (max_frames >= 1) {
        score = 1;
    } else {
        score = 0;
    }
    dt_info(TAG, "score:%d frames:%d \n", score, frames);
    if (score >= 50) {
        return 1;
    } else {
        return 0;
    }
}

#define BUF_NOT_ENOUGH(b1,n,b2) (b1+n >= b2)
static int estimate_duration(demuxer_wrapper_t *wrapper)
{
    aac_ctx_t *aac_ctx = (aac_ctx_t *)wrapper->demuxer_priv;
    dtdemuxer_context_t *ctx = (dtdemuxer_context_t *)wrapper->parent;
    dt_buffer_t *probe_buf = &ctx->probe_buf;
    const uint8_t *buf = probe_buf->data;
    const uint8_t *buf2 = buf;
    const uint8_t *end = buf + probe_buf->level - 7;
    uint8_t c1, c2;
    int len, srate, num;
    float tm = 0;

    while (buf2 < end) {
        c1 = c2 = 0;
        while (c1 != 0xFF) {
            if (BUF_NOT_ENOUGH(buf2, 1, end)) {
                break;
            }
            c1 = DT_RB8(buf2);
            buf2++;
        }
        if (BUF_NOT_ENOUGH(buf2, 1, end)) {
            break;
        }
        c2 = DT_RB8(buf2);
        if ((c2 & 0xF6) != 0xF0) {
            continue;
        }
        buf2++;
        aac_ctx->buf[0] = (unsigned char) c1;
        aac_ctx->buf[1] = (unsigned char) c2;

        if (BUF_NOT_ENOUGH(buf2, 6, end)) {
            break;
        }
        memcpy(&(aac_ctx->buf[2]), buf2, 6);
        buf2 += 6;
        len = aac_parse_frame(aac_ctx->buf, &srate, &num);
        if (BUF_NOT_ENOUGH(buf2, len, end)) {
            break;
        }
        if (len > 0) {
            if (srate) {
                tm = (float)(num * 1024.0 / srate);
            }
            aac_ctx->last_pts += tm;
            aac_ctx->size += len;
            aac_ctx->time += tm;
            aac_ctx->bitrate = (int)(aac_ctx->size / aac_ctx->time);
            dt_debug(TAG, "READ ONE FRAME: len:%d size:%llu time:%f bit:%d\n", len,
                     aac_ctx->size, aac_ctx->time, aac_ctx->bitrate);
            buf2 += len - 8;
        } else {
            buf2 -= 6;
        }
    }
    aac_ctx->last_pts = 0;
    aac_ctx->size = 0;
    aac_ctx->time = 0;
    aac_ctx->duration = (aac_ctx->file_size > 0) ? (aac_ctx->file_size /
                        aac_ctx->bitrate) : -1;
    dt_info(TAG, "AAC ESTIMATE DURATION:bitrate:%d duration:%d \n",
            aac_ctx->bitrate, aac_ctx->duration);
    return 0;
}

static int demuxer_aac_open(demuxer_wrapper_t *wrapper)
{
    //for aac we can use probe buf directly
    aac_ctx_t *aac_ctx = (aac_ctx_t *)malloc(sizeof(aac_ctx_t));
    memset(aac_ctx, 0, sizeof(aac_ctx_t));
    wrapper->demuxer_priv = (void *)aac_ctx;
    dtdemuxer_context_t *ctx = (dtdemuxer_context_t *)wrapper->parent;
    dt_buffer_t *probe_buf = &ctx->probe_buf;
    const uint8_t *buf = probe_buf->data;
    //const uint8_t *end = buf + probe_buf->level - 7;
    uint32_t header = DT_RB16(buf);
    if ((header & 0xFFF6) != 0xFFF0) {
        dt_info(TAG, "NOT aac file\n");
        return -1;
    }
    int profile = (DT_RB8(buf + 2) >> 6) & 0xf;
    aac_ctx->profile = profile;
    dt_debug(TAG, "Profile:%s \n", aac_profile[profile]);
    int sr = (DT_RB8(buf + 2) >> 2) & 0xf;
    aac_ctx->samplerate = sr_index[sr];
    dt_debug(TAG, "Samplerate:%d \n", sr_index[sr]);
    int channel = (DT_RB16(buf + 2) >> 6) & 0x7;
    aac_ctx->channels = channel;
    dt_debug(TAG, "channel:%d \n", channel);
    aac_ctx->bps = 16;
    aac_ctx->file_size = dtstream_get_size(ctx->stream_priv);
    estimate_duration(wrapper);
    return 0;
}

static int demuxer_aac_setup_info(demuxer_wrapper_t * wrapper,
                                  dtp_media_info_t * info)
{
    dtdemuxer_context_t *ctx = (dtdemuxer_context_t *)(wrapper->parent);
    aac_ctx_t *aac_ctx = (aac_ctx_t *)wrapper->demuxer_priv;
    track_info_t *tracks = &info->tracks;
    /*reset vars */
    memset(info, 0, sizeof(*info));
    //set cur stream index -1 ,other vars have been reset to 0
    info->cur_ast_index = -1;
    info->cur_vst_index = -1;
    info->cur_sst_index = -1;

    /*get media info */
    info->format = DTP_MEDIA_FORMAT_AAC;
    info->file = ctx->para.file_name;
    info->bit_rate = aac_ctx->bitrate;
    info->duration = aac_ctx->duration;
    info->file_size = aac_ctx->file_size;

    astream_info_t *ast_info = (astream_info_t *) malloc(sizeof(astream_info_t));
    memset(ast_info, 0, sizeof(astream_info_t));
    ast_info->index = 0;
    ast_info->id = 0;
    ast_info->channels = aac_ctx->channels;
    ast_info->sample_rate = aac_ctx->samplerate;
    ast_info->bps = aac_ctx->bps;
    ast_info->duration = aac_ctx->duration;
    ast_info->time_base.num = -1;
    ast_info->time_base.den = -1;
    ast_info->bit_rate = aac_ctx->bitrate;
    ast_info->format = DT_AUDIO_FORMAT_AAC;
    ast_info->codec_priv = NULL;
    tracks->astreams[tracks->ast_num] = ast_info;
    tracks->ast_num++;

    info->has_audio = 1;
    info->cur_ast_index = 0;
    return 0;
}

static int demuxer_aac_read_frame(demuxer_wrapper_t *wrapper,
                                  dt_av_pkt_t *frame)
{
    dtdemuxer_context_t *dem_ctx = (dtdemuxer_context_t *) wrapper->parent;
    aac_ctx_t *aac_ctx = (aac_ctx_t *)wrapper->demuxer_priv;

    int len, srate, num;
    float tm = 0;
    int ret = 0;
    uint8_t c1, c2;
    if (dtstream_eof(dem_ctx->stream_priv)) {
        return DTERROR_READ_EOF;
    }

    /* find sync word */
    while (!dtstream_eof(dem_ctx->stream_priv)) {
        c1 = c2 = 0;
        while (c1 != 0xFF) {
            ret = dtstream_read(dem_ctx->stream_priv, &c1, 1);
            if (ret < 0) {
                return DTERROR_READ_FAILED;
            }
        }
        ret = dtstream_read(dem_ctx->stream_priv, &c2, 1);
        if (ret < 0) {
            return DTERROR_READ_FAILED;
        }
        if ((c2 & 0xF6) != 0xF0) {
            continue;
        }
        aac_ctx->buf[0] = (unsigned char) c1;
        aac_ctx->buf[1] = (unsigned char) c2;
        if (dtstream_read(dem_ctx->stream_priv, &(aac_ctx->buf[2]), 6) < 6) {
            return DTERROR_READ_FAILED;
        }

        len = aac_parse_frame(aac_ctx->buf, &srate, &num);
        if (len > 0) {

            uint8_t *data = malloc(len);
            if (!data) {
                dt_error(TAG, "read frame, NEW_PACKET(%d)FAILED\n", len);
                dtstream_skip(dem_ctx->stream_priv, -8);
                return DTERROR_READ_AGAIN;
            }
            memcpy(data, aac_ctx->buf, 8);
            dtstream_read(dem_ctx->stream_priv, &(data[8]), len - 8);
            if (srate) {
                tm = (float)(num * 1024.0 / srate);
            }
            aac_ctx->last_pts += tm;
            aac_ctx->size += len;
            aac_ctx->time += tm;
            aac_ctx->bitrate = (int)(aac_ctx->size / aac_ctx->time);
            dt_debug(TAG, "READ ONE FRAME: len:%d size:%llu time:%f bit:%d pts:%f \n", len,
                     aac_ctx->size, aac_ctx->time, aac_ctx->bitrate, aac_ctx->last_pts);
            //setup frame
            frame->type = DTP_MEDIA_TYPE_AUDIO;
            frame->data = data;
            frame->size = len;
            frame->pts = (int64_t)(aac_ctx->last_pts * 90000);
            frame->dts = -1;
            frame->duration = -1;

            return DTERROR_NONE;
        } else {
            dtstream_skip(dem_ctx->stream_priv, -6);
        }
    }
    return DTERROR_READ_EOF;
}

/*
 * timestamp , us
 * */
static int demuxer_aac_seek_frame(demuxer_wrapper_t *wrapper, int64_t timestamp)
{
    dtdemuxer_context_t *dem_ctx = (dtdemuxer_context_t *) wrapper->parent;
    aac_ctx_t *aac_ctx = (aac_ctx_t *)wrapper->demuxer_priv;

    float time;

    time = timestamp;
    if (time < 0) {
        return -1;
    }

    aac_ctx->last_pts = 0;
    dtstream_seek(dem_ctx->stream_priv, 0, SEEK_SET);

    int len, nf, srate, num;
    nf = time / 1000000 * aac_ctx->samplerate / 1024;

    while (nf > 0) {
        if (dtstream_read(dem_ctx->stream_priv, aac_ctx->buf, 8) < 8) {
            break;
        }
        len = aac_parse_frame(aac_ctx->buf, &srate, &num);
        if (len <= 0) {
            dtstream_skip(dem_ctx->stream_priv, -7);
            continue;
        }
        dtstream_skip(dem_ctx->stream_priv, len - 8);
        aac_ctx->last_pts += (float)(num * 1024.0 / srate);
        nf -= num;
    }
    dt_info(TAG, "aac demuxer seek finish, last_time:%f \n", aac_ctx->last_pts);
    return 0;
}

static int demuxer_aac_close(demuxer_wrapper_t *wrapper)
{
    return 0;
}

demuxer_wrapper_t demuxer_aac = {
    .name = "aac demuxer",
    .id = DEMUXER_AAC,
    .probe = demuxer_aac_probe,
    .open = demuxer_aac_open,
    .read_frame = demuxer_aac_read_frame,
    .setup_info = demuxer_aac_setup_info,
    .seek_frame = demuxer_aac_seek_frame,
    .close = demuxer_aac_close
};
