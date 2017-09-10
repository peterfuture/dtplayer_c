/***********************************************************************
**
**  Module : dtsub_output.c
**  Summary: sub render module
**  Section: dtsub
**  Author : peter
**  Notes  :
**
***********************************************************************/

#include "dtsub_output.h"
#include "dtsub_decoder.h"
#include "dtsub.h"

#define TAG "SUB-OUT"

#define REGISTER_SO(X, x)       \
    {                           \
        extern so_wrapper_t so_##x##_ops; \
        register_so(&so_##x##_ops); \
    }

static so_wrapper_t *g_so = NULL;

/***********************************************************************
**
** register_so
**
** - register sub out
**
***********************************************************************/
static void register_so(so_wrapper_t * so)
{
    so_wrapper_t **p;
    p = &g_so;
    while (*p != NULL) {
        p = &((*p)->next);
    }
    *p = so;
    so->next = NULL;
    dt_info(TAG, "register so. id:%d name:%s \n", so->id, so->name);
}

/***********************************************************************
**
** so_register_ext
**
** - register external sub out
**
***********************************************************************/
void so_register_ext(so_wrapper_t * so)
{
    so_wrapper_t **p;
    p = &g_so;
    if (*p == NULL) {
        *p = so;
        so->next = NULL;
    } else {
        so->next = *p;
        *p = so;
    }

    dt_info(TAG, "register ext so. id:%d name:%s \n", so->id, so->name);
}

/***********************************************************************
**
** so_register_ext
**
** - register internal sub out
**
***********************************************************************/
void so_register_all()
{
    REGISTER_SO(NULL, null);
    return;
}

/***********************************************************************
**
** so_remove_all
**
***********************************************************************/
void so_remove_all()
{
    g_so = NULL;
}

/***********************************************************************
**
** select_so_device
**
***********************************************************************/
so_wrapper_t *select_so_device(int id)
{
    so_wrapper_t **p;
    p = &g_so;

    if (id == -1) { // user did not choose vo,use default one
        if (!*p) {
            return NULL;
        }
        dt_info(TAG, "SELECT SO:%s \n", (*p)->name);
        return *p;
    }

    while (*p != NULL && (*p)->id != id) {
        p = &(*p)->next;
    }
    if (!*p) {
        dt_error(TAG, "no valid so device found\n");
        return NULL;
    }
    dt_info(TAG, "SELECT SO:%s \n", (*p)->name);
    return *p;
}

static so_context_t *alloc_so_context(so_wrapper_t *wrapper)
{
    so_context_t *soc = (so_context_t *)malloc(sizeof(so_context_t));
    if (!soc) {
        return NULL;
    }
    if (wrapper->private_data_size > 0) {
        soc->private_data = malloc(wrapper->private_data_size);
        if (!soc->private_data) {
            free(soc);
            return NULL;
        }
    }
    soc->wrapper = wrapper;
    return soc;
}

static void free_so_context(so_context_t *soc)
{
    if (!soc) {
        return;
    }
    if (!soc->private_data) {
        free(soc->private_data);
    }
    free(soc);
    return;
}


/***********************************************************************
**
** sub_output_start
**
***********************************************************************/
int sub_output_start(dtsub_output_t * so)
{
    so->status = SO_STATUS_RUNNING;
    return 0;
}

/***********************************************************************
**
** sub_output_pause
**
***********************************************************************/
int sub_output_pause(dtsub_output_t * so)
{
    so->status = SO_STATUS_PAUSE;
    return 0;
}

/***********************************************************************
**
** sub_output_resume
**
***********************************************************************/
int sub_output_resume(dtsub_output_t * so)
{
    so->status = SO_STATUS_RUNNING;
    return 0;
}

/***********************************************************************
**
** sub_output_stop
**
***********************************************************************/
int sub_output_stop(dtsub_output_t * so)
{
    so_context_t *soc = so->soc;
    so->status = SO_STATUS_EXIT;
    pthread_join(so->output_thread_pid, NULL);
    if (soc) {
        so_wrapper_t *wrapper = soc->wrapper;
        wrapper->so_stop(soc);
        free_so_context(soc);
    }
    dt_info(TAG, "[%s:%d] sout stop ok \n", __FUNCTION__, __LINE__);
    return 0;
}

/***********************************************************************
**
** dtsub_frame_free
**
***********************************************************************/
static void dtsub_frame_free(void *frame)
{
    return;
    int i;

    dtav_sub_frame_t *sub = (dtav_sub_frame_t *)(frame);
    for (i = 0; i < sub->num_rects; i++) {
        free(&sub->rects[i]->pict.data[0]);
        free(&sub->rects[i]->pict.data[1]);
        free(&sub->rects[i]->pict.data[2]);
        free(&sub->rects[i]->pict.data[3]);
        free(&sub->rects[i]->text);
        free(&sub->rects[i]->ass);
        free(&sub->rects[i]);
    }

    free(&sub->rects);
    memset(sub, 0, sizeof(dtav_sub_frame_t));
    return;
}

/***********************************************************************
**
** sub_output_thread
**
** - render one frame consider pts
**
***********************************************************************/
#define REFRESH_DURATION 10*1000 //us
static void *sub_output_thread(void *args)
{
    dtsub_output_t *so = (dtsub_output_t *) args;
    dtsub_context_t *sctx = (dtsub_context_t *) so->parent;
    so_context_t *soc = so->soc;
    so_wrapper_t *wrapper = soc->wrapper;
    int ret, wlen;
    ret = wlen = 0;
    dtav_sub_frame_t *frame_pre;
    dtav_sub_frame_t *frame;
    int64_t sys_time;          //contrl sub display

    for (;;) {
        if (so->status == SO_STATUS_EXIT) {
            goto EXIT;
        }
        if (so->status == SO_STATUS_IDLE || so->status == SO_STATUS_PAUSE) {
            usleep(100);
            continue;
        }
        /*pre read frame and update sys time */
        frame_pre = (dtav_sub_frame_t *)dtsub_output_pre_read(so->parent);
        if (!frame_pre) {
            dt_debug(TAG, "[%s:%d]frame read failed ! \n", __FUNCTION__, __LINE__);
            usleep(100);
            continue;
        }
        sys_time = dtsub_get_systime(so->parent);
        if (sys_time == -1) {
            continue; // av not ready
        }
        if (frame_pre->pts == -1) { //invalid pts, calc using last pts
            dt_error(TAG, "Err: sub frame pts invalid \n");
        }
        dt_info(TAG, "read one sub frame, pts:%lld systime:%lld \n", frame_pre->pts,
                sys_time);
        //maybe need to block
        if (sys_time < frame_pre->pts) {
            dt_debug(TAG, "[%s:%d] not to show ! \n", __FUNCTION__, __LINE__);
            dt_usleep(REFRESH_DURATION);
            continue;
        }
        /*read data from decoder buffer */
        frame = (dtav_sub_frame_t *) dtsub_output_read(so->parent);
        if (!frame) {
            dt_error(TAG, "[%s:%d]frame read failed ! \n", __FUNCTION__, __LINE__);
            usleep(1000);
            continue;
        }

        //update pts
        if (sctx->last_valid_pts == -1) {
            sctx->last_valid_pts = sctx->current_pts = frame->pts;
        } else {
            sctx->last_valid_pts = sctx->current_pts;
            sctx->current_pts = frame->pts;
            //printf("[%s:%d]!update pts:%llu \n",__FUNCTION__,__LINE__,sctx->current_pts);
        }
        /*read next frame ,check drop frame */
        frame_pre = (dtav_sub_frame_t *) dtsub_output_pre_read(so->parent);
        if (frame_pre) {
            if (sys_time >= frame_pre->pts) {
                dt_info(TAG, "drop frame,sys time:%lld thispts:%lld next->pts:%lld \n",
                        sys_time, frame->pts, frame_pre->pts);
                dtsub_frame_free(frame);
                free(frame);
                continue;
            }
        }
        ret = wrapper->so_render(soc, frame);
        if (ret < 0) {
            dt_error(TAG, "frame toggle failed! \n");
            usleep(1000);
        }

        /*update vpts */
        dtsub_update_pts(so->parent);
        dtsub_frame_free(frame);
        free(frame);
        dt_usleep(REFRESH_DURATION);
    }
EXIT:
    dt_info(TAG, "[file:%s][%s:%d]so playback thread exit\n", __FILE__,
            __FUNCTION__, __LINE__);
    pthread_exit(NULL);
    return NULL;
}

/***********************************************************************
**
** sub_output_init
**
***********************************************************************/
int sub_output_init(dtsub_output_t * so, int so_id)
{
    int ret = 0;
    pthread_t tid;

    /*select ao device */
    so_wrapper_t *wrapper = select_so_device(so_id);
    if (wrapper == NULL) {
        return -1;
    }

    so_context_t *soc = alloc_so_context(wrapper);
    if (!soc) {
        return -1;
    }
    memcpy(&soc->para, so->para, sizeof(dtsub_para_t));
    so->soc = soc;
    wrapper->so_init(soc);
    dt_info(TAG, "[%s:%d] sub output init success\n", __FUNCTION__, __LINE__);

    /*start aout pthread */
    ret = pthread_create(&tid, NULL, sub_output_thread, (void *) so);
    if (ret != 0) {
        dt_error(TAG, "[%s:%d] create sub output thread failed\n", __FUNCTION__,
                 __LINE__);
        return ret;
    }
    so->output_thread_pid = tid;
    dt_info(TAG, "[%s:%d] create sub output thread success\n", __FUNCTION__,
            __LINE__);
    return 0;
}
