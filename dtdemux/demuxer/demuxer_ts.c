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
    dt_buffer_t dbt_audio;
    dt_buffer_t dbt_video;
    dt_buffer_t dbt_sub;

}ts_ctx_t;

static int dumptable(ts_stream_t *stream, ts_table_t *table, int complete);

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

#if 0
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
#endif

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

static int
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

static int ts_type_convert(int type)
{
    int ret = -1;
    switch(type)
    {
        case ES_TYPE_H264:
            ret = VIDEO_FORMAT_H264;
            break;
        case ES_TYPE_AAC:
             ret = AUDIO_FORMAT_AAC;
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
    memset(ts_ctx,0,sizeof(ts_ctx_t));
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
    dumptable(stream,stream->pat,complete);
    
    //during demuxer open, need to get stream info,including
    dt_info(TAG,"START TO DETECT INFO\n");
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
		    continue;
	    if(1 == table->expected)
		    continue;
        
        for(c = 0; c < table->d.pmt.nes; c++) // get es info
	    {
		    info = table->d.pmt.es[c];
		    if(NULL == (stype = ts_typeinfo(info->stype)))
                continue;
		    
            switch(info->subtype)
		    {
			    case PST_UNSPEC: printf(" unspecified"); break;
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
			    case PST_INTERACTIVE: printf(" interactive"); break;
			    case PST_CC: printf(" closed captioning"); break;
			    case PST_IP: printf(" Internet Protocol"); break;
			    case PST_SI: printf(" stream information"); break;
			    case PST_NI: printf(" network information"); break;
		    }
	    }
	}

    dt_info(TAG,"esnum:%d anum:%d vnum:%d \n",ts_ctx->es_num,ts_ctx->audio_num,ts_ctx->video_num);
    if(ts_ctx->audio_num > 0)
        buf_init(&ts_ctx->dbt_audio,TS_CACHE_AUDIO); 
    if(ts_ctx->video_num > 0)
        buf_init(&ts_ctx->dbt_video,TS_CACHE_VIDEO); 
    
    //step 3 parse es info
    ts_ctx->filesize = dtstream_get_size(ctx->stream_priv);
    ts_ctx->bitrate = -1; // invalid
    ts_ctx->duration = -1; // invalid

    int pid_tmp = -1;
    int idx = 0;
    ts_pidinfo_t *tmp;//a-v-s stream
    for(idx = 0;idx< ts_ctx->es_num;idx++)
    {
        tmp = ts_ctx->es_info[idx];
        if(tmp->subtype == PST_VIDEO)
        {
            pid_tmp = tmp->pid;
            break;
        }
        if(tmp->subtype == PST_AUDIO && ts_ctx->video_num == 0)
        {
            pid_tmp = tmp->pid;
            break;
        }
    }
    if(pid_tmp == -1)
    {
        dt_error(TAG,"FAILED TO GET INFO\n");
        return -1;
    }
    dt_info(TAG,"PID:%x \n",pid_tmp); 
    
    //find first pts
    pos = find_syncword(buf,buf_size);
    int64_t first_pts = -1;
    int64_t last_pts = -1;
    
    while(1)
    {
        if(buf[pos] != 0x47)
        {
            pos += find_syncword(buf+pos,buf_size-pos);
        }
        if(pos >= buf_size - 188)
            break;

        ret = ts_stream_fill_packet(stream,&packet,buf+pos);
        if(packet.pid == pid_tmp)
        {
#if 0 // calc 
			if(!packet.unitstart)
                goto NEXT_PKT;
            if(packet.payloadlen == 0)
                goto NEXT_PKT;
            uint8_t *pcrbuf = packet.payload;
            int len = pcrbuf[0];
			if(len < 0 || len > 183)	//broken from the stream layer or invalid
			{
				goto NEXT_PKT;
			}
			//c==0 is allowed!
			if(len <= 0)
                goto NEXT_PKT;
            pcrbuf++;
			int flags = pcrbuf[0];
			int has_pcr;
			has_pcr = flags & 0x10;
            pcrbuf++;
            if(!has_pcr)
                goto NEXT_PKT;
			int64_t pcr = -1;
            int64_t pcr_ext = -1;
            unsigned int v = 0;
            v = DT_RB32(pcrbuf);
            //v = (pcrbuf[0]<<24) | pcrbuf[1]<<16 | pcrbuf[2]<<8 | pcrbuf[3];
			pcr = ((int64_t)v<<1) | (pcrbuf[4] >> 7);

			pcr_ext = (pcrbuf[4] & 0x01) << 8;
			pcr_ext |= pcrbuf[5];

			//pcr = pcr * 300 + pcr_ext;
			first_pts = pcr;
#endif
            if(packet.pts == -1)
                goto NEXT_PKT;
            first_pts = packet.pts;
            dt_info(TAG,"Find first pcr :%lld %lld %02x %02x\n",packet.pts,first_pts,packet.payload[0],packet.payload[1]);
            break;
            //demuxer->reference_clock = (double)pcr/(double)27000000.0;
		}
NEXT_PKT:
        pos += 188;
	}
    dt_info(TAG,"GET FIRST PTS:%lld \n",first_pts);
    if(first_pts == -1)
        return 0;
    //find last pts
    if(ts_ctx->filesize > buf_size)
        return -1;
    pos = ts_ctx->filesize - buf_size;
    ret = dtstream_seek(ctx->stream_priv,pos,SEEK_SET);
    if(ret <0)
    {
        dtstream_seek(ctx->stream_priv,0,SEEK_SET);
        return -1;
    }
    ret = dtstream_read(ctx->stream_priv,probe_buf->data,buf_size);
    if(ret <=0)
        return -1;
    buf = probe_buf->data;
    buf_size = ret;
    dtstream_seek(ctx->stream_priv,0,SEEK_SET);
    pos = 0;
    pos = find_syncword(buf,buf_size);
    memset(&packet,0,sizeof(packet));
    while(1)
    {
        if(buf[pos] != 0x47)
        {
            pos += find_syncword(buf+pos,buf_size-pos);
        }
        if(pos >= buf_size - 188)
            break;

        ret = ts_stream_fill_packet(stream,&packet,buf+pos);
        if(packet.pid == pid_tmp)
        {    
#if 0
            if(packet.payloadlen <= 0)
                goto NEXT_PKT2;
            char *pcrbuf = packet.payload;
			
            int len = pcrbuf[0];
			if(len < 0 || len > 183)	//broken from the stream layer or invalid
			{
				goto NEXT_PKT2;
			}

			//c==0 is allowed!
			if(len <= 0)
                goto NEXT_PKT2;
            pcrbuf++;
			int flags = pcrbuf[0];
			int has_pcr;
			has_pcr = flags & 0x10;
            pcrbuf++;
            if(!has_pcr)
                goto NEXT_PKT2;
			
            int64_t pcr = -1;
            int64_t pcr_ext = -1;
            unsigned int v = 0;
            v = DT_RB32(pcrbuf);
            //v = (pcrbuf[0]<<24) | pcrbuf[1]<<16 | pcrbuf[2]<<8 | pcrbuf[3];
			pcr = ((int64_t)v<<1) | (pcrbuf[4] >> 7);

			pcr_ext = (pcrbuf[4] & 0x01) << 8;
			pcr_ext |= pcrbuf[5];

			//pcr = pcr * 300 + pcr_ext;
			last_pts = pcr;

#endif
            if(packet.pts == -1)
                goto NEXT_PKT2;
            last_pts = packet.pts;
            dt_debug(TAG,"Find last pcr :%lld %lld\n",packet.pts,last_pts);
            //demuxer->reference_clock = (double)pcr/(double)27000000.0;
		}
NEXT_PKT2:
        pos += 188;
	}
    dt_info(TAG,"GET LAST PTS:%lld \n",last_pts);
    if(last_pts == -1)
        return 0;
 
    //step - calc stream
    //get first pts and last pts to calc
    //bitrate = filesize / duration

    ts_ctx->duration = (last_pts - first_pts)/90000;
    //if(ts_ctx->duration > 0)
        ts_ctx->bitrate = ts_ctx->filesize / ts_ctx->duration;

    dt_info(TAG,"Duration:%d bitrate:%d \n",ts_ctx->duration,ts_ctx->bitrate);
    return 0;
}

