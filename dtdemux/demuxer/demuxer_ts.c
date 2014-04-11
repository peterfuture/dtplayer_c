#include "dtdemuxer.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TAG "DEMUXER-TS"

typedef struct{
    ts_stream_t *stream;
	ts_options_t options;
	ts_packet_t packet;
 
}ts_ctx_t;

static int ts_probe(demuxer_wrapper_t *wrapper, dt_buffer_t *probe_buf)
{
    const uint8_t *buf = probe_buf->data;
    const uint8_t *end = buf + probe_buf->level - 7;
    
    if(probe_buf->level < 10)
        return 0;

    int retry_times = 100;
    for(; buf < end; buf++) 
    {
        
        uint32_t header = DT_RB8(buf);
        if((header&0xFF) != 0x47)
        {
            if(retry_times-- == 0)
                return 0;
            continue;
        }
        //found 0x47
        if(buf+188 > end)
            return 0;
        header = DT_RB8(buf+188);
        if((header & 0xFF) == 0x47)
        {
            dt_info(TAG,"ts detect \n");
            return 1;
        }
        else
            return 0;
    }
    return 0;
}

static int find_syncword(const uint8_t *buf)
{

    return 0;
}

static int ts_open(demuxer_wrapper_t *wrapper)
{
    ts_ctx_t *ts_ctx = (ts_ctx_t *)malloc(sizeof(ts_ctx_t));
    memset(ts_ctx,0,sizeof(ts_ctx_t));
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
	r = 0;
	if(argc < 2)
	{
		fprintf(stderr, "Usage: %s [OPTIONS] FILE1 ... FILEn\n", argv[0]);
		return 1;
	}
	stream = ts_stream_create(&options);



    const uint8_t *buf = probe_buf->data;
    int pos = find_syncword(buf);


    uint32_t header = DT_RB8(buf+pos);
    if((header&0xFFF6) != 0xFFF0)
        return -1;
    int profile = (DT_RB8(buf+2)>>6)&0xf;
    aac_ctx->profile = profile; 
    dt_debug(TAG,"Profile:%s \n",aac_profile[profile]);
    int sr = (DT_RB8(buf+2)>>2) & 0xf;
    aac_ctx->samplerate = sr_index[sr];
    dt_debug(TAG,"Samplerate:%d \n",sr_index[sr]);
    int channel = (DT_RB16(buf + 2) >> 6) & 0x7;
    aac_ctx->channels = channel;
    dt_debug(TAG,"channel:%d \n",channel);
    aac_ctx->bps = 16;
    aac_ctx->file_size = dtstream_get_size(ctx->stream_priv);
    estimate_duration(wrapper);
    return 0;

    return 0;
}

static int ts_setup_info (demuxer_wrapper_t * wrapper, dt_media_info_t * info)
{
    return 0;
}

static int ts_read_frame(demuxer_wrapper_t *wrapper, dt_av_frame_t *frame)
{
    return DTERROR_READ_EOF;
}

static int ts_seek_frame(demuxer_wrapper_t *wrapper, int timestamp)
{
    return 0;
}

static int ts_close(demuxer_wrapper_t *wrapper)
{
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
