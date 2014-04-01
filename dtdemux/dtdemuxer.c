#include "dtdemuxer.h"
#include "dtstream_api.h"

#define TAG "DEMUXER"

#define REGISTER_DEMUXER(X,x) \
    {                         \
        extern demuxer_wrapper_t demuxer_##x; \
        register_demuxer(&demuxer_##x);     \
    }
static demuxer_wrapper_t *g_demuxer = NULL;

static void register_demuxer (demuxer_wrapper_t * wrapper)
{
    demuxer_wrapper_t **p;
    p = &g_demuxer;
    while (*p != NULL)
        p = &((*p)->next);
    *p = wrapper;
    wrapper->next = NULL;
}

void demuxer_register_all ()
{
    REGISTER_DEMUXER (AAC, aac);
#ifdef ENABLE_DEMUXER_FFMPEG
    REGISTER_DEMUXER (FFMPEG, ffmpeg);
#endif
}

static int demuxer_select (dtdemuxer_context_t * dem_ctx)
{
    if (!g_demuxer)
        return -1;
    int score = 0;
    demuxer_wrapper_t *entry = g_demuxer;
    while(entry != NULL)
    {
        score = (entry)->probe(entry,dem_ctx);
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

static void dump_media_info (dt_media_info_t * info)
{
    dt_info (TAG, "|====================MEDIA INFO====================|\n", info->file_name);
    dt_info (TAG, "|file_name:%s\n", info->file_name);
    dt_info (TAG, "|file_size:%d \n", info->file_size);
    dt_info (TAG, "|duration:%d bitrate:%d\n", info->duration, info->bit_rate);
    dt_info (TAG, "|has video:%d has audio:%d has sub:%d\n", info->has_video, info->has_audio, info->has_sub);
    dt_info (TAG, "|video stream info,num:%d\n", info->vst_num);
    int i = 0;
    for (i = 0; i < info->vst_num; i++)
    {
        dt_info (TAG, "|video stream:%d index:%d id:%d\n", i, info->vstreams[i]->index, info->vstreams[i]->id);
        dt_info (TAG, "|bitrate:%d width:%d height:%d duration:%lld \n", info->vstreams[i]->bit_rate, info->vstreams[i]->width, info->vstreams[i]->height, info->vstreams[i]->duration);
    }
    dt_info (TAG, "|audio stream info,num:%d\n", info->ast_num);
    for (i = 0; i < info->ast_num; i++)
    {
        dt_info (TAG, "|audio stream:%d index:%d id:%d\n", i, info->astreams[i]->index, info->astreams[i]->id);
        dt_info (TAG, "|bitrate:%d sample_rate:%d channels:%d bps:%d duration:%lld \n", info->astreams[i]->bit_rate, info->astreams[i]->sample_rate, info->astreams[i]->channels, info->astreams[i]->bps, info->astreams[i]->duration);
    }

    dt_info (TAG, "|subtitle stream num:%d\n", info->sst_num);
    dt_info (TAG, "|================================================|\n", info->file_name);
}

int demuxer_open (dtdemuxer_context_t * dem_ctx)
{
    int ret = 0;
    /* open stream */
    dtstream_para_t para;
    para.stream_name = dem_ctx->file_name; 
    ret = dtstream_open(&dem_ctx->stream_priv,&para,dem_ctx);
    if(ret != DTERROR_NONE)
    {
        dt_error (TAG, "stream open failed \n");
        return -1;
    }
   
    char value[512];
    int probe_enable = 0;
    int probe_size = PROBE_BUF_SIZE;
    if(GetEnv("DEMUXER","demuxer.probe",value) > 0)
    {
        probe_enable = atoi(value);
        dt_info(TAG,"probe enable:%d \n",probe_enable);
    }
    else
        dt_info(TAG,"probe enable not set, use default:%d \n",probe_enable);

    if(GetEnv("DEMUXER","demuxer.probesize",value) > 0)
    {
        probe_size = atoi(value);
        dt_info(TAG,"probe size:%d \n",probe_size);
    }
    else
        dt_info(TAG,"probe size not set, use default:%d \n",probe_size);


    if(probe_enable)
    {
        int64_t old_pos = dtstream_tell(dem_ctx->stream_priv);
        dt_info(TAG,"old:%lld \n",old_pos);
        ret = buf_init(&dem_ctx->probe_buf,PROBE_BUF_SIZE);
        if(ret < 0)
            return -1; 
        ret = dtstream_read(dem_ctx->stream_priv,dem_ctx->probe_buf.data,probe_size); 
        if(ret <= 0)
            return -1;
        dem_ctx->probe_buf.level = ret;
        ret = dtstream_seek(dem_ctx->stream_priv,old_pos,SEEK_SET); 
        dt_info(TAG,"seek back to:%lld ret:%d \n",old_pos,ret);
    }

    /* select demuxer */
    if (demuxer_select (dem_ctx) == -1)
    {
        dt_error (TAG, "select demuxer failed \n");
        return -1;
    }
    demuxer_wrapper_t *wrapper = dem_ctx->demuxer;
    ret = wrapper->open (wrapper);
    if (ret < 0)
    {
        dt_error (TAG, "demuxer open failed\n");
        return -1;
    }
    dt_info (TAG, "demuxer open ok\n");
    dt_media_info_t *info = &(dem_ctx->media_info);
    wrapper->setup_info (wrapper, info);
    dump_media_info (info);
    dt_info (TAG, "demuxer setup info ok\n");
    return 0;
}

int demuxer_read_frame (dtdemuxer_context_t * dem_ctx, dt_av_frame_t * frame)
{
    demuxer_wrapper_t *wrapper = dem_ctx->demuxer;
    return wrapper->read_frame (wrapper, frame);
}

int demuxer_seekto (dtdemuxer_context_t * dem_ctx, int timestamp)
{
    demuxer_wrapper_t *wrapper = dem_ctx->demuxer;
    return wrapper->seek_frame (wrapper, timestamp);
}

int demuxer_close (dtdemuxer_context_t * dem_ctx)
{
    int i = 0;
    demuxer_wrapper_t *wrapper = dem_ctx->demuxer;
    wrapper->close (wrapper);
    /*free media info */
    dt_media_info_t *info = &(dem_ctx->media_info);
    if (info->has_audio)
        for (i = 0; i < info->ast_num; i++)
        {
            if (info->astreams[i] == NULL)
                continue;
            if (info->astreams[i]->extradata_size)
                free (info->astreams[i]->extradata);
            free (info->astreams[i]);
            info->astreams[i] = NULL;
        }
    if (info->has_video)
        for (i = 0; i < info->vst_num; i++)
        {
            if (info->vstreams[i] == NULL)
                continue;
            if (info->vstreams[i]->extradata_size)
                free (info->vstreams[i]->extradata);
            free (info->vstreams[i]);
            info->vstreams[i] = NULL;
        }
    if (info->has_sub)
        for (i = 0; i < info->sst_num; i++)
        {
            if (info->sstreams[i] == NULL)
                continue;
            if (info->sstreams[i]->extradata_size)
                free (info->sstreams[i]->extradata);
            free (info->sstreams[i]);
            info->sstreams[i] = NULL;
        }
    /* release probe buf */
    buf_release(&dem_ctx->probe_buf);
    /* close stream */
    dtstream_close(dem_ctx->stream_priv);
    return 0;
}
