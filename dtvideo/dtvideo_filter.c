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
#include "dtp_vf.h"

#define TAG "VIDEO-FILTER"

#define REGISTER_VF(X,x)        \
    {                           \
        extern vf_wrapper_t vf_##x##_ops;   \
        register_vf(&vf_##x##_ops);     \
    }

static vf_wrapper_t *g_vf = NULL;


/***********************************************************************    **
** register internal filter
**
***********************************************************************/
static void register_vf(vf_wrapper_t * vf)
{
    vf_wrapper_t **p;
    p = &g_vf;
    while (*p != NULL) {
        p = &(*p)->next;
    }
    *p = vf;
    dt_info(TAG, "[%s:%d] register internal vf, name:%s \n", __FUNCTION__, __LINE__,
            (*p)->name);
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
    if (*p == NULL) {
        *p = vf;
        vf->next = NULL;
    } else {
        vf->next = *p;
        *p = vf;
    }
    dt_info(TAG, "[%s:%d]register external vf. name:%s \n", __FUNCTION__, __LINE__,
            vf->name);
    return;
}

/***********************************************************************
**
** register all internal filters
**
***********************************************************************/
void vf_register_all()
{
#ifdef ENABLE_VF_FFMPEG
    REGISTER_VF(FFMPEG, ffmpeg);
#endif
    return;
}

void vf_remove_all()
{
    g_vf = NULL;
}

/***********************************************************************
**
** select first vf with capbilities cap
** cap: vf capbilities set
**
***********************************************************************/
static vf_wrapper_t *select_vf(vf_cap_t cap)
{
    vf_wrapper_t *vf = g_vf;

    if (!vf) {
        return NULL;
    }

    while (vf != NULL) {
        if (vf->capable(cap) == cap) { // maybe cap have several elements
            dt_info(TAG, "[%s:%d] %s video filter selected \n", __FUNCTION__, __LINE__,
                    vf->name);
            return vf;
        }
        vf = vf->next;
    }
    return NULL;
}

static vf_context_t *alloc_vf_context(vf_wrapper_t *wrapper)
{
    vf_context_t *vfc = (vf_context_t *)malloc(sizeof(vf_context_t));
    if (!vfc) {
        return NULL;
    }
    if (wrapper->private_data_size > 0) {
        vfc->private_data = malloc(wrapper->private_data_size);
        if (!vfc->private_data) {
            free(vfc);
            return NULL;
        }
    }
    vfc->wrapper = wrapper;
    return vfc;
}

static void free_vf_context(vf_context_t *vfc)
{
    if (!vfc) {
        return;
    }
    if (!vfc->private_data) {
        free(vfc->private_data);
    }
    free(vfc);
    return;
}

/***********************************************************************
**
** Init video filter
**
***********************************************************************/
int video_filter_init(dtvideo_filter_t *filter)
{
    int cap = 0;
    dtvideo_para_t *para = &filter->para;
    if (para->s_pixfmt != para->d_pixfmt) {
        cap |= VF_CAP_COLORSPACE_CONVERT;
    }

    if (para->s_width != para->d_width || para->s_height != para->d_height) {
        cap |= VF_CAP_CLIP;
    }

    vf_wrapper_t *wrapper = select_vf(cap);
    if (!wrapper) {
        return -1;
    }

    vf_context_t *vfc = alloc_vf_context(wrapper);
    if (!vfc) {
        return -1;
    }
    memcpy(&vfc->para, para, sizeof(dtvideo_para_t));
    filter->vfc = vfc;
    int ret = wrapper->init(vfc);
    if (ret < 0) {
        goto FAIL;
    }
    filter->status = VF_STATUS_RUNNING;
    return 0;
FAIL:
    if (vfc) {
        free_vf_context(vfc);
    }
    return -1;
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

    if (filter->status == VF_STATUS_IDLE) { // No need to Process
        return 0;
    }
    vf_context_t *vfc = filter->vfc;
    vf_wrapper_t *wrapper = vfc->wrapper;
    ret = wrapper->process(vfc, frame);
    return ret;
}

/***********************************************************************
**
** stop filter
**
***********************************************************************/
int video_filter_stop(dtvideo_filter_t *filter)
{
    vf_context_t *vfc = filter->vfc;
    if (!vfc) {
        return 0;
    }
    vf_wrapper_t *wrapper = vfc->wrapper;
    if (!wrapper) {
        goto EXIT;
    }
    if (filter->status == VF_STATUS_RUNNING) {
        wrapper->release(vfc);
        free_vf_context(vfc);
    }
EXIT:
    filter->status = VF_STATUS_IDLE;
    return 0;
}
