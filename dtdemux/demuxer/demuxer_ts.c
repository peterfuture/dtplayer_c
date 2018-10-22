#include "dtdemuxer.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ts/tsdemux.h"
#include "ts/p_tsdemux.h"

#define TAG "DEMUXER-TS"

#define TS_CACHE_AUDIO 1024*10
#define TS_CACHE_VIDEO 1024*100
#define TS_CACHE_SUB   1024

#define PES_START_SIZE 6
#define PES_HEADER_SIZE 9
#define MAX_PES_HEADER_SIZE (9+255)

#define MAX_PES_PAYLOAD 200*1024
#define PES_PADDING_SIZE 16
enum {
    TS_INVLAID = -1,
    TS_HEADER = 0,
    TS_PESHEADER,
    TS_PESHEADER_FILL,
    TS_PAYLOAD,
    TS_SKIP
};

typedef struct SL_ConfigDescr {
    int use_au_start;
    int use_au_end;
    int use_rand_acc_pt;
    int use_padding;
    int use_timestamps;
    int use_idle;
    int timestamp_res;
    int timestamp_len;
    int ocr_len;
    int au_len;
    int inst_bitrate_len;
    int degr_prior_len;
    int au_seq_num_len;
    int packet_seq_num_len;
} SL_ConfigDescr;

typedef struct {
    dt_buffer_t dbt;
    int state;
    int data_index;
    int pkt_pos;


    int64_t pts, dts;

    int pid;
    int total_size;
    int pes_header_size;
    int extended_stream_id;
    uint8_t header[MAX_PES_HEADER_SIZE];
    uint8_t *buffer;
    int stream_type;
    SL_ConfigDescr sl;
} pes_t;

typedef struct {
    ts_stream_t *stream;
    ts_pidinfo_t *es_info[100];//a-v-s stream

    int es_num;
    int audio_num;
    int video_num;
    int sub_num;

    int bitrate;
    int64_t filesize;
    int duration;

    int cur_apts;
    int cur_vpts;

    //a-v-s cache
    pes_t es_audio;
    pes_t es_video;
    pes_t es_sub;
} ts_ctx_t;

static int dumptable(ts_stream_t *stream, ts_table_t *table, int complete);

static int ts_probe(demuxer_wrapper_t *wrapper, dt_buffer_t *probe_buf)
{
    const uint8_t *buf = probe_buf->data;
    const uint8_t *end = buf + probe_buf->level - 7;

    if (probe_buf->level < 10) {
        dt_info(TAG, "[%s:%d] buf level:%d too low\n", __FUNCTION__, __LINE__,
                probe_buf->level);
        return 0;
    }

    dt_info(TAG, "[%s:%d] buf level:%d. %02x\n", __FUNCTION__, __LINE__,
            probe_buf->level, buf[0]);
    int retry_times = 100;
    for (; buf < end; buf++) {
        uint32_t header = DT_RB8(buf);
        if ((header & 0xFF) != 0x47) {
            if (retry_times-- == 0) {
                dt_info(TAG, "[%s:%d] sync header not found\n", __FUNCTION__, __LINE__);
                return 0;
            }
            continue;
        }
        //found 0x47
        if (buf + 188 > end) {
            dt_info(TAG, "[%s:%d] buffer lev too low\n", __FUNCTION__, __LINE__);
            return 0;
        }
        header = DT_RB8(buf + 188);
        if ((header & 0xFF) == 0x47) {
            dt_info(TAG, "ts detect \n");
            return 1;
        } else {
            dt_info(TAG, "[%s:%d] not ts format\n", __FUNCTION__, __LINE__);
            return 0;
        }
    }
    return 0;
}

static int find_syncword(const uint8_t *buf, int max)
{
    int pos = 0;
    while (pos < max) {
        if (buf[pos] == 0x47) {
            return pos;
        }
        pos++;
    }
    return -1;
}

