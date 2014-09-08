#include "dtplayer.h"
#include "dtdemuxer_api.h"
#include "dtplayer_io.h"
#include "dtplayer_util.h"
#include "dtplayer_update.h"
#include "dt_ini.h"
#include "dtstream.h"
#include "dtdemuxer.h"
#include "dtaudio.h"
#include "dtvideo.h"
#include "pthread.h"

#define TAG "PLAYER"

static void *event_handle_loop (dtplayer_context_t * dtp_ctx);

void player_register_all()
{
    static int register_ok = 0;
    if(!register_ok)
    {
        stream_register_all();
        demuxer_register_all();
        audio_register_all();
        video_register_all();
        register_ok = 1;
    }
}

static void set_player_status (dtplayer_context_t * dtp_ctx, player_status_t status)
{
    dtp_ctx->state.last_status = dtp_ctx->state.cur_status;
    dtp_ctx->state.cur_status = status;
}

static player_status_t get_player_status (dtplayer_context_t * dtp_ctx)
{
    return dtp_ctx->state.cur_status;
}

static int player_server_init (dtplayer_context_t * dtp_ctx)
{
    event_server_t *server = dt_alloc_server ();
    dt_info (TAG, "PLAYER SERVER INIT:%p \n", server);
    server->id = EVENT_SERVER_PLAYER;
    strcpy (server->name, "SERVER-PLAYER");
    dt_register_server (server);
    dtp_ctx->player_server = (void *) server;
    return 0;
}

static int player_server_release (dtplayer_context_t * dtp_ctx)
{
    event_server_t *server = (event_server_t *) dtp_ctx->player_server;
    dt_remove_server (server);
    dtp_ctx->player_server = NULL;
    return 0;
}

