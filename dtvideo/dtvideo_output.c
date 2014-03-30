#include "dtvideo_output.h"
#include "dtvideo_decoder.h"
#include "dtvideo.h"

#define TAG "VIDEO-OUT"

#define REGISTER_VO(X, x)	 	\
	if( ENABLE_VO_##X ) 		\
	{							\
		extern vo_operations_t vo_##x##_ops; \
		register_vo(&vo_##x##_ops); \
	}

static int64_t last_time = -1;

static vo_operations_t *first_vo = NULL;

static void register_vo (vo_operations_t * vo)
{
    vo_operations_t **p;
    p = &first_vo;
    while (*p != NULL)
        p = &(*p)->next;
    *p = vo;
    vo->next = NULL;
    dt_info (TAG, "register vo. id:%d name:%s \n", vo->id, vo->name);
}

void vout_register_all ()
{
    /*Register all audio_output */
    //REGISTER_VO(NULL, null);
#ifdef ENABLE_VO_SDL
    REGISTER_VO (SDL, sdl);
#endif
#ifdef ENABLE_VO_SDL2
    REGISTER_VO (SDL2, sdl2);
#endif
    return;
}

/*default alsa*/
int select_vo_device (dtvideo_output_t * vo, int id)
{
    vo_operations_t **p;
    p = &first_vo;

    if(id == -1) // user did not choose vo,use default one
    {
        if(!*p)
            return -1;
        vo->vout_ops = *p;
        dt_info(TAG,"SELECT VO:%s \n",(*p)->name);
        return 0;
    }

    while (*p != NULL && (*p)->id != id)
    {
        p = &(*p)->next;
    }
    if (!*p)
    {
        dt_error (TAG, "no valid vo device found\n");
        return -1;
    }
    vo->vout_ops = *p;
    dt_info(TAG,"SELECT VO:%s \n",(*p)->name);
    return 0;
}

int video_output_start (dtvideo_output_t * vo)
{
    /*start playback */
    vo->status = VO_STATUS_RUNNING;
    return 0;
}

int video_output_pause (dtvideo_output_t * vo)
{
    vo->status = VO_STATUS_PAUSE;
    return 0;
}

int video_output_resume (dtvideo_output_t * vo)
{
    vo->status = VO_STATUS_RUNNING;
    return 0;
}

int video_output_stop (dtvideo_output_t * vo)
{
    vo->status = VO_STATUS_EXIT;
    pthread_join (vo->output_thread_pid, NULL);
    vo->vout_ops->vo_stop (vo);
    dt_info (TAG, "[%s:%d] vout stop ok \n", __FUNCTION__, __LINE__);
    return 0;
}

int video_output_latency (dtvideo_output_t * vo)
{
    return 0;
#if 0
    if (ao->status == AO_STATUS_IDLE)
        return 0;
    if (ao->status == AO_STATUS_PAUSE)
        return ao->last_valid_latency;
    ao->last_valid_latency = ao->aout_ops->ao_latency (ao);
    return ao->last_valid_latency;
#endif
}

int video_output_get_level (dtvideo_output_t * ao)
{
    return 0;
    //return ao->state.aout_buf_level;
}

//output one frame to output gragh
//using pts

#define REFRESH_DURATION 10*1000 //us
static void *video_output_thread (void *args)
{
    dtvideo_output_t *vo = (dtvideo_output_t *) args;
    int ret, wlen;
    ret = wlen = 0;
    AVPicture_t *picture_pre;
    AVPicture_t *picture;
    AVPicture_t *pic;
    int64_t sys_clock;          //contrl video display
    int64_t cur_time, time_diff;
    dtvideo_context_t *vctx = vo->parent;
    for (;;)
    {
        if (vo->status == VO_STATUS_EXIT)
            goto EXIT;
        if (vo->status == VO_STATUS_IDLE || vo->status == VO_STATUS_PAUSE)
        {
            usleep (50 * 1000);
            continue;
        }
        /*pre read picture and update sys time */
        picture_pre = (AVPicture_t *) dtvideo_output_pre_read (vo->parent);
        if (!picture_pre)
        {
            dt_debug (TAG, "[%s:%d]frame read failed ! \n", __FUNCTION__, __LINE__);
            usleep (1000);
            continue;
        }
        cur_time = (int64_t) dt_gettime ();
        sys_clock = dtvideo_get_systime (vo->parent);
        if (sys_clock == -1)
        {
            dt_info (TAG, "FIRST SYSCLOK:%lld \n", picture_pre->pts);
            sys_clock = picture_pre->pts;
        }
        if (last_time == -1)
            last_time = cur_time;
        time_diff = cur_time - last_time;
        sys_clock += (time_diff * 9) / 100;
        dt_debug (TAG, "time_diff:%lld pts_inc:%lld sys_clock:%lld nextpts:%lld cur_time:%llu last_time:%llu\n", time_diff, time_diff * 9 / 100, sys_clock, picture_pre->pts, cur_time, last_time);
        last_time = cur_time;
        if (picture_pre->pts == -1) //invalid pts, calc using last pts
            picture_pre->pts = vctx->current_pts + 90000 / vo->para.fps;
        //update sys time
        dtvideo_update_systime (vo->parent, sys_clock);
        //maybe need to block
        if (sys_clock < picture_pre->pts)
        {
            dt_usleep (REFRESH_DURATION);
            continue;
        }
        /*read data from filter or decode buffer */
        picture = (AVPicture_t *) dtvideo_output_read (vo->parent);
        if (!picture)
        {
            dt_error (TAG, "[%s:%d]frame read failed ! \n", __FUNCTION__, __LINE__);
            usleep (1000);
            continue;
        }
        pic = (AVPicture_t *) picture;

        //update pts
        if (vctx->last_valid_pts == -1)
            vctx->last_valid_pts = vctx->current_pts = picture->pts;
        else
        {
            vctx->last_valid_pts = vctx->current_pts;
            vctx->current_pts = picture->pts;
            //printf("[%s:%d]!update pts:%llu \n",__FUNCTION__,__LINE__,vctx->current_pts);
        }
        /*read next frame ,check drop frame */
        picture_pre = (AVPicture_t *) dtvideo_output_pre_read (vo->parent);
        if (picture_pre)
        {
            if (picture_pre->pts == -1) //invalid pts, calc using last pts
            {
                dt_debug (TAG, "can not get vpts from frame,estimate using fps:%d  \n", vo->para.fps);
                picture_pre->pts = vctx->current_pts + 90000 / vo->para.fps;
            }
            if (sys_clock >= picture_pre->pts)
            {
                dt_debug (TAG, "drop frame,sys clock:%lld thispts:%lld next->pts:%lld \n", sys_clock, picture->pts, picture_pre->pts);
                dtpicture_free (pic);
                free(picture);
                continue;
            }
        }
        /*display picture & update vpts */
        ret = vo->vout_ops->vo_render (vo, pic);
        if (ret < 0)
        {
            printf ("frame toggle failed! \n");
            usleep (1000);
        }

        /*update vpts */
        dtvideo_update_pts (vo->parent);
        dtpicture_free (pic);
        free(picture);
        dt_usleep (REFRESH_DURATION);
    }
  EXIT:
    dt_info (TAG, "[file:%s][%s:%d]ao playback thread exit\n", __FILE__, __FUNCTION__, __LINE__);
    pthread_exit (NULL);
    return NULL;
}

int video_output_init (dtvideo_output_t * vo, int vo_id)
{
    int ret = 0;
    pthread_t tid;
    
    /*select ao device */
    ret = select_vo_device (vo, vo_id);
    if (ret < 0)
        return -1;
    vo->vout_ops->vo_init (vo);
    dt_info (TAG, "[%s:%d] video output init success\n", __FUNCTION__, __LINE__);

    /*start aout pthread */
    ret = pthread_create (&tid, NULL, video_output_thread, (void *) vo);
    if (ret != 0)
    {
        dt_error (TAG, "[%s:%d] create video output thread failed\n", __FUNCTION__, __LINE__);
        return ret;
    }
    vo->output_thread_pid = tid;
    dt_info (TAG, "[%s:%d] create video output thread success\n", __FUNCTION__, __LINE__);
    return 0;
}

uint64_t video_output_get_latency (dtvideo_output_t * vo)
{
    return 0;
}