static int
dumpdvbnit(ts_stream_t *stream, ts_table_t *table, int complete)
{
    if (0 == complete && 1 == table->expected) {
        printf("  0x%04x - DVB Network Information Table 0x%02x (not yet defined)\n",
               (unsigned int) table->pid, table->tableid);
        return 0;
    }
    printf("  0x%04x - DVB Network Information Table 0x%02x\n",
           (unsigned int) table->pid, table->tableid);
    if (1 == table->expected) {
        if (1 == complete) {
            printf("  - Table defined but not present in stream\n");
        }
        return 0;
    }
    return 0;
}

static int
dumppmt(ts_stream_t *stream, ts_table_t *table, int complete)
{
    size_t c;
    ts_pidinfo_t *info;
    const ts_streamtype_t *stype;

    if (0 == complete && 1 == table->expected) {
        printf("  0x%04x - Program 0x%04x (not yet defined)\n",
               (unsigned int) table->pid, (unsigned int) table->progid);
        return 0;
    }
    printf("  0x%04x - Program 0x%04x\n", (unsigned int) table->pid,
           (unsigned int) table->progid);
    if (1 == table->expected) {
        if (1 == complete) {
            printf("  - Table defined but not present in stream\n");
        }
        return 0;
    }
    printf("  - Table contains details of %lu streams\n",
           (unsigned long) table->d.pmt.nes);
    for (c = 0; c < table->d.pmt.nes; c++) {
        info = table->d.pmt.es[c];
        printf("    0x%04x - ", info->pid);
        if (NULL == (stype = ts_typeinfo(info->stype))) {
            printf("Unknown stream type 0x%02x", info->stype);
        } else {
            printf("%s", stype->name);
        }
        switch (info->subtype) {
        case PST_UNSPEC:
            printf(" unspecified");
            break;
        case PST_VIDEO:
            printf(" video");
            break;
        case PST_AUDIO:
            printf(" audio");
            break;
        case PST_INTERACTIVE:
            printf(" interactive");
            break;
        case PST_CC:
            printf(" closed captioning");
            break;
        case PST_IP:
            printf(" Internet Protocol");
            break;
        case PST_SI:
            printf(" stream information");
            break;
        case PST_NI:
            printf(" network information");
            break;
        }
        switch (info->pidtype) {
        case PT_SECTIONS:
            printf(" sections");
            break;
        case PT_DATA:
            printf(" data");
            break;
        case PT_PES:
            printf(" PES");
            break;
        }
        if (1 == info->pcr) {
            printf(" (PCR)");
        }
        if (1 == complete && 0 == info->seen) {
            printf(" (defined but not present)");
        }
        putchar('\n');
    }
    return 0;
}

static int
dumppat(ts_stream_t *stream, ts_table_t *table, int complete)
{
    size_t c;

    printf("0x%04x - Program Association Table\n", (unsigned int) table->pid);
    if (1 == table->expected) {
        if (1 == complete) {
            printf("- Table defined but not present in stream\n");
        }
        return 0;
    }
    for (c = 0; c < table->d.pat.nprogs; c++) {
        dumptable(stream, table->d.pat.progs[c], complete);
    }
    return 0;
}

static int
dumptable(ts_stream_t *stream, ts_table_t *table, int complete)
{
    switch (table->tableid) {
    case TID_PAT:
        return dumppat(stream, table, complete);
    case TID_PMT:
        return dumppmt(stream, table, complete);
    case TID_DVB_NIT:
        return dumpdvbnit(stream, table, complete);
    default:
        printf("0x%04x - Unknown table ID 0x%02x\n", (unsigned int) table->pid,
               table->tableid);
    }
    return 0;
}

static int ts_type_convert(int type)
{
    int ret = -1;
    switch (type) {
    case ES_TYPE_H264:
        ret = DT_VIDEO_FORMAT_H264;
        break;
    case ES_TYPE_AAC:
        ret = DT_AUDIO_FORMAT_AAC;
    case ES_TYPE_AC3:
        ret = DT_AUDIO_FORMAT_AC3;

        break;
    default:
        break;
    }
    return ret;
}


