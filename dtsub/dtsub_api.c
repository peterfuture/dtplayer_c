/***********************************************************************
**
**  Module : dtsub_api.c
**  Summary: dtsub API
**  Section: dtsub
**  Author : peter
**  Notes  : 
**           provide dtsub api
**
***********************************************************************/

#define TAG "SUB-API"

#include "dtsub_para.h"
#include "dtsub.h"
#include "dt_av.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

/***********************************************************************
**
** dtsub_init
**
***********************************************************************/

int dtsub_init(void **sub_priv, dtsub_para_t *para, void *parent)
{
    int ret = 0;
    dtsub_context_t *sctx = (dtsub_context_t *)malloc(sizeof(dtsub_context_t));
    if (!sctx)
    {
        dt_error(TAG, "[%s:%d]dtsub module init failed \n", __FUNCTION__, __LINE__);
        ret = -1;
        goto ERR0;
    }
    memcpy (&sctx->para, para, sizeof(dtsub_para_t));

    //we need to set parent early
    sctx->parent = parent;
    ret = sub_init(sctx);
    if (ret < 0)
    {
        dt_error (TAG, "[%s:%d] sub init failed \n", __FUNCTION__, __LINE__);
        ret = -1;
        goto ERR1;
    }
    *sub_priv = (void *)sctx;
    return ret;
ERR1:
    free(sctx);
ERR0:
    return ret;
}