int player_init (dtplayer_context_t * dtp_ctx)
{
    int ret = 0;
    dtplayer_para_t *para = &dtp_ctx->player_para;
    player_ctrl_t *ctrl_info = &dtp_ctx->ctrl_info;
    
    if(get_player_status(dtp_ctx) >= PLAYER_STATUS_INIT_ENTER)
    {
        dt_error(TAG,"player already inited \n");
        goto ERR0;
    }


    set_player_status (dtp_ctx, PLAYER_STATUS_INIT_ENTER);
    dt_info (TAG, "[%s:%d] START PLAYER INIT\n", __FUNCTION__, __LINE__);
    dtp_ctx->file_name = dtp_ctx->player_para.file_name;
    dtp_ctx->update_cb = dtp_ctx->player_para.update_cb;

    dtdemuxer_para_t demux_para;
    demux_para.file_name = dtp_ctx->file_name;
    ret = dtdemuxer_open (&dtp_ctx->demuxer_priv, &demux_para, dtp_ctx);
    if (ret < 0)
    {
        ret = -1;
        goto ERR0;
    }
    dtp_ctx->media_info = dtdemuxer_get_media_info (dtp_ctx->demuxer_priv);

    /* setup player ctrl info 
     * 
     * have two ways to contrl, priority as follows
     * para
     * sys_set.ini
     *
     * */

    ctrl_info->has_audio = dtp_ctx->media_info->has_audio;
    ctrl_info->has_video = dtp_ctx->media_info->has_video;
    ctrl_info->has_sub = dtp_ctx->media_info->has_sub;

    //parse ini file
    char value[512];
    ctrl_info->sync_enable = (int)(ctrl_info->has_audio && ctrl_info->has_video); // default
    if (GetEnv ("PLAYER", "player.syncenable", value) > 0)
    {
        dt_info(TAG,"sync enable set by ini, value:%d\n ",ctrl_info->sync_enable);
        ctrl_info->sync_enable = atoi (value);
    }
    dt_info (TAG, "initial setting, has audio:%d has video:%d has sub:%d sync_enable:%d\n", ctrl_info->has_audio, ctrl_info->has_video, ctrl_info->has_sub, ctrl_info->sync_enable);


    if (GetEnv ("PLAYER", "player.noaudio", value) > 0)
    {
        if(atoi(value) == 1)
            ctrl_info->has_audio = 0;
    }
    if (GetEnv ("PLAYER", "player.novideo", value) > 0)
    {
        if(atoi(value) == 1)
            ctrl_info->has_video = 0;
    }
    if (GetEnv ("PLAYER", "player.nosub", value) > 0)
    {
        if(atoi(value) == 1)
            ctrl_info->has_sub = 0;
    }
    dt_info (TAG, "after ini setting, has audio:%d has video:%d has sub:%d \n", ctrl_info->has_audio, ctrl_info->has_video, ctrl_info->has_sub);


    //then use para update contrl info
    if (para->sync_enable != -1)
        ctrl_info->sync_enable = para->sync_enable;

    if(para->no_audio != -1)
        ctrl_info->has_audio = (!para->no_audio);
    if(para->no_video != -1)
        ctrl_info->has_video = (!para->no_video);
    if(para->no_sub != -1)
        ctrl_info->has_sub = (!para->no_sub);

    if(para->audio_index != -1)
        ctrl_info->cur_ast_index = dtp_ctx->media_info->cur_ast_index = para->audio_index;
    else
        ctrl_info->cur_ast_index = dtp_ctx->media_info->cur_ast_index;

    if(para->video_index != -1)
        ctrl_info->cur_vst_index = dtp_ctx->media_info->cur_vst_index = para->video_index;
    else
        ctrl_info->cur_vst_index = dtp_ctx->media_info->cur_vst_index; 

    if(para->sub_index != -1)
        ctrl_info->cur_sst_index = dtp_ctx->media_info->cur_sst_index = para->sub_index;
    else
        ctrl_info->cur_sst_index = dtp_ctx->media_info->cur_sst_index;

    dt_info (TAG, "ast_idx:%d vst_idx:%d sst_idx:%d \n", ctrl_info->cur_ast_index, ctrl_info->cur_vst_index, ctrl_info->cur_sst_index);

    //Fixme cur index valid check ...

    //other info setup
    ctrl_info->start_time = dtp_ctx->media_info->start_time;
    ctrl_info->first_time = -1;
    ctrl_info->width = para->width;
    ctrl_info->height = para->height;

    //invalid check
    if (!ctrl_info->has_audio && !ctrl_info->has_video)
    {
        dt_info (TAG, "HAVE NO A-V STREAM \n");
        goto ERR0;
    }

    //updte mediainfo --
    //
    ctrl_info->has_sub = 0;     // do not support sub for now 
    dtp_ctx->media_info->disable_audio = !ctrl_info->has_audio;
    dtp_ctx->media_info->disable_video = !ctrl_info->has_video;
    dtp_ctx->media_info->disable_sub = !ctrl_info->has_sub;
    dt_info (TAG, "Finally, ctrl info, audio:%d video:%d sub:%d \n", ctrl_info->has_audio, ctrl_info->has_video, ctrl_info->has_sub);


    dt_info (TAG, "[%s:%d] END PLAYER INIT, RET = %d\n", __FUNCTION__, __LINE__, ret);
    set_player_status (dtp_ctx, PLAYER_STATUS_INIT_EXIT);
    player_handle_cb (dtp_ctx);
    return 0;
ERR0:
    set_player_status (dtp_ctx, PLAYER_STATUS_ERROR);
    player_handle_cb (dtp_ctx);
    return -1;
}

int player_set_video_size (dtplayer_context_t * dtp_ctx, int width, int height)
{
    player_ctrl_t *pctrl = &dtp_ctx->ctrl_info;
    dt_info(TAG,"[%s:%d] width:%d height:%d\n",__FUNCTION__,__LINE__,width,height);
    if(pctrl->width == -1)
    {
        pctrl->width = width;
        pctrl->height = height;
    }
    else
        dt_error(TAG,"dst width:%d height:%d have been set \n",pctrl->width,pctrl->height);
    return 0;
}

