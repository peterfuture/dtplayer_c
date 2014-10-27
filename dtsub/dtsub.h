#ifndef DTSUB_H
#define DTSUB_H

#include "dtsub_para.h"

typedef struct{
    dtsub_para_t para;
    void *parent;
}dtsub_context_t;

int sub_init(dtsub_context_t *sctx);

#endif
