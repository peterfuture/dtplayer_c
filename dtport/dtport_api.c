#include "dtport_api.h"
#include "dtport.h"

#define TAG "PORT-API"

//==Part1:Control
//
int dtport_stop(void *port)
{
    int ret = 0;
    dtport_context_t *pctx = (dtport_context_t *) port;
    port_stop(pctx);
    free(pctx);
    port = NULL;
    return ret;

}

int dtport_init(void **port, dtport_para_t * para, void *parent)
{
    int ret;
    dtport_context_t *pctx = malloc(sizeof(dtport_context_t));
    if (!pctx) {
        dt_error(TAG, "[%s:%d] dtport_context_t malloc failed \n", __FUNCTION__,
                 __LINE__);
        ret = -1;
        goto ERR0;
    }
    memset(pctx, 0, sizeof(dtport_context_t));
    ret = port_init(pctx, para);
    if (ret < 0) {
        dt_error(TAG, "[%s:%d] dtport_init failed \n", __FUNCTION__, __LINE__);
        ret = -1;
        goto ERR1;
    }
    *port = (void *) pctx;
    pctx->parent = parent;
    return ret;
ERR1:
    free(pctx);
ERR0:
    return ret;
}

//==Part2:DATA IO Relative
int dtport_write_frame(void *port, dt_av_pkt_t * frame, int type)
{
    dtport_context_t *pctx = (dtport_context_t *) port;
    return port_write_frame(pctx, frame, type);
}

int dtport_read_frame(void *port, dt_av_pkt_t **frame, int type)
{
    dtport_context_t *pctx = (dtport_context_t *) port;
    dt_debug(TAG, "[%s:%d]READ FRAME BEGIN \n", __FUNCTION__, __LINE__);
    int ret = port_read_frame(pctx, frame, type);
    dt_debug(TAG, "[%s:%d]READ FRAME END \n", __FUNCTION__, __LINE__);
    return ret;
}

//==Part3:State
int dtport_get_state(void *port, buf_state_t * buf_state, int type)
{
    dtport_context_t *pctx = (dtport_context_t *) port;
    return port_get_state(pctx, buf_state, type);
}