static int ts_open(demuxer_wrapper_t *wrapper)
{
    int ret = 0;
    int complete = 1;
    ts_ctx_t *ts_ctx = (ts_ctx_t *)malloc(sizeof(ts_ctx_t));
    memset(ts_ctx, 0, sizeof(ts_ctx_t));
    ts_ctx->cur_apts = ts_ctx->cur_vpts = -1;
    wrapper->demuxer_priv = (void *)ts_ctx;
    dtdemuxer_context_t *ctx = (dtdemuxer_context_t *)wrapper->parent;
    dt_buffer_t *probe_buf = &ctx->probe_buf;

    ts_stream_t *stream;
    ts_options_t options;
    ts_packet_t packet;

    memset(&options, 0, sizeof(options));
    options.timecode = 0;
    options.autosync = 1;
    options.synclimit = 256;
    options.prepad = 0;
    options.postpad = 0;

    stream = ts_stream_create(&options);
    ts_ctx->stream = stream;

    const uint8_t *buf = probe_buf->data;
    const uint8_t *buf_end = probe_buf->data + probe_buf->level;
    int buf_size = probe_buf->level;
    int pos = find_syncword(buf, buf_size);
    if (pos == -1) {
        dt_info(TAG, "Cannot find sync word \n");
        return -1;
    }

    uint32_t header = DT_RB8(buf + pos);
    if ((header & 0xFF) != 0x47) {
        return -1;
    }

    while (1) {
        if (buf + pos + 188 >= buf_end) {
            break;
        }
        ret = ts_stream_read_packet(stream, &packet, buf + pos);
        if (ret == -1) {
            dt_info(TAG, "Parse one packet failed\n");
            break;
        }
        pos += 188;
    }

    //dump info
    if (stream->pat == NULL) {
        dt_info(TAG, "HEADER PARSE FAILED \n");
        return -1;
    }
    dumptable(stream, stream->pat, complete);

    //during demuxer open, need to get stream info,including
    dt_info(TAG, "START TO DETECT INFO\n");
    ts_table_t *tab_pat = stream->pat;
    if (1 == tab_pat->expected) {
        return -1;    //got nothing
    }
    int c = 0;
    int i = 0;
    for (i = 0; i < tab_pat->d.pat.nprogs; i++) {
        ts_table_t *table = tab_pat->d.pat.progs[i];
        ts_pidinfo_t *info;
        const ts_streamtype_t *stype;

        if (0 == complete && 1 == table->expected) {
            continue;
        }
        if (1 == table->expected) {
            continue;
        }

        for (c = 0; c < table->d.pmt.nes; c++) { // get es info
            info = table->d.pmt.es[c];
            if (NULL == (stype = ts_typeinfo(info->stype))) {
                continue;
            }

            switch (info->subtype) {
            case PST_UNSPEC:
                printf(" unspecified");
                break;
            case PST_VIDEO:
                ts_ctx->es_info[ts_ctx->es_num] = info;
                ts_ctx->es_num++;
                ts_ctx->audio_num++;
                break;
            case PST_AUDIO:
                ts_ctx->es_info[ts_ctx->es_num] = info;
                ts_ctx->es_num++;
                ts_ctx->video_num++;
                printf("\n");
                break;
            case PST_INTERACTIVE:
                printf(" interactive");
                break;
            case PST_CC:
                printf(" closed captioning");
                break;
            case PST_IP:
                printf(" Internet Protocol");
                break;
            case PST_SI:
                printf(" stream information");
                ts_ctx->es_info[ts_ctx->es_num] = info;
                ts_ctx->es_num++;
                printf("\n");
                break;
            case PST_NI:
                printf(" network information");
                break;
            }
        }
    }

    dt_info(TAG, "esnum:%d anum:%d vnum:%d \n", ts_ctx->es_num, ts_ctx->audio_num,
            ts_ctx->video_num);
    if (ts_ctx->audio_num > 0) {
        pes_t *aes = &ts_ctx->es_audio;
        aes->state = TS_INVLAID;
        buf_init(&aes->dbt, TS_CACHE_AUDIO);
    }
    if (ts_ctx->video_num > 0) {
        pes_t *ves = &ts_ctx->es_video;
        ves->state = TS_INVLAID;
        buf_init(&ves->dbt, TS_CACHE_VIDEO);
    }
    //step 3 parse es info
    ts_ctx->filesize = dtstream_get_size(ctx->stream_priv);
    ts_ctx->bitrate = -1; // invalid
    ts_ctx->duration = -1; // invalid

    int pid_tmp = -1;
    int idx = 0;
    ts_pidinfo_t *tmp;//a-v-s stream
    for (idx = 0; idx < ts_ctx->es_num; idx++) {
        tmp = ts_ctx->es_info[idx];
        if (tmp->subtype == PST_SI) {
            pid_tmp = tmp->pid;
            goto PID_FOUND;
        }
    }
    for (idx = 0; idx < ts_ctx->es_num; idx++) {
        tmp = ts_ctx->es_info[idx];

        if (tmp->subtype == PST_VIDEO) {
            pid_tmp = tmp->pid;
        }

        if (tmp->subtype == PST_AUDIO && ts_ctx->video_num == 0) {
            pid_tmp = tmp->pid;
            break;
        }
    }
PID_FOUND:
    if (pid_tmp == -1) {
        dt_error(TAG, "FAILED TO GET INFO\n");
        return -1;
    }
    dt_info(TAG, "PID:%x \n", pid_tmp);

    //find first pts
    pos = find_syncword(buf, buf_size);
    int64_t first_pts = -1;
    int64_t last_pts = -1;

    while (1) {
        if (buf[pos] != 0x47) {
            pos += find_syncword(buf + pos, buf_size - pos);
        }
        if (pos >= buf_size - 188) {
            break;
        }

        ret = ts_stream_fill_packet(stream, &packet, buf + pos);
        if (packet.pid == pid_tmp) {
            if (packet.pts == -1) {
                goto NEXT_PKT;
            }
            first_pts = packet.pts;
            dt_info(TAG, "Find first pcr :%lld %lld \n", packet.pts, first_pts);
            break;
            //demuxer->reference_clock = (double)pcr/(double)27000000.0;
        }
NEXT_PKT:
        pos += 188;
    }
    dt_info(TAG, "GET FIRST PTS:%lld \n", first_pts);
    if (first_pts == -1) {
        return 0;
    }
    //find last pts
    int read_size = MIN(ts_ctx->filesize, probe_buf->size);
    dt_info(TAG, "[%s:%d]:filesize :%lld probe size:%d readsize:%d\n", __FUNCTION__,
            __LINE__, ts_ctx->filesize, probe_buf->size, read_size);
    if (read_size <=  188 * 10) {
        dt_info(TAG, "[%s:%d]:readsize :%d less than 1880\n", __FUNCTION__, __LINE__,
                read_size);
        return -1;
    }
    pos = ts_ctx->filesize - read_size;
    ret = dtstream_seek(ctx->stream_priv, pos, SEEK_SET);
    if (ret < 0) {
        dt_info(TAG, "[%s:%d]:seek failed\n", __FUNCTION__, __LINE__);
        dtstream_seek(ctx->stream_priv, 0, SEEK_SET);
        return -1;
    }
    ret = dtstream_read(ctx->stream_priv, probe_buf->data, read_size);
    if (ret <= 0) {
        return -1;
    }
    buf = probe_buf->data;
    read_size = ret;
    dtstream_seek(ctx->stream_priv, 0, SEEK_SET);
    pos = 0;
    pos = find_syncword(buf, read_size);
    memset(&packet, 0, sizeof(packet));
    while (1) {
        if (buf[pos] != 0x47) {
            pos += find_syncword(buf + pos, read_size - pos);
        }
        if (pos >= read_size - 188) {
            break;
        }

        ret = ts_stream_fill_packet(stream, &packet, buf + pos);
        if (packet.pid == pid_tmp) {
            if (packet.pts == -1) {
                goto NEXT_PKT2;
            }
            last_pts = packet.pts;
            dt_debug(TAG, "Find last pcr :%lld %lld\n", packet.pts, last_pts);
            //demuxer->reference_clock = (double)pcr/(double)27000000.0;
        }
NEXT_PKT2:
        pos += 188;
    }
    dt_info(TAG, "GET LAST PTS:%lld \n", last_pts);
    if (last_pts == -1) {
        return 0;
    }

    ts_ctx->duration = (last_pts - first_pts) / 90000;
    if (ts_ctx->duration > 0) {
        ts_ctx->bitrate = ts_ctx->filesize / ts_ctx->duration;
    }

    dt_info(TAG, "Duration:%d bitrate:%d \n", ts_ctx->duration, ts_ctx->bitrate);
    return 0;
}

