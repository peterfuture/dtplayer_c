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

typedef struct{
    ts_stream_t *stream;
	ts_pidinfo_t *es_info[3];//a-v-s
    int audio_found;
    int video_found;
    int sub_found;

    //a-v-s cache
    dt_buffer_t dbt_audio;
    dt_buffer_t dbt_video;
    dt_buffer_t dbt_sub;

    int bitrate;
    int64_t filesize;
    int duration;

    //A-V INFO
    int channels;
    int samplerate;
    int bps;

    int width;
    int height;
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

static int find_syncword(const uint8_t *buf, int max)
{
    int pos = 0;
    while(pos < max)
    {
        if(buf[pos] == 0x47)
            return pos;
        pos++;
    }
    return -1;
}

static int
hexdump(ts_packet_t *packet)
{
	size_t c, d;
	uint8_t *bp;
	char sbuf[20];
	
	for(c = 0; c < packet->payloadlen; c += 16)
	{
		bp = &(packet->payload[c]);
		for(d = 0; d < 16; d++)
		{
			if(bp[d] < 0x80 && (isalnum((char) bp[d]) || bp[d] == 0x20))
			{
				sbuf[d] = (char) bp[d];
			}
			else
			{
				sbuf[d] = '.';
			}
		}
		sbuf[d] = 0x00;
		printf("%04x  %02x %02x %02x %02x %02x %02x %02x %02x-%02x %02x %02x %02x %02x %02x %02x %02x  %s\n",
			(unsigned int) c, bp[0], bp[1], bp[2], bp[3], bp[4], bp[5], bp[6], bp[7],
			bp[8], bp[9], bp[10], bp[11], bp[12], bp[13], bp[14], bp[15],
			sbuf);
	}
	return 0;
}

static int
dumpdvbnit(ts_stream_t *stream, ts_table_t *table, int complete)
{
	if(0 == complete && 1 == table->expected)
	{
		printf("  0x%04x - DVB Network Information Table 0x%02x (not yet defined)\n", (unsigned int) table->pid, table->tableid);
		return 0;
	}
	printf("  0x%04x - DVB Network Information Table 0x%02x\n", (unsigned int) table->pid, table->tableid);
	if(1 == table->expected)
	{
		if(1 == complete)
		{
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
	
	if(0 == complete && 1 == table->expected)
	{
		printf("  0x%04x - Program 0x%04x (not yet defined)\n",  (unsigned int) table->pid, (unsigned int) table->progid);
		return 0;
	}
	printf("  0x%04x - Program 0x%04x\n",  (unsigned int) table->pid, (unsigned int) table->progid);
	if(1 == table->expected)
	{
		if(1 == complete)
		{
			printf("  - Table defined but not present in stream\n");
		}
		return 0;
	}
	printf("  - Table contains details of %lu streams\n", (unsigned long) table->d.pmt.nes);
	for(c = 0; c < table->d.pmt.nes; c++)
	{
		info = table->d.pmt.es[c];
		printf("    0x%04x - ", info->pid);
		if(NULL == (stype = ts_typeinfo(info->stype)))
		{
			printf("Unknown stream type 0x%02x", info->stype);
		}
		else
		{
			printf("%s", stype->name);
		}
		switch(info->subtype)
		{
			case PST_UNSPEC: printf(" unspecified"); break;
			case PST_VIDEO: printf(" video"); break;
			case PST_AUDIO: printf(" audio"); break;
			case PST_INTERACTIVE: printf(" interactive"); break;
			case PST_CC: printf(" closed captioning"); break;
			case PST_IP: printf(" Internet Protocol"); break;
			case PST_SI: printf(" stream information"); break;
			case PST_NI: printf(" network information"); break;
		}
		switch(info->pidtype)
		{
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
		if(1 == info->pcr)
		{
			printf(" (PCR)");
		}
		if(1 == complete && 0 == info->seen)
		{
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
	if(1 == table->expected)
	{
		if(1 == complete)
		{
			printf("- Table defined but not present in stream\n");
		}
		return 0;
	}
	for(c = 0; c < table->d.pat.nprogs; c++)
	{
		dumptable(stream, table->d.pat.progs[c], complete);
	}
	return 0;
}

int
dumptable(ts_stream_t *stream, ts_table_t *table, int complete)
{
	switch(table->tableid)
	{
		case TID_PAT:
			return dumppat(stream, table, complete);
		case TID_PMT:
			return dumppmt(stream, table, complete);
		case TID_DVB_NIT:
			return dumpdvbnit(stream, table, complete);
		default:
			printf("0x%04x - Unknown table ID 0x%02x\n", (unsigned int) table->pid, table->tableid);
	}
	return 0;
}

static int ts_open(demuxer_wrapper_t *wrapper)
{
    int ret = 0;
    int complete = 1;
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
	
   	stream = ts_stream_create(&options);

    const uint8_t *buf = probe_buf->data;
    const uint8_t *buf_end = probe_buf->data + probe_buf->level;
    int buf_size = probe_buf->level;
    int pos = find_syncword(buf,buf_size);
    if(pos == -1)
    {
        dt_info(TAG,"Cannot find sync word \n");
        return -1;
    }

    uint32_t header = DT_RB8(buf+pos);
    if((header&0xFF) != 0x47)
        return -1;

    while(1)
    {
        if(buf+pos+188 >= buf_end)
            break;
        ret = ts_stream_read_packet(stream,&packet,buf+pos);
        if(ret == -1)
        {
            dt_info(TAG,"Parse one packet failed\n");
            break;
        }
        pos += 188;
    }
    
    //dump info
    if(stream->pat == NULL)
    {
        dt_info(TAG,"HEADER PARSE FAILED \n");
        return -1;
    }
    //dumptable(stream,stream->pat,complete);
    
    //during demuxer open, need to get stream info,including
    //stream: size
    //video: type w h
    //audio: type channels samplerate channels datawidth, default 16bit
    
    dt_info(TAG,"OK, START TO DETECT INFO\n");

    ts_table_t *tab_pat = stream->pat;
    if(1 == tab_pat->expected)
		return -1; //got nothing
	int c = 0;
    int i = 0;
    for(i = 0; i < tab_pat->d.pat.nprogs; i++)
	{
        ts_table_t *table = tab_pat->d.pat.progs[i];
	    ts_pidinfo_t *info;
	    const ts_streamtype_t *stype;
	
	    if(0 == complete && 1 == table->expected)
	    {
		    printf("  0x%04x - Program 0x%04x (not yet defined)\n",  (unsigned int) table->pid, (unsigned int) table->progid);
		    continue;
	    }
	    printf("  0x%04x - Program 0x%04x\n",  (unsigned int) table->pid, (unsigned int) table->progid);
	    if(1 == table->expected)
	    {
		    if(1 == complete)
		    {
			    printf("  - Table defined but not present in stream\n");
		    }
		    continue;
	    }
	    printf("  - Table contains details of %lu streams\n", (unsigned long) table->d.pmt.nes);
        for(c = 0; c < table->d.pmt.nes; c++) // get es info
	    {
		    info = table->d.pmt.es[c];
		    printf("    0x%04x - ", info->pid);
		    if(NULL == (stype = ts_typeinfo(info->stype)))
		    {
		    	printf("Unknown stream type 0x%02x", info->stype);
		    }
    		else
		    {
			    printf("%s", stype->name);
		    }
		    switch(info->subtype)
		    {
			    case PST_UNSPEC: printf(" unspecified"); break;
			    case PST_VIDEO: 
                    if(!ts_ctx->video_found)
                    {
                        ts_ctx->es_info[1] = info;
                        ts_ctx->video_found = 1; 
                    }
                    printf(" video");
                    break;
			    case PST_AUDIO: 
                    if(!ts_ctx->audio_found)
                    {
                        ts_ctx->es_info[0] = info;
                        ts_ctx->audio_found = 1; 
                    }
                    printf(" audio"); 
                    break;
			    case PST_INTERACTIVE: printf(" interactive"); break;
			    case PST_CC: printf(" closed captioning"); break;
			    case PST_IP: printf(" Internet Protocol"); break;
			    case PST_SI: printf(" stream information"); break;
			    case PST_NI: printf(" network information"); break;
		    }
		    switch(info->pidtype)
		    {
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
		    if(1 == info->pcr)
		    {
			    printf(" (PCR)");
		    }
		    if(1 == complete && 0 == info->seen)
		    {
			    printf(" (defined but not present)");
		    }
		    putchar('\n');
	    }
   
	}

    if(!ts_ctx->audio_found && ts_ctx->video_found)
    {
        dt_error(TAG,"NO ES STREAM FOUND \n");
        return -1;
    }
    dt_info(TAG,"AUDIO FOUND:%d VIDEO Found:%d \n",ts_ctx->audio_found,ts_ctx->video_found);
    if(ts_ctx->audio_found)
        buf_init(&ts_ctx->dbt_audio,TS_CACHE_AUDIO); 
    if(ts_ctx->video_found)
        buf_init(&ts_ctx->dbt_video,TS_CACHE_VIDEO); 
#if 0
    //step 3 parse es info
    int apid = (ts_ctx->audio_found)?(ts_ctx->es_info[0]->pid) : -1;
    int vpid = (ts_ctx->video_found)?(ts_ctx->es_info[1]->pid) : -1;
    int aes_parsed = (ts_ctx->audio_found)?0:1;
    int ves_parsed = (ts_ctx->video_found)?0:1;
    pos = find_syncword(buf,buf_size);

    while(1)
    {
        //fill one frame
        ret = ts_stream_fill_packet(stream,&packet,buf+pos);
        if(packet.pid == apid && !aes_parsed)
        {
            if(ts_ctx->dbt_audio.level > 0 && packet.unitstart) // have got one frame
            {
                //parse dbt buf

                aes_parsed = 1;
                continue;
            }
            buf_put(&ts_ctx->dbt_audio,packet.payload,packet.payloadlen);
        }
        else if(packet.pid == vpid && !ves_parsed)
        {
            if(ts_ctx->dbt_video.level > 0 && packet.unitstart) // have got one frame
            {
                ves_parsed = 1;
                continue;
            }
            buf_put(&ts_ctx->dbt_video,packet.payload,packet.payloadlen);
        }

        if(aes_parsed && ves_parsed)
            break;

    }
#endif

    wrapper->demuxer_priv = ts_ctx; 
    return 0;
}

static int ts_setup_info (demuxer_wrapper_t * wrapper, dt_media_info_t * info)
{
    dtdemuxer_context_t *ctx = (dtdemuxer_context_t *)(wrapper->parent);
    ts_ctx_t *ts_ctx = (ts_ctx_t *)wrapper->demuxer_priv;
    
    /*reset vars */
    memset (info, 0, sizeof (*info));
    //set cur stream index -1 ,other vars have been reset to 0
    info->cur_ast_index = -1;
    info->cur_vst_index = -1;
    info->cur_sst_index = -1;

    /*get media info */
    info->format = MEDIA_FORMAT_MPEGTS;
    strcpy (info->file_name, ctx->file_name);

    info->bit_rate = ts_ctx->bitrate;
    info->duration = ts_ctx->duration;
    info->file_size = ts_ctx->file_size;

    astream_info_t *ast_info = (astream_info_t *) malloc (sizeof (astream_info_t));
    memset (ast_info, 0, sizeof (astream_info_t));
    ast_info->index = 0;
    ast_info->id = 0;
    ast_info->channels = aac_ctx->channels;
    ast_info->sample_rate = aac_ctx->samplerate;
    ast_info->bps = aac_ctx->bps;
    ast_info->duration = aac_ctx->duration;
    ast_info->time_base.num = -1;
    ast_info->time_base.den = -1;
    ast_info->bit_rate = aac_ctx->bitrate;
    ast_info->format = AUDIO_FORMAT_AAC;
    ast_info->codec_priv = NULL;
    info->astreams[info->ast_num] = ast_info;
    info->ast_num++;
        
    info->has_audio = 1;
    info->cur_ast_index = 0;
    return 0;

    return 0;
}

static int ts_read_frame(demuxer_wrapper_t *wrapper, dt_av_frame_t *frame)
{
    
#if 0
    //step 3 parse es info
    int apid = (ts_ctx->audio_found)?(ts_ctx->es_info[0]->pid) : -1;
    int vpid = (ts_ctx->video_found)?(ts_ctx->es_info[1]->pid) : -1;
    int aes_parsed = (ts_ctx->audio_found)?0:1;
    int ves_parsed = (ts_ctx->video_found)?0:1;
    pos = find_syncword(buf,buf_size);

    while(1)
    {
        //fill one frame
        ret = ts_stream_fill_packet(stream,&packet,buf+pos);
        if(packet.pid == apid && !aes_parsed)
        {
            if(ts_ctx->dbt_audio.level > 0 && packet.unitstart) // have got one frame
            {
                //parse dbt buf

                aes_parsed = 1;
                continue;
            }
            buf_put(&ts_ctx->dbt_audio,packet.payload,packet.payloadlen);
        }
        else if(packet.pid == vpid && !ves_parsed)
        {
            if(ts_ctx->dbt_video.level > 0 && packet.unitstart) // have got one frame
            {
                ves_parsed = 1;
                continue;
            }
            buf_put(&ts_ctx->dbt_video,packet.payload,packet.payloadlen);
        }

        if(aes_parsed && ves_parsed)
            break;

    }
#endif
    return DTERROR_READ_EOF;
}

static int ts_seek_frame(demuxer_wrapper_t *wrapper, int timestamp)
{
    return 0;
}

static int ts_close(demuxer_wrapper_t *wrapper)
{
    ts_ctx_t *ts_ctx = (ts_ctx_t *)wrapper->demuxer_priv;

    if(ts_ctx->audio_found)
        buf_release(&ts_ctx->dbt_audio); 
    if(ts_ctx->video_found)
        buf_release(&ts_ctx->dbt_video); 
    
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
