#include "dtvideo.h"

#define TAG "VIDEO-API"

//==Part1:Control
int dtvideo_init (void **video_priv, dtvideo_para_t * para, void *parent)
{
    int ret = 0;
    dtvideo_context_t *vctx = (dtvideo_context_t *) malloc (sizeof (dtvideo_context_t));
    if (!vctx)
    {
        dt_error (TAG, "[%s:%d] video init failed \n", __FUNCTION__, __LINE__);
        ret = -1;
        goto ERR0;
    }
    memcpy (&vctx->video_para, para, sizeof (dtvideo_para_t));
    vctx->video_para.extradata_size = para->extradata_size;
    memcpy (&(vctx->video_para.extradata[0]), &(para->extradata[0]), para->extradata_size);

    //we need to set parent early
    vctx->parent = parent;
    ret = video_init (vctx);
    if (ret < 0)
    {
        dt_error (TAG, "[%s:%d] dtvideo_mgt_init failed \n", __FUNCTION__, __LINE__);
        ret = -1;
        goto ERR1;
    }
    *video_priv = (void *) vctx;
    return ret;
  ERR1:
    free (vctx);
  ERR0:
    return ret;
}

int dtvideo_start (void *video_priv)
{
    dtvideo_context_t *vctx = (dtvideo_context_t *) video_priv;
    return video_start (vctx);
}

int dtvideo_pause (void *video_priv)
{
    dtvideo_context_t *vctx = (dtvideo_context_t *) video_priv;
    return video_pause (vctx);
}

int dtvideo_resume (void *video_priv)
{
    dtvideo_context_t *vctx = (dtvideo_context_t *) video_priv;
    return video_resume (vctx);

}

int dtvideo_stop (void *video_priv)
{
    int ret;
    dtvideo_context_t *vctx = (dtvideo_context_t *) video_priv;
    if (!vctx)
    {
        dt_error (TAG, "[%s:%d] dt video context == NULL\n", __FUNCTION__, __LINE__);
        ret = -1;
        goto ERR0;
    }
    ret = video_stop (vctx);
    if (ret < 0)
    {
        dt_error (TAG, "[%s:%d] DTVIDEO STOP FAILED\n", __FUNCTION__, __LINE__);
        ret = -1;
        goto ERR0;
    }
    free (video_priv);
    video_priv = NULL;
    return ret;
  ERR0:
    return ret;

}

//==Part2:PTS&STATUS Relative
int64_t dtvideo_external_get_pts (void *video_priv)
{
    dtvideo_context_t *vctx = (dtvideo_context_t *) video_priv;
    return dtvideo_get_current_pts (vctx);
}

int64_t dtvideo_get_first_pts (void *video_priv)
{
    dtvideo_context_t *vctx = (dtvideo_context_t *) video_priv;
    return video_get_first_pts (vctx);
}

int dtvideo_drop (void *video_priv, int64_t target_pts)
{
    dtvideo_context_t *vctx = (dtvideo_context_t *) video_priv;
    return video_drop (vctx, target_pts);
}

int dtvideo_get_state (void *video_priv, dec_state_t * dec_state)
{
    int ret;
    dtvideo_context_t *vctx = (dtvideo_context_t *) video_priv;
    ret = video_get_dec_state (vctx, dec_state);
    return ret;
}

int dtvideo_get_out_closed (void *video_priv)
{
    int ret;
    dtvideo_context_t *vctx = (dtvideo_context_t *) video_priv;
    ret = video_get_out_closed (vctx);
    return ret;
}
