#ifndef DTSUB_API_H
#define DTSUB_API_H

#include "dtsub_para.h"

int dtsub_init (void **sub_priv, dtsub_para_t * para, void *parent);
int dtsub_start (void *sub_priv);
int dtsub_pause (void *sub_priv);
int dtsub_resume (void *sub_priv);
int dtsub_stop (void *sub_priv);

#endif