static int ts_setup_info(demuxer_wrapper_t * wrapper, dt_media_info_t * info)
{
    dtdemuxer_context_t *ctx = (dtdemuxer_context_t *)(wrapper->parent);
    ts_ctx_t *ts_ctx = (ts_ctx_t *)wrapper->demuxer_priv;

    vstream_info_t *vst_info;
    astream_info_t *ast_info;

    /*reset vars */
    memset(info, 0, sizeof(*info));
    //set cur stream index -1 ,other vars have been reset to 0
    info->cur_ast_index = -1;
    info->cur_vst_index = -1;
    info->cur_sst_index = -1;

    /*get media info */
    info->format = DT_MEDIA_FORMAT_MPEGTS;
    strcpy(info->file_name, ctx->file_name);

    info->bit_rate = ts_ctx->bitrate;
    info->duration = ts_ctx->duration;
    info->file_size = ts_ctx->filesize;

    int i = 0;
    for (i = 0; i < ts_ctx->es_num; i++) {
        ts_pidinfo_t *es = ts_ctx->es_info[i];
        if (es->subtype == PST_VIDEO) {
            vst_info = (vstream_info_t *) malloc(sizeof(vstream_info_t));
            memset(vst_info, 0, sizeof(vstream_info_t));
            vst_info->index = i;
            vst_info->id = es->pid;
            vst_info->width = -1; //invalid
            vst_info->height = -1;//invalid
            vst_info->pix_fmt = -1;
            vst_info->duration = info->duration;
            vst_info->bit_rate = -1;
            vst_info->format = ts_type_convert(es->stype);
            vst_info->codec_priv = NULL;
            info->vstreams[info->vst_num] = vst_info;
            info->vst_num++;
        }
        if (es->subtype == PST_AUDIO) {
            ast_info = (astream_info_t *) malloc(sizeof(astream_info_t));
            memset(ast_info, 0, sizeof(astream_info_t));
            ast_info->index = i;
            ast_info->id = es->pid;
            ast_info->channels = 2; // default
            ast_info->sample_rate = 48000; //default
            ast_info->bps = 16; // default
            ast_info->duration = info->duration;
            ast_info->bit_rate = -1;
            ast_info->format = ts_type_convert(es->stype);
            ast_info->codec_priv = NULL;
            info->astreams[info->ast_num] = ast_info;
            info->ast_num++;
        }
    }
    if (info->vst_num > 0) {
        info->has_video = 1;
        info->cur_vst_index = 0;
    }
    if (info->ast_num > 0) {
        info->has_audio = 1;
        info->cur_ast_index = 0;
    }
    if (info->sst_num > 0) {
        info->has_sub = 1;
        info->cur_sst_index = 0;
    }

    //set selcted stream pid
    ts_ctx->es_audio.pid = (info->has_audio) ?
                           info->astreams[info->cur_ast_index]->id : -1;
    ts_ctx->es_video.pid = (info->has_video) ?
                           info->vstreams[info->cur_vst_index]->id : -1;
    if (info->has_audio) {
        ts_ctx->es_audio.stream_type =
            ts_ctx->es_info[info->astreams[info->cur_ast_index]->index]->stype;
    }
    if (info->has_video) {
        ts_ctx->es_video.stream_type =
            ts_ctx->es_info[info->vstreams[info->cur_vst_index]->index]->stype;
    }
    dt_info(TAG, "audio STYPE:%d video stype:%d \n ", ts_ctx->es_audio.stream_type,
            ts_ctx->es_video.stream_type);
    return 0;
}

