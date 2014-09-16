#include "dtvideo.h"
#include "dtvideo_filter.h"

#define TAG "VIDEO-FILTER"

#define REGISTER_VF(X,x)	 	\
	{							\
		extern vf_wrapper_t vf_##x##_ops; 	\
		register_vf(&vf_##x##_ops); 	\
	}

static vf_wrapper_t *g_vf = NULL;

static void register_vf (vf_wrapper_t * vf)
{
    vf_wrapper_t **p;
    p = &g_vf;
    while (*p != NULL)
        p = &(*p)->next;
    *p = vf;
    dt_info (TAG, "[%s:%d] register vf, name:%s \n", __FUNCTION__, __LINE__, (*p)->name);
    vf->next = NULL;
}

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
    
    dt_info (TAG, "[%s:%d]register ext vf. name:%s \n",__FUNCTION__,__LINE__, vf->name);

}

void vf_register_all ()
{
#ifdef ENABLE_VF_FFMPEG
    REGISTER_VF (FFMPEG, ffmpeg);
#endif
    return;
}

static int select_vf(dtvideo_filter_t *filter)
{
    if(!g_vf)
        return -1;
    filter->wrapper = g_vf;
    dt_info (TAG, "[%s:%d] select--%s video filter \n", __FUNCTION__, __LINE__, g_vf->name);
    return 0;
}

static int video_filter_init(dtvideo_filter_t *filter)
{
    int ret = select_vf(filter);
    if(ret < 0)
        return ret;
    vf_wrapper_t *wrapper = filter->wrapper;
    ret = wrapper->init(filter);
    return ret;
}

int video_filter_reset(dtvideo_filter_t *filter, dtvideo_para_t *para)
{
    if(filter->status == VF_STATUS_IDLE) // no need reset
        return 0;
    return 0;
}

int video_filter_process(dtvideo_filter_t *filter, dt_av_frame_t *pic)
{
    int ret = 0;
    //init first
    if(filter->status == VF_STATUS_IDLE)
    {
        ret = video_filter_init(filter);
        if(ret < 0)
        {
            dt_info (TAG, "[%s:%d]vf init failed \n",__FUNCTION__,__LINE__);
            ret = -1;
            goto END;
        }
        filter->status = VF_STATUS_RUNNING;
        dt_info (TAG, "[%s:%d]vf init ok \n",__FUNCTION__,__LINE__);
    }

    vf_wrapper_t *wrapper = filter->wrapper;
    ret = wrapper->process(filter, pic);
END:
    return ret;
}

int video_filter_stop(dtvideo_filter_t *filter)
{
    vf_wrapper_t *wrapper = filter->wrapper;
    wrapper->release(filter);
    filter->status = VF_STATUS_IDLE;
    return 0;
}