static int ts_setup_info (demuxer_wrapper_t * wrapper, dt_media_info_t * info)
{
    dtdemuxer_context_t *ctx = (dtdemuxer_context_t *)(wrapper->parent);
    ts_ctx_t *ts_ctx = (ts_ctx_t *)wrapper->demuxer_priv;
 
    vstream_info_t *vst_info;
    astream_info_t *ast_info;
   
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
    info->file_size = ts_ctx->filesize;
    
    int i = 0; 
    for (i = 0; i < ts_ctx->es_num; i++)
    {
        ts_pidinfo_t *es = ts_ctx->es_info[i];
        if (es->subtype == PST_VIDEO)
        {
            vst_info = (vstream_info_t *) malloc (sizeof (vstream_info_t));
            memset (vst_info, 0, sizeof (vstream_info_t));
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
        if (es->subtype == PST_AUDIO)
        {
            ast_info = (astream_info_t *) malloc (sizeof (astream_info_t));
            memset (ast_info, 0, sizeof (astream_info_t));
            ast_info->index = i;
            ast_info->id = es->pid;
            ast_info->channels = 2; // default
            ast_info->sample_rate = 48000; //default
            ast_info->bps = 16; // default
            ast_info->duration = info->duration;
            ast_info->bit_rate = -1;
            ast_info->format = ts_type_convert (es->stype);
            ast_info->codec_priv = NULL;
            info->astreams[info->ast_num] = ast_info;
            info->ast_num++;
        }
    }
    if (info->vst_num > 0)
    {
        info->has_video = 1;
        info->cur_vst_index = 0;
    }
    if (info->ast_num > 0)
    {
        info->has_audio = 1;
        info->cur_ast_index = 0;
    }
    if (info->sst_num > 0)
    {
        info->has_sub = 1;
        info->cur_sst_index = 0;
    }
 
    return 0;
}

static int ts_read_frame(demuxer_wrapper_t *wrapper, dt_av_frame_t *frame)
{
	ts_packet_t packet;
    
    int ret = 0;
    dtdemuxer_context_t *ctx = (dtdemuxer_context_t *)(wrapper->parent);
    dt_media_info_t *media_info = &ctx->media_info;
    ts_ctx_t *ts_ctx = (ts_ctx_t *)wrapper->demuxer_priv;
 
    int has_audio = (media_info->no_audio) ? 0 : media_info->has_audio;
    int has_video = (media_info->no_video) ? 0 : media_info->has_video;
    int has_sub = (media_info->no_sub) ? 0 : media_info->has_sub;

    int apid = (has_audio) ? media_info->astreams[media_info->cur_ast_index]->id : -1;
    int vpid = (has_video) ? media_info->vstreams[media_info->cur_vst_index]->id : -1;

    uint8_t ts_pkt[188];
    
    while(1)
    {
        while(1)
        {
            ret = dtstream_read(ctx->stream_priv,ts_pkt,1);
            if(ret <= 0)
                return DTERROR_READ_EOF;
            if(ts_pkt[0] == 0x47)
                break;
        }
        ret = dtstream_read(ctx->stream_priv,ts_pkt+1,TS_PACKET_LENGTH - 1);
        if(ret < TS_PACKET_LENGTH -1)
            return DTERROR_READ_EOF;
        
        //fill one frame
        ret = ts_stream_fill_packet(ts_ctx->stream,&packet,ts_pkt);
        if(packet.pid == apid)
        {
            if(ts_ctx->dbt_audio.level > 0 && packet.unitstart) // have got one frame
            {
                frame->type = DT_TYPE_AUDIO;
                frame->size = ts_ctx->dbt_audio.level;
                frame->data = malloc(frame->size);
                buf_get(&ts_ctx->dbt_audio,frame->data,frame->size);
                frame->pts = ts_ctx->cur_apts;
                frame->dts = -1;
                frame->duration = -1;
                buf_put(&ts_ctx->dbt_audio,packet.payload,packet.payloadlen);
                dt_info(TAG,"READ ONE AUDIO FRAME, PTS:%lld size:%d dbtlevel:%d \n",frame->pts,frame->size,ts_ctx->dbt_audio.level);
                ts_ctx->cur_apts = packet.pts;
                return DTERROR_NONE;
            }
            buf_put(&ts_ctx->dbt_audio,packet.payload,packet.payloadlen);
            if(ts_ctx->cur_apts == -1 && packet.pts != -1)
                ts_ctx->cur_apts = packet.pts;
        }
        else if(packet.pid == vpid)
        {
            if(ts_ctx->dbt_video.level > 0 && packet.unitstart) // have got one frame
            {
                frame->type = DT_TYPE_VIDEO;
                frame->size = ts_ctx->dbt_video.level;
                frame->data = malloc(frame->size);
                buf_get(&ts_ctx->dbt_video,frame->data,frame->size);
                frame->pts = ts_ctx->cur_vpts;
                frame->dts = -1;
                frame->duration = -1;
                buf_put(&ts_ctx->dbt_video,packet.payload,packet.payloadlen);
                ts_ctx->cur_vpts = packet.pts;
                return DTERROR_NONE;
            }
            buf_put(&ts_ctx->dbt_video,packet.payload,packet.payloadlen);
            if(ts_ctx->cur_vpts == -1)
                ts_ctx->cur_vpts = packet.pts;
        }
    }
    
    return DTERROR_READ_EOF;
}

static int ts_seek_frame(demuxer_wrapper_t *wrapper, int timestamp)
{
    return 0;
}

static int ts_close(demuxer_wrapper_t *wrapper)
{
    ts_ctx_t *ts_ctx = (ts_ctx_t *)wrapper->demuxer_priv;
    if(ts_ctx->stream)
        free(ts_ctx->stream);
    ts_ctx->stream = NULL;
    if(ts_ctx->audio_num > 0)
        buf_release(&ts_ctx->dbt_audio); 
    if(ts_ctx->video_num > 0)
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