static int64_t parse_pes_pts(uint8_t *buf)
{
    return (int64_t)(*buf & 0x0e) << 29 | (DT_RB16(buf + 1) >> 1) << 15 | (DT_RB16(
                buf + 3) >> 1);
}

static int setup_frame(pes_t *es, dt_av_pkt_t *frame, int type)
{
    frame->type = type;
    frame->size = es->data_index;
    frame->data = es->buffer;
    frame->pts = es->pts;
    frame->dts = es->dts;
    frame->duration = -1;
    dt_debug(TAG, "GET ONEFRAME \n");

    es->data_index = 0;
    es->pts = es->dts = -1;
    es->buffer = NULL;
}

// 0 noerr 1 get one frame <0 err
static int handle_ts_pkt(ts_ctx_t *ts_ctx, ts_packet_t *packet,
                         dt_av_pkt_t *frame, int type)
{
    int ret = 0;
    int is_start = packet->unitstart;
    pes_t *es = NULL;
    if (type == DT_TYPE_AUDIO) {
        es = &ts_ctx->es_audio;
    } else {
        es = &ts_ctx->es_video;
    }

    //if first pkt is not frame start ,just skip
    if (!is_start && es->state == TS_INVLAID) {
        return 0;
    }
    if (is_start) {
        if (es->state == TS_PAYLOAD && es->data_index > 0) {
            setup_frame(es, frame, type);
            ret = 1;
        }

        es->pts = es->dts = -1;
        es->buffer = NULL;
        es->state = TS_HEADER;
        es->data_index = 0;
    }

    uint8_t *buf = packet->payload;
    int buf_size = packet->payloadlen;
    int len = 0;
    int code = 0;
    while (buf_size > 0) {
        switch (es->state) {
        case TS_HEADER:
            len = PES_START_SIZE - es->data_index;
            if (len > buf_size) {
                len = buf_size;
            }
            memcpy(es->header + es->data_index, buf, len);
            es->data_index += len;
            buf += len;
            buf_size -= len;
            if (es->data_index == PES_START_SIZE) {
                //we got all the pes or section header
                if (es->header[0] == 0x00 && es->header[1] == 0x00 && es->header[2] == 0x01) {
                    //it must be mpeg2 pes stream
                    code = es->header[3] | 0x100;
                    dt_debug(TAG, "PID:%x pes_code:%#x \n", es->pid, code);
                    if (code == 0x1be) { // padding stream
                        goto skip;
                    }
                    es->total_size = (int)DT_RB16(es->header + 4);
                    dt_debug(TAG, "TOTAL SIZE:%d \n", es->total_size);
                    //NOTE: A zero total size means the pes size is unbounded
                    if (!es->total_size) {
                        es->total_size = MAX_PES_PAYLOAD;
                    }
                    es->buffer = malloc(es->total_size);
                    if (es->buffer == NULL) { // err
                        return -1;
                    }
                    if (code != 0x1bc && code != 0x1bf && /* program_stream_map, private_stream_2 */
                        code != 0x1f0 && code != 0x1f1 && /* ECM, EMM */
                        code != 0x1ff && code != 0x1f2 && /* program_stream_directory, DSMCC_stream */
                        code != 0x1f8) {                  /* ITU-T Rec. H.222.1 type E stream */
                        es->state = TS_PESHEADER;
                        dt_debug(TAG, "ES HEADER PARSE OK,ENTER PESHEADER \n");
                    } else {
                        es->state      = TS_PAYLOAD;
                        dt_debug(TAG, "ES HEADER PARSE OK,ENTER PAYLOAD DIRECTLY \n");
                        es->data_index = 0;
                    }
                } else {
                    /* otherwise, it should be a table */
                    /* skip packet */
skip:
                    es->state = TS_SKIP;
                    continue;
                }
            }
            break;
        //#pes packing parsing
        case TS_PESHEADER:
            len = PES_HEADER_SIZE - es->data_index;
            if (len < 0) {
                return -1;
            }
            if (len > buf_size) {
                len = buf_size;
            }
            memcpy(es->header + es->data_index, buf, len);
            es->data_index += len;
            buf += len;
            buf_size -= len;
            if (es->data_index == PES_HEADER_SIZE) {
                es->pes_header_size = es->header[8] + 9;
                es->state           = TS_PESHEADER_FILL;
                dt_debug(TAG, "ENTER PESHEADER_FILL FROM PESHEADER \n");
            }
            break;
        case TS_PESHEADER_FILL:
            len = es->pes_header_size - es->data_index;
            if (len < 0) {
                return -1;
            }
            if (len > buf_size) {
                len = buf_size;
            }
            memcpy(es->header + es->data_index, buf, len);
            es->data_index += len;
            buf += len;
            buf_size -= len;
            if (es->data_index == es->pes_header_size) {
                const uint8_t *r;
                unsigned int flags, pes_ext, skip;

                flags = es->header[7];
                r = es->header + 9;
                es->pts = -1;
                es->dts = -1;
                if ((flags & 0xc0) == 0x80) {
                    es->dts = es->pts = parse_pes_pts(r);
                    r += 5;
                } else if ((flags & 0xc0) == 0xc0) {
                    es->pts = parse_pes_pts(r);
                    r += 5;
                    es->dts = parse_pes_pts(r);
                    r += 5;
                }
                es->extended_stream_id = -1;
                if (flags & 0x01) {
                    /* PES extension */
                    pes_ext = *r++;
                    /* Skip PES private data, program packet sequence counter and P-STD buffer */
                    skip  = (pes_ext >> 4) & 0xb;
                    skip += skip & 0x9;
                    r    += skip;
                    if ((pes_ext & 0x41) == 0x01 &&
                        (r + 2) <= (es->header + es->pes_header_size)) {
                        /* PES extension 2 */
                        if ((r[0] & 0x7f) > 0 && (r[1] & 0x80) == 0) {
                            es->extended_stream_id = r[1];
                        }
                    }
                }

                /* we got the full header. We parse it and get the payload */
                es->state = TS_PAYLOAD;
                dt_debug(TAG, "ENTER PESHEADER_FILL FROM PESHEADER \n");
                es->data_index = 0;

                if (es->stream_type == 0x12 && buf_size > 0) {
                    //fixme need to drop header
                    dt_info(TAG, "WARN you need to drop header \n");
                }
                if (es->stream_type == 0x15 && buf_size >= 5) {
                    /* skip metadata access unit header */
                    es->pes_header_size += 5;
                    buf += 5;
                    buf_size -= 5;
                    dt_info(TAG, "STREAM TYPE:%d \n", es->stream_type);
                }
            }
            break;
        case TS_PAYLOAD:
            if (buf_size > 0 && es->buffer) {
                if (es->data_index > 0 && es->data_index + buf_size > es->total_size) {

#if 1 // just for test
                    int idx = 0;
                    printf("payloda exceed total, data_index:%d buf_size:%d total:%d \n ",
                           es->data_index, buf_size, es->total_size);
                    for (idx = 0; idx < buf_size; idx++) {
                        printf("%02x ", buf[idx]);
                    }
                    printf(" \n");
#endif
                    uint8_t *tmp_buf = malloc(es->total_size + buf_size);
                    memcpy(tmp_buf, es->buffer, es->data_index);
                    free(es->buffer);
                    es->buffer = tmp_buf;
                    es->total_size = es->data_index + buf_size;

                } else if (es->data_index == 0 &&
                           buf_size > es->total_size) {
                    // pes packet size is < ts size packet and pes data is padded with 0xff
                    // not sure if this is legal in ts but see issue #2392
                    buf_size = es->total_size;
                }
                memcpy(es->buffer + es->data_index, buf, buf_size);
                es->data_index += buf_size;
                dt_debug(TAG, "copy payload SIZE:%d size:%d \n", buf_size, es->data_index);
            }
            buf_size = 0;
            break;
        case TS_SKIP:
            buf_size = 0;
            break;
        }
    }

    return ret;
}

