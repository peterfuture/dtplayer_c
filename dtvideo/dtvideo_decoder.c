#include "dtvideo.h"
#include "dtvideo_decoder.h"

//#define DTVIDEO_DECODER_DUMP 0

#define TAG "VIDEO-DEC"

#define REGISTER_VDEC(X,x)	 	\
	{							\
		extern vd_wrapper_t vd_##x##_ops; 	\
		register_vdec(&vd_##x##_ops); 	\
	}

//pts calc mode, 
//0: calc with pts read from demuxer 
//1: calc with duration
static int pts_mode = 0; //pts calc mode, 0: calc with pts 1: calc with duration

static vd_wrapper_t *g_vd = NULL;

static void register_vdec (vd_wrapper_t * vd)
{
    vd_wrapper_t **p;
    p = &g_vd;
    while (*p != NULL)
        p = &(*p)->next;
    *p = vd;
    dt_info (TAG, "[%s:%d] register vdec, name:%s fmt:%d \n", __FUNCTION__, __LINE__, (*p)->name, (*p)->vfmt);
    vd->next = NULL;
}

void register_vdec_ext(vd_wrapper_t *vd)
{
    vd_wrapper_t **p;
    p = &g_vd;
    if(*p == NULL)
    {
        *p = vd;
        vd->next = NULL;
    }
    else
    {
        vd->next = *p;
        *p = vd;
    }
    
    dt_info (TAG, "[%s:%d]register ext vd. vfmt:%d name:%s \n",__FUNCTION__,__LINE__,vd->vfmt, vd->name);

}

void vdec_register_all ()
{
    //comments:video using ffmpeg decoder only
#ifdef ENABLE_VDEC_FFMPEG
    REGISTER_VDEC (FFMPEG, ffmpeg);
#endif
    return;
}

static int select_video_decoder (dtvideo_decoder_t * decoder)
{
    vd_wrapper_t **p;
    dtvideo_para_t *para = &(decoder->para);
    p = &g_vd;
    while(*p != NULL)
    {
        if((*p)->vfmt == DT_VIDEO_FORMAT_UNKOWN)
            break;
        if((*p)->vfmt == para->vfmt)
            break;
        p = &(*p)->next;
    }
    if(*p == NULL)
    {
        dt_info (TAG, "[%s:%d]no valid video decoder found vfmt:%d\n", __FUNCTION__, __LINE__, para->vfmt);
        return -1;
    }
    decoder->wrapper = *p;
    dt_info (TAG, "[%s:%d] select--%s video decoder \n", __FUNCTION__, __LINE__, (*p)->name);
    return 0;
}

static int64_t pts_exchange (dtvideo_decoder_t * decoder, int64_t pts)
{
    return pts;
}

static void *video_decode_loop (void *arg)
{
    dt_av_frame_t frame;
    dtvideo_decoder_t *decoder = (dtvideo_decoder_t *) arg;
    vd_wrapper_t *wrapper = decoder->wrapper;
    dtvideo_context_t *vctx = (dtvideo_context_t *) decoder->parent;
    dtvideo_filter_t *filter = (dtvideo_filter_t *) &(vctx->video_filt);
    queue_t *picture_queue = vctx->vo_queue;
    /*used for decode */
    dt_av_pic_t *picture = NULL;
    int ret;
    dt_info (TAG, "[%s:%d] start decode loop \n", __FUNCTION__, __LINE__);

    do
    {
        //exit check before idle, maybe recieve exit cmd in idle status
        if (decoder->status == VDEC_STATUS_EXIT)
        {
            dt_info (TAG, "[%s:%d] receive decode loop exit cmd \n", __FUNCTION__, __LINE__);
            break;
        }

        if (decoder->status == VDEC_STATUS_IDLE)
        {
            dt_info (TAG, "[%s:%d] Idle status ,please wait \n", __FUNCTION__, __LINE__);
            usleep (100);
            continue;
        }

        /*read frame */
        if (!decoder->parent)
        {
            usleep (100);
            continue;
        }

        if (picture_queue->length >= VIDEO_OUT_MAX_COUNT)
        {
            //vo queue full
            usleep (1000);
            continue;
        }
        ret = dtvideo_read_frame (decoder->parent, &frame);
        if (ret < 0)
        {
            if(decoder->pts_first == -1 || decoder->pts_first == DT_NOPTS_VALUE)
            {
                usleep(1000);
                continue;
            }
            //no data left, maybe eof, need to flush left data
            memset(&frame,0,sizeof(dt_av_frame_t));
            dt_info (TAG, "[%s:%d] no video frame left, flush left frames \n", __FUNCTION__, __LINE__);
        }
        /*read one frame,enter decode frame module */
        //will exec once for one time
        ret = wrapper->decode_frame (decoder, &frame, &picture);
        if (ret <= 0)
        {
            decoder->decode_err_cnt++;
            dt_debug (TAG, "[%s:%d]decode failed \n", __FUNCTION__, __LINE__);
            picture = NULL;
            usleep(10000);
            goto DECODE_END;
        }
        if (!picture)
            goto DECODE_END;

        //got one frame, filter process
        if(wrapper->info_changed(decoder))
        {
            memcpy(&decoder->para, &wrapper->para, sizeof(dtvideo_para_t));
            memcpy(&filter->para, &wrapper->para, sizeof(dtvideo_para_t));
            video_filter_reset(filter, &decoder->para);
        }

        video_filter_process(filter, picture);

        decoder->frame_count++;
        //Got one frame
        //picture->pts = frame.pts;
        
        //update current pts, clear the buffer size
        if (frame.pts >= 0 && decoder->pts_first == -1)
        {
            //we will use first pts to estimate pts
            dt_info (TAG, "[%s:%d]first frame decoded ok, pts:0x%llx dts:0x%llx duration:%d size:%d\n", __FUNCTION__, __LINE__, frame.pts, frame.dts, frame.duration, frame.size);
            decoder->pts_first = pts_exchange (decoder, picture->pts);
            decoder->pts_current = decoder->pts_first;
        }
        else
        {
            if(pts_mode)
            {
                int fps = decoder->para.fps;
                float dur_inc = 90000/fps;
                picture->pts = decoder->pts_current + dur_inc;
                decoder->pts_current = picture->pts; 
            }
        }

        /*queue in */
        queue_push_tail (picture_queue, picture);
        picture = NULL;
      DECODE_END:
        //we successfully decodec one frame
        if (frame.data)
        {
            if(frame.data)
                free (frame.data);
            frame.data = NULL;
            frame.size = 0;
            frame.pts = -1;
        }
    }
    while (1);
    dt_info (TAG, "[file:%s][%s:%d]decoder loop thread exit ok\n", __FILE__, __FUNCTION__, __LINE__);

    //filter release
    video_filter_stop(filter);

    pthread_exit (NULL);
    return NULL;
}

int video_decoder_init (dtvideo_decoder_t * decoder)
{
    int ret = 0;
    pthread_t tid;
    /*select decoder */
    ret = select_video_decoder (decoder);
    if (ret < 0)
        return -1;

    vd_wrapper_t *wrapper = decoder->wrapper; 
    /*init decoder */
    decoder->pts_current = decoder->pts_first = -1;
    decoder->vd_priv = decoder->para.avctx_priv;
    ret = wrapper->init (decoder);
    if (ret < 0)
        return -1;
    
    pts_mode = 0;    
    char value[512];
    if(GetEnv("VIDEO","pts.mode",value) > 0)
    {
        pts_mode = atoi(value);
        dt_info(TAG,"pts mode:%d fps:%f \n",pts_mode,decoder->para.fps);
    }
    
    dt_info (TAG, "[%s:%d] video decoder init ok\n", __FUNCTION__, __LINE__);
    /*init pcm buffer */
    dtvideo_context_t *vctx = (dtvideo_context_t *) decoder->parent;
    vctx->vo_queue = queue_new ();
    queue_t *picture_queue = vctx->vo_queue;
    if (NULL == picture_queue)
    {
        dt_error (TAG, "create video out queue failed\n");
        return -1;
    }
    /*create thread */
    ret = pthread_create (&tid, NULL, video_decode_loop, (void *) decoder);
    if (ret != 0)
    {
        dt_error (TAG, "create audio decoder thread failed\n");
        return ret;
    }
    decoder->video_decoder_pid = tid;
    video_decoder_start (decoder);
    //decoder->status=ADEC_STATUS_RUNNING;//start decode after init
    return 0;
}

int video_decoder_start (dtvideo_decoder_t * decoder)
{
    decoder->status = VDEC_STATUS_RUNNING;
    return 0;
}

void dtpicture_free (void *pic)
{

    AVPicture_t *picture = (AVPicture_t *) (pic);
    if (picture->data)
        free (picture->data[0]);
    return;
}

int video_decoder_stop (dtvideo_decoder_t * decoder)
{
    vd_wrapper_t *wrapper = decoder->wrapper;
    
    /*Decode thread exit */
    decoder->status = VDEC_STATUS_EXIT;
    pthread_join (decoder->video_decoder_pid, NULL);
    wrapper->release (decoder);
   
    /*release queue */
    dtvideo_context_t *vctx = (dtvideo_context_t *) decoder->parent;
    queue_t *picture_queue = vctx->vo_queue;
    if (picture_queue)
    {
        queue_free (picture_queue, (free_func) dtpicture_free);
        picture_queue = NULL;
    }
    return 0;
}
