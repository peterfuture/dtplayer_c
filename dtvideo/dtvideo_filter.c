/***********************************************************************
**
**  Module : dtvideo_filter.c
**  Summary: video pp wrapper
**  Section: dtvideo
**  Author : peter
**  Notes  : 
**
***********************************************************************/

#include "dtvideo.h"
#include "dtvideo_filter.h"

#define TAG "VIDEO-FILTER"

#define REGISTER_VF(X,x)	 	\
	{							\
		extern vf_wrapper_t vf_##x##_ops; 	\
		register_vf(&vf_##x##_ops); 	\
	}

static vf_wrapper_t *g_vf = NULL;


/***********************************************************************
**
** register internal filter
**
***********************************************************************/
static void register_vf(vf_wrapper_t * vf)
{
    vf_wrapper_t **p;
    p = &g_vf;
    while (*p != NULL)
    {
        p = &(*p)->next;
    }
    *p = vf;
    dt_info (TAG, "[%s:%d] register internal vf, name:%s \n", __FUNCTION__, __LINE__, (*p)->name);
    vf->next = NULL;
    return;
}

/***********************************************************************
**
** register external filter
**
***********************************************************************/
void vf_register_ext(vf_wrapper_t *vf)
{
    vf_wrapper_t **p;
    p = &g_vf;
    if(*p == NULL)
    {
        *p = vf;
        vf->next = NULL;
    }
    else
    {
        vf->next = *p;
        *p = vf;
    }
    dt_info (TAG, "[%s:%d]register external vf. name:%s \n",__FUNCTION__,__LINE__, vf->name);
    return;
}

/***********************************************************************
**
** register all internal filters
**
***********************************************************************/
void vf_register_all ()
{
#ifdef ENABLE_VF_FFMPEG
    REGISTER_VF (FFMPEG, ffmpeg);
#endif
    return;
}

void vf_remove_all ()
{
    g_vf = NULL;
}

/***********************************************************************
**
** select first vf with capbilities cap
** cap: vf capbilities set
**
***********************************************************************/
static int select_vf(dtvideo_filter_t *filter, vf_cap_t cap)
{
    int ret = -1;
    vf_wrapper_t *vf = g_vf;

    if(!vf)
        return ret;

    while(vf != NULL)
    {
        if(vf->capable(cap) == cap) // maybe cap have several elements
        {
            ret = 0;
            break;
        }
        vf = vf->next;
    }

    filter->wrapper = vf;
    if(vf) 
        dt_info (TAG, "[%s:%d] %s video filter selected \n", __FUNCTION__, __LINE__, g_vf->name);
    else
        dt_info (TAG, "[%s:%d] No video filter selected \n", __FUNCTION__, __LINE__, g_vf->name);
    return ret;
}

/***********************************************************************
**
** Init video filter
**
***********************************************************************/
int video_filter_init(dtvideo_filter_t *filter)
{
    dt_lock_init(&filter->mutex, NULL);
    dt_lock(&filter->mutex);

    int cap = 0;
    dtvideo_para_t *para = &filter->para;
    if(para->s_pixfmt != para->d_pixfmt)
        cap |= VF_CAP_COLORSPACE_CONVERT;

    if(para->s_width != para->d_width || para->s_height != para->d_height)
        cap |= VF_CAP_CLIP;

    int ret = select_vf(filter, cap);
    if(ret < 0)
        goto EXIT;

    vf_wrapper_t *wrapper = filter->wrapper;
    ret = wrapper->init(filter);
    if(ret >= 0)
        filter->status = VF_STATUS_RUNNING;
EXIT:
    if(ret < 0)
    {
        filter->wrapper = NULL;
        ret = 0;
    }

    dt_unlock(&filter->mutex);
    return ret;
}

/***********************************************************************
**
** update video filter
** reset para in filter first 
**
***********************************************************************/
int video_filter_update(dtvideo_filter_t *filter)
{
    video_filter_stop(filter);
    video_filter_init(filter);
    return 0;
}

/***********************************************************************
**
** process one frame
**
***********************************************************************/
int video_filter_process(dtvideo_filter_t *filter, dt_av_frame_t *frame)
{
    int ret = 0;

    if(filter->status == VF_STATUS_IDLE) // No need to Process
    {
        return 0;
    }
    dt_lock(&filter->mutex);
    vf_wrapper_t *wrapper = filter->wrapper;
    ret = wrapper->process(filter, frame);
    dt_unlock(&filter->mutex);
    return ret;
}

/***********************************************************************
**
** stop filter
**
***********************************************************************/
int video_filter_stop(dtvideo_filter_t *filter)
{
    dt_lock(&filter->mutex);
    vf_wrapper_t *wrapper = filter->wrapper;
    if(!wrapper)
        goto EXIT;
    if(filter->status == VF_STATUS_RUNNING)
        wrapper->release(filter);
EXIT:
    filter->status = VF_STATUS_IDLE;
    dt_unlock(&filter->mutex);
    return 0;
}