static int ts_read_frame(demuxer_wrapper_t *wrapper, dt_av_pkt_t *frame)
{
    ts_packet_t packet;

    int ret = 0;
    dtdemuxer_context_t *ctx = (dtdemuxer_context_t *)(wrapper->parent);
    dt_media_info_t *media_info = &ctx->media_info;
    ts_ctx_t *ts_ctx = (ts_ctx_t *)wrapper->demuxer_priv;

    int has_audio = (media_info->disable_audio) ? 0 : media_info->has_audio;
    int has_video = (media_info->disable_video) ? 0 : media_info->has_video;
    int has_sub = (media_info->disable_sub) ? 0 : media_info->has_sub;

    int apid = (has_audio) ? media_info->astreams[media_info->cur_ast_index]->id :
               -1;
    int vpid = (has_video) ? media_info->vstreams[media_info->cur_vst_index]->id :
               -1;

    uint8_t ts_pkt[188];

    while (1) {
        while (1) {
            ret = dtstream_read(ctx->stream_priv, ts_pkt, 1);
            if (ret <= 0) {
                return DTERROR_READ_EOF;
            }
            if (ts_pkt[0] == 0x47) {
                break;
            }
        }
        ret = dtstream_read(ctx->stream_priv, ts_pkt + 1, TS_PACKET_LENGTH - 1);
        if (ret < TS_PACKET_LENGTH - 1) {
            return DTERROR_READ_EOF;
        }

        //fill one frame
        ret = ts_stream_fill_packet(ts_ctx->stream, &packet, ts_pkt);
        ret = 0;
        if (packet.pid == apid || packet.pid == vpid) {
            int type = (packet.pid == apid) ? DT_TYPE_AUDIO : DT_TYPE_VIDEO;
            ret = handle_ts_pkt(ts_ctx, &packet, frame, type);
            dt_debug(TAG, "HANDLE_TS_PKT RET:%d \n", ret);
            if (ret = 1) { // got one frame
                return DTERROR_NONE;
            }
        }
    }

    return DTERROR_READ_EOF;
}

static int ts_seek_frame(demuxer_wrapper_t *wrapper, int64_t timestamp)
{
    return 0;
}

static int ts_close(demuxer_wrapper_t *wrapper)
{
    ts_ctx_t *ts_ctx = (ts_ctx_t *)wrapper->demuxer_priv;
    if (ts_ctx->stream) {
        free(ts_ctx->stream);
    }
    ts_ctx->stream = NULL;
    if (ts_ctx->audio_num > 0) {
        buf_release(&ts_ctx->es_audio.dbt);
    }
    if (ts_ctx->video_num > 0) {
        buf_release(&ts_ctx->es_video.dbt);
    }

    return 0;
}

demuxer_wrapper_t demuxer_ts = {
    .name = "ts demuxer",
    .id = DEMUXER_TS,
    .probe = ts_probe,
    .open = ts_open,
    .read_frame = ts_read_frame,
    .setup_info = ts_setup_info,
    .seek_frame = ts_seek_frame,
    .close = ts_close
};