int player_start (dtplayer_context_t * dtp_ctx)
{
    int ret = 0;
    pthread_t tid;


    if(get_player_status(dtp_ctx) >= PLAYER_STATUS_START)
    {
        dt_error(TAG,"player already started \n");
        return 0;
    }

    set_player_status (dtp_ctx, PLAYER_STATUS_START);

    /* init & start player server first */
    player_server_init (dtp_ctx);
    ret = pthread_create (&tid, NULL, (void *) &event_handle_loop, (void *) dtp_ctx);
    if (ret == -1)
    {
        dt_error (TAG "file:%s [%s:%d] player io thread start failed \n", __FILE__, __FUNCTION__, __LINE__);
        player_server_release (dtp_ctx);
        goto ERR3;
    }
    dt_info (TAG, "[%s:%d] create event handle loop thread id = %lu\n", __FUNCTION__, __LINE__, tid);
    dtp_ctx->event_loop_id = tid;

    ret = player_host_init (dtp_ctx);
    if (ret < 0)
    {
        dt_error (TAG, "[%s:%d] player_host_init failed!\n", __FUNCTION__, __LINE__);
        goto ERR3;
    }
    ret = start_io_thread (dtp_ctx);
    if (ret == -1)
    {
        dt_error (TAG "file:%s [%s:%d] player io thread start failed \n", __FILE__, __FUNCTION__, __LINE__);
        goto ERR2;
    }

    ret = player_host_start (dtp_ctx);
    if (ret != 0)
    {
        dt_error (TAG "file:%s [%s:%d] player host start failed \n", __FILE__, __FUNCTION__, __LINE__);
        set_player_status (dtp_ctx, PLAYER_STATUS_ERROR);
        goto ERR1;
    }

    dt_info (TAG, "PLAYER START OK\n");
    set_player_status (dtp_ctx, PLAYER_STATUS_RUNNING);
    player_handle_cb (dtp_ctx);
    return 0;
  ERR1:
    stop_io_thread (dtp_ctx);
  ERR2:
    player_host_stop (dtp_ctx);
  ERR3:
    set_player_status (dtp_ctx, PLAYER_STATUS_ERROR);
    player_handle_cb (dtp_ctx);
    return ret;
}

int player_pause (dtplayer_context_t * dtp_ctx)
{
    if (get_player_status (dtp_ctx) == PLAYER_STATUS_PAUSED)
        return player_resume (dtp_ctx);

    if (player_host_pause (dtp_ctx) == -1)
    {
        dt_error (TAG, "PAUSE NOT HANDLED\n");
        return -1;
    }
    set_player_status (dtp_ctx, PLAYER_STATUS_PAUSED);
    player_handle_cb (dtp_ctx);
    return 0;
}

int player_resume (dtplayer_context_t * dtp_ctx)
{
    if (get_player_status (dtp_ctx) != PLAYER_STATUS_PAUSED)
        return -1;
    if (player_host_resume (dtp_ctx) == -1)
    {
        dt_error (TAG, "RESUME NOT HANDLED\n");
        return -1;
    }
    set_player_status (dtp_ctx, PLAYER_STATUS_RESUME);
    player_handle_cb (dtp_ctx);
    set_player_status (dtp_ctx, PLAYER_STATUS_RUNNING);
    return 0;
}

int player_seekto (dtplayer_context_t * dtp_ctx, int seek_time)
{
    player_ctrl_t *ctrl_info = &dtp_ctx->ctrl_info;
    
    if (get_player_status (dtp_ctx) < PLAYER_STATUS_INIT_EXIT)
        return -1;
    
    player_state_t *play_stat = &dtp_ctx->state;
    play_stat->cur_time = seek_time;
    play_stat->cur_time_ms = seek_time*1000;
 
    set_player_status (dtp_ctx, PLAYER_STATUS_SEEK_ENTER);
    if(io_thread_running(dtp_ctx))
        pause_io_thread (dtp_ctx);
    player_host_pause (dtp_ctx);
    

    int64_t start_time = (ctrl_info->first_time != -1)?ctrl_info->first_time : ctrl_info->start_time;
    int64_t target_time = seek_time * 1000000 + start_time; 
    int ret = dtdemuxer_seekto (dtp_ctx->demuxer_priv, target_time);
    if (ret == -1)
        goto FAIL;
    player_host_stop (dtp_ctx);
    player_host_init (dtp_ctx);
    if(io_thread_running(dtp_ctx)) // maybe io thread already quit
        resume_io_thread (dtp_ctx);
    else
        start_io_thread(dtp_ctx);
    
    player_update_state (dtp_ctx);
    set_player_status (dtp_ctx, PLAYER_STATUS_SEEK_EXIT);
    player_handle_cb (dtp_ctx);

    //check if another seek event comming soom
    event_server_t *server = (event_server_t *) dtp_ctx->player_server;
    event_t *event = dt_peek_event (server);
    if(event != NULL && event->type == PLAYER_EVENT_SEEK)
    {
        dt_info(TAG,"execute queue seekto event \n");
        return 0;
    }
    player_host_start (dtp_ctx);
    set_player_status (dtp_ctx, PLAYER_STATUS_RUNNING);
    player_update_state (dtp_ctx);
    player_handle_cb (dtp_ctx);

    return 0;
  FAIL:
    //seek fail, continue running
    dt_error(TAG,"[%s:%d] seek failed, continue playing \n",__FUNCTION__,__LINE__);
    resume_io_thread (dtp_ctx);
    if(io_thread_running(dtp_ctx))
        player_host_resume (dtp_ctx);
    set_player_status (dtp_ctx, PLAYER_STATUS_SEEK_EXIT);
    player_handle_cb (dtp_ctx);
    set_player_status (dtp_ctx, PLAYER_STATUS_RUNNING);
    player_handle_cb (dtp_ctx);
    return 0;
}

