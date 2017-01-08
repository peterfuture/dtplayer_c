#ifndef DTPLAYER_HOST_H
#define DTPLAYER_HOST_H

#include "dtplayer.h"
#include "dthost_api.h"

int player_host_init(dtplayer_context_t * dtp_ctx);
int player_host_start(dtplayer_context_t * dtp_ctx);
int player_host_pause(dtplayer_context_t * dtp_ctx);
int player_host_resume(dtplayer_context_t * dtp_ctx);
int player_host_stop(dtplayer_context_t * dtp_ctx);
int player_host_resize(dtplayer_context_t * dtp_ctx, int w, int h);
int player_host_get_info(dtplayer_context_t *dtp_ctx, enum HOST_CMD cmd,
                         unsigned long arg);
int player_host_set_info(dtplayer_context_t *dtp_ctx, enum HOST_CMD cmd,
                         unsigned long arg);

#endif
