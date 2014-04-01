#include "dtvideo.h"
#include "dtvideo_decoder.h"

//#define DTVIDEO_DECODER_DUMP 0

#define TAG "VIDEO-DEC"

#define REGISTER_VDEC(X,x)	 	\
	{							\
		extern dec_video_wrapper_t vdec_##x##_ops; 	\
		register_vdec(&vdec_##x##_ops); 	\
	}
static dec_video_wrapper_t *first_vdec = NULL;

static void register_vdec (dec_video_wrapper_t * vdec)
{
    dec_video_wrapper_t **p;
    p = &first_vdec;
    while (*p != NULL)
        p = &(*p)->next;
    *p = vdec;
    dt_info (TAG, "[%s:%d] register vdec, name:%s fmt:%d \n", __FUNCTION__, __LINE__, (*p)->name, (*p)->vfmt);
    vdec->next = NULL;
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
    dec_video_wrapper_t **p;
    p = &first_vdec;
    if (!*p)
    {
        dt_error (TAG, "[%s:%d] select no video decoder \n", __FUNCTION__, __LINE__);
        return -1;
    }
    decoder->dec_wrapper = *p;
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
    dec_video_wrapper_t *dec_wrapper = decoder->dec_wrapper;
    dtvideo_context_t *vctx = (dtvideo_context_t *) decoder->parent;
    queue_t *picture_queue = vctx->vo_queue;
    /*used for decode */
    AVPicture_t *picture = NULL;
    int ret;
    dt_info (TAG, "[%s:%d] start decode loop \n", __FUNCTION__, __LINE__);

    do
    {
        if (decoder->status == VDEC_STATUS_IDLE)
        {
            dt_info (TAG, "[%s:%d] Idle status ,please wait \n", __FUNCTION__, __LINE__);
            usleep (1000);
            continue;
        }
        if (decoder->status == VDEC_STATUS_EXIT)
        {
            dt_info (TAG, "[%s:%d] receive decode loop exit cmd \n", __FUNCTION__, __LINE__);
            break;
        }

        /*read frame */
        if (!decoder->parent)
        {
            usleep (1000);
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
            usleep (1000);
            dt_debug (TAG, "[%s:%d] dtaudio decoder loop read frame failed \n", __FUNCTION__, __LINE__);
            continue;
        }
        //update current pts, clear the buffer size
        if (frame.pts >= 0 && decoder->pts_first == -1)
        {
            //we will use first pts to estimate pts
            dt_info (TAG, "[%s:%d]first frame: pts:%llu dts:%llu duration:%d size:%d\n", __FUNCTION__, __LINE__, frame.pts, frame.dts, frame.duration, frame.size);
            decoder->pts_first = pts_exchange (decoder, frame.pts);

        }

        /*read one frame,enter decode frame module */
        //will exec once for one time
        ret = dec_wrapper->decode_frame (decoder, &frame, &picture);
        if (ret <= 0)
        {
            decoder->decode_err_cnt++;
            dt_error (TAG, "[%s:%d]decode failed \n", __FUNCTION__, __LINE__);
            picture = NULL;
            goto DECODE_END;
        }
        if (!picture)
            goto DECODE_END;
        decoder->frame_count++;
        //Got one frame
        //picture->pts = frame.pts;
        /*queue in */
        queue_push_tail (picture_queue, picture);
        picture = NULL;
      DECODE_END:
        //we successfully decodec one frame
        if (frame.data)
        {
            free (frame.data);
            frame.data = NULL;
            frame.size = 0;
            frame.pts = -1;
        }
    }
    while (1);
    dt_info (TAG, "[file:%s][%s:%d]decoder loop thread exit ok\n", __FILE__, __FUNCTION__, __LINE__);
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

    /*init decoder */
    decoder->pts_current = decoder->pts_first = -1;
    decoder->decoder_priv = decoder->para.avctx_priv;
    ret = decoder->dec_wrapper->init (decoder);
    if (ret < 0)
        return -1;

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
    /*Decode thread exit */
    decoder->status = VDEC_STATUS_EXIT;
    pthread_join (decoder->video_decoder_pid, NULL);
     /**/ decoder->dec_wrapper->release (decoder);
    /*uninit buf */
    dtvideo_context_t *vctx = (dtvideo_context_t *) decoder->parent;
    //uninit_buf(&vctx->video_decoded_buf);     
    queue_t *picture_queue = vctx->vo_queue;
    if (picture_queue)
    {
        queue_free (picture_queue, (free_func) dtpicture_free);
        picture_queue = NULL;
    }
    return 0;
}