int player_get_mediainfo (dtplayer_context_t * dtp_ctx, dt_media_info_t *info)
{
	if(!dtp_ctx->media_info)
		return -1;
	memcpy(info,dtp_ctx->media_info,sizeof(dt_media_info_t));
	return 0;
}

int player_stop (dtplayer_context_t * dtp_ctx)
{
    dt_info (TAG, "PLAYER STOP STATUS SET\n");
    set_player_status (dtp_ctx, PLAYER_STATUS_STOP);
    return 0;
}

static char * get_event_name(int cmd)
{
    switch(cmd)
    {
    case PLAYER_EVENT_START:
        return "player_event_start";
    case PLAYER_EVENT_PAUSE:
        return "player_event_pause";
    case PLAYER_EVENT_RESUME:
        return "player_event_resume";
    case PLAYER_EVENT_STOP:
        return "player_event_stop";
    case PLAYER_EVENT_SEEK:
        return "player_event_seek";
    default:
        return "unkown cmd";
    }
    return "null";
}

static int player_handle_event (dtplayer_context_t * dtp_ctx)
{
    event_server_t *server = (event_server_t *) dtp_ctx->player_server;
    event_t *event = dt_get_event (server);
START:
    if (!event)
    {
        dt_debug (TAG, "GET EVENT NULL \n");
        return 0;
    }
    dt_info (TAG, "GET EVENT:%d %s\n", event->type,get_event_name(event->type));
    switch (event->type)
    {
    case PLAYER_EVENT_START:
        player_start (dtp_ctx);
        break;
    case PLAYER_EVENT_PAUSE:
        player_pause (dtp_ctx);
        break;
    case PLAYER_EVENT_RESUME:
        player_resume (dtp_ctx);
        break;
    case PLAYER_EVENT_STOP:
        player_stop (dtp_ctx);
        break;
    case PLAYER_EVENT_SEEK:
        player_seekto (dtp_ctx, event->para.np);
        break;
    default:
        break;
    }
    if (event)
    {
        free (event);
        event = NULL;
    }

    //check if still have event
    event = dt_peek_event (server);
    if(event)
    {
        event = dt_get_event (server);
        goto START;
    }
    return 0;
}

static void *event_handle_loop (dtplayer_context_t * dtp_ctx)
{
    while (1)
    {
        player_handle_event (dtp_ctx);
        if (get_player_status (dtp_ctx) == PLAYER_STATUS_STOP)
            goto QUIT;
        usleep (300 * 1000);    // 1/3s update
        if (get_player_status (dtp_ctx) != PLAYER_STATUS_RUNNING)
            continue;
        if (get_player_status (dtp_ctx) == PLAYER_STATUS_RUNNING)
            player_update_state (dtp_ctx);
        player_handle_cb (dtp_ctx);
        if (!dtp_ctx->ctrl_info.eof_flag)
            continue;
        if (dthost_get_out_closed (dtp_ctx->host_priv) == 1)
            goto QUIT;
    }
    /* when playend itself ,we need to release manually */
  QUIT:
    stop_io_thread (dtp_ctx);
    player_host_stop (dtp_ctx);
    dtdemuxer_close (dtp_ctx->demuxer_priv);
    player_server_release (dtp_ctx);
    dt_event_server_release ();
    dt_info (TAG, "EXIT PLAYER EVENT HANDLE LOOP\n");
    set_player_status (dtp_ctx, PLAYER_STATUS_EXIT);
    player_handle_cb (dtp_ctx);

    free(dtp_ctx);
    pthread_exit (NULL);
    return NULL;
}
