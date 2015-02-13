#include "dtdemuxer.h"
#include "dtstream_api.h"
#include "dt_setting.h"

#include <unistd.h>

#define TAG "DEMUXER"

#define REGISTER_DEMUXER(X,x) \
    {                         \
        extern demuxer_wrapper_t demuxer_##x; \
        register_demuxer(&demuxer_##x);     \
    }
static demuxer_wrapper_t *g_demuxer = NULL;

static void register_demuxer(demuxer_wrapper_t * wrapper)
{
    demuxer_wrapper_t **p;
    p = &g_demuxer;
    while(*p != NULL)
        p = &((*p)->next);
    *p = wrapper;
    wrapper->next = NULL;
    dt_info(TAG,"REGISTER DEMUXER:%s \n",wrapper->name);
}

void demuxer_register_all()
{
#ifdef ENABLE_DEMUXER_AAC
    REGISTER_DEMUXER(AAC, aac);
#endif
#ifdef ENABLE_DEMUXER_TS
    REGISTER_DEMUXER(TS, ts);
#endif

#ifdef ENABLE_DEMUXER_FFMPEG
    REGISTER_DEMUXER(FFMPEG, ffmpeg);
#endif
}

void demuxer_remove_all()
{
    g_demuxer = NULL;
}

static int demuxer_select(dtdemuxer_context_t * dem_ctx)
{
    if(!g_demuxer)
        return -1;
    int score = 0;
    demuxer_wrapper_t *entry = g_demuxer;
    while(entry != NULL)
    {
        score =(entry)->probe(entry,&(dem_ctx->probe_buf));
        if(score == 1)
            break;
        entry = entry->next;
    }
    if(!entry)
        return -1;
    dem_ctx->demuxer = entry;
    dt_info(TAG,"SELECT DEMUXER:%s \n",entry->name);
    return 0;
}

static void dump_media_info(dt_media_info_t * info)
{
    const char *type = "ung";
    dt_info(TAG, "|====================MEDIA INFO====================| \n");
    dt_info(TAG, "|file_name:%s\n", info->file_name);
    dt_info(TAG, "|file_size:%lld \n", info->file_size);
    dt_info(TAG, "|file_format:%s \n", dt_mediafmt2str(info->format));
    dt_info(TAG, "|duration:%lld bitrate:%d\n", info->duration, info->bit_rate);
    dt_info(TAG, "|has video:%d has audio:%d has sub:%d\n", info->has_video, info->has_audio, info->has_sub);
    dt_info(TAG, "|====================VIDEO INFO====================| \n");
    dt_info(TAG, "|video stream info,num:%d\n", info->vst_num);
    int i = 0;
    for(i = 0; i < info->vst_num; i++)
    {
        dt_info(TAG, "|--video stream:%d index:%d id:%d fmt:%s lang:%s \n", i, info->vstreams[i]->index, info->vstreams[i]->id, type, info->vstreams[i]->language);
        dt_info(TAG, "|--bitrate:%d width:%d height:%d duration:%lld \n", info->vstreams[i]->bit_rate, info->vstreams[i]->width, info->vstreams[i]->height, info->vstreams[i]->duration);
    }
    dt_info(TAG, "|====================AUDIO INFO====================| \n");
    dt_info(TAG, "|audio stream info,num:%d\n", info->ast_num);
    for(i = 0; i < info->ast_num; i++)
    {
        dt_info(TAG, "|--audio stream:%d index:%d id:%d fmt:%s lang:%s \n", i, info->astreams[i]->index, info->astreams[i]->id, type, info->astreams[i]->language);
        dt_info(TAG, "|--bitrate:%d sample_rate:%d channels:%d bps:%d duration:%lld \n", info->astreams[i]->bit_rate, info->astreams[i]->sample_rate, info->astreams[i]->channels, info->astreams[i]->bps, info->astreams[i]->duration);
    }

    dt_info(TAG, "|====================SUB INFO======================| \n");
    dt_info(TAG, "|subtitle stream num:%d\n", info->sst_num);
    for(i = 0; i < info->sst_num; i++)
    {
        dt_info(TAG, "|--sub stream:%d index:%d id:%d fmt:%s lang:%s \n", i, info->sstreams[i]->index, info->sstreams[i]->id, type, info->sstreams[i]->language);
        dt_info(TAG, "|--width:%d height:%d \n", info->sstreams[i]->width, info->sstreams[i]->height);
    }
    dt_info(TAG, "|==================================================|\n");
}

int demuxer_open(dtdemuxer_context_t * dem_ctx)
{
    int ret = 0;
    /* open stream */
    dtstream_para_t para;
    para.stream_name = dem_ctx->file_name; 
    ret = dtstream_open(&dem_ctx->stream_priv,&para,dem_ctx);
    if(ret != DTERROR_NONE)
    {
        dt_error(TAG, "stream open failed \n");
        return -1;
    }
   
    dt_info(TAG,"probe enable start \n");
    int probe_enable = dtp_setting.demuxer_probe;
    int probe_size = dtstream_local(dem_ctx->stream_priv)?PROBE_LOCAL_SIZE:PROBE_STREAM_SIZE;
    dt_info(TAG,"probe enable:%d \n",probe_enable);
    dt_info(TAG,"probe size:%d \n",probe_size);

    if(probe_enable)
    {
        int64_t old_pos = dtstream_tell(dem_ctx->stream_priv);
        dt_info(TAG,"old:%lld \n",old_pos);
        ret = buf_init(&dem_ctx->probe_buf,probe_size);
        if(ret < 0)
            return -1;
        //buffer data for probe
        int total = probe_size;
        int rtotal = 0;
        int rlen = 0;
        int retry = 100;
        const int read_once = 10240;
        uint8_t tmp_buf[read_once]; 
        do{
            
            rlen = dtstream_read(dem_ctx->stream_priv,tmp_buf,read_once); 
            if(rlen < 0) // read eof
            {
                if((probe_size-total)==0)
                    return -1;
                else
                    break;
            }
            if(rlen >0)
                retry = 20;
            else
            {
                usleep(100000); // total time :20 * 100000 = 10s
                retry--;
            }
            if(retry == 0)
            {
                dt_info(TAG,"retry 100 times, total:%d  \n",total);
                if((probe_size-total) == 0)
                    return -1;
                else
                    break;
            }
            dt_debug(TAG,"total:%d rtotal:%d rlen:%d \n",total,rtotal,rlen);
            if(rlen == 0)
                continue;
            if(buf_put(&dem_ctx->probe_buf,tmp_buf,rlen) == 0) // full
                break;
            rtotal += rlen;
            total -= rlen;
            if(total == 0)
                break;
        }while(1);

        dt_info(TAG,"buffered %d bytes for probe \n",probe_size - total);
        ret = dtstream_seek(dem_ctx->stream_priv,old_pos,SEEK_SET); 
        dt_info(TAG,"seek back to:%lld ret:%d \n",old_pos,ret);
    }

    /* select demuxer */
    if(demuxer_select(dem_ctx) == -1)
    {
        dt_error(TAG, "select demuxer failed \n");
        return -1;
    }
    demuxer_wrapper_t *wrapper = dem_ctx->demuxer;
    wrapper->parent = dem_ctx;
    ret = wrapper->open(wrapper);
    if(ret < 0)
    {
        dt_error(TAG, "demuxer open failed\n");
        return -1;
    }
    dt_info(TAG, "demuxer open ok\n");
    dt_media_info_t *info = &(dem_ctx->media_info);
    wrapper->setup_info(wrapper, info);
    dump_media_info(info);
    dt_info(TAG, "demuxer setup info ok\n");
    return 0;
}

int demuxer_read_frame(dtdemuxer_context_t * dem_ctx, dt_av_pkt_t * frame)
{
    demuxer_wrapper_t *wrapper = dem_ctx->demuxer;
    return wrapper->read_frame(wrapper, frame);
}

int demuxer_seekto(dtdemuxer_context_t * dem_ctx, int64_t timestamp)
{
    demuxer_wrapper_t *wrapper = dem_ctx->demuxer;
    dt_info(TAG,"Enter seek \n");
    return wrapper->seek_frame(wrapper, timestamp);
}

int demuxer_close(dtdemuxer_context_t * dem_ctx)
{
    int i = 0;
    demuxer_wrapper_t *wrapper = dem_ctx->demuxer;
    wrapper->close(wrapper);
    /*free media info */
    dt_media_info_t *info = &(dem_ctx->media_info);
    if(info->has_audio)
        for(i = 0; i < info->ast_num; i++)
        {
            if(info->astreams[i] == NULL)
                continue;
            if(info->astreams[i]->extradata_size)
                free(info->astreams[i]->extradata);
            free(info->astreams[i]);
            info->astreams[i] = NULL;
        }
    if(info->has_video)
        for(i = 0; i < info->vst_num; i++)
        {
            if(info->vstreams[i] == NULL)
                continue;
            free(info->vstreams[i]);
            info->vstreams[i] = NULL;
        }
    if(info->has_sub)
        for(i = 0; i < info->sst_num; i++)
        {
            if(info->sstreams[i] == NULL)
                continue;
            if(info->sstreams[i]->extradata_size)
                free(info->sstreams[i]->extradata);
            free(info->sstreams[i]);
            info->sstreams[i] = NULL;
        }
    /* release probe buf */
    buf_release(&dem_ctx->probe_buf);
    /* close stream */
    dtstream_close(dem_ctx->stream_priv);
    return 0;
}
