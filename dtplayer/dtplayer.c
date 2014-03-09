#include "dtplayer.h"
#include "dtdemuxer_api.h"
#include "dtplayer_io.h"
#include "dtplayer_util.h"
#include "dtplayer_update.h"
#include "dt_ini.h"

#define TAG "PLAYER"

static void *event_handle_loop(dtplayer_context_t * dtp_ctx);

static void set_player_status(dtplayer_context_t * dtp_ctx, player_status_t status)
{
    dtp_ctx->state.last_status = dtp_ctx->state.cur_status;
    dtp_ctx->state.cur_status = status;
}

static player_status_t get_player_status(dtplayer_context_t * dtp_ctx)
{
	return dtp_ctx->state.cur_status;
}

static int player_server_init(dtplayer_context_t *dtp_ctx)
{
    event_server_t *server = dt_alloc_server();
    dt_info(TAG,"PLAYER SERVER INIT:%p \n",server);
    server->id = EVENT_SERVER_PLAYER;
    strcpy(server->name,"SERVER-PLAYER");
    dt_register_server(server);
    dtp_ctx->player_server = (void *)server;
    return 0;
}

static int player_server_release(dtplayer_context_t *dtp_ctx)
{
    event_server_t *server = (event_server_t *)dtp_ctx->player_server;
    dt_remove_server(server);
    dtp_ctx->player_server = NULL;
    return 0;
}

int player_init(dtplayer_context_t *dtp_ctx)
{
    int ret = 0;
	pthread_t tid;
	set_player_status(dtp_ctx, PLAYER_STATUS_INIT_ENTER);
    dt_info(TAG,"[%s:%d] START PLAYER INIT\n",__FUNCTION__,__LINE__);
    dtp_ctx->file_name = dtp_ctx->player_para.file_name;
    dtp_ctx->update_cb = dtp_ctx->player_para.update_cb; 

    /* init server */
    player_server_init(dtp_ctx);
    
    dtdemuxer_para_t demux_para;
    demux_para.file_name = dtp_ctx->file_name;
    ret = dtdemuxer_open(&dtp_ctx->demuxer_priv,&demux_para,dtp_ctx);
    if(ret < 0)
    {
        ret = -1;
        goto ERR1;
    }
    dtp_ctx->media_info = dtdemuxer_get_media_info(dtp_ctx->demuxer_priv);
   
    /* setup player ctrl info */
    char value[512];
    dtplayer_para_t *para = &dtp_ctx->player_para;
    player_ctrl_t *ctrl_info = &dtp_ctx->ctrl_info;
    ctrl_info->start_time = dtp_ctx->media_info->start_time;
    ctrl_info->first_time = -1;
    ctrl_info->has_audio = (para->no_audio == -1)?dtp_ctx->media_info->has_audio:(!para->no_audio);
    ctrl_info->has_video = (para->no_video == -1)?dtp_ctx->media_info->has_video:(!para->no_video);
    ctrl_info->has_sub = (para->no_sub == -1)?dtp_ctx->media_info->has_sub:(!para->no_sub);
    
    int sync_enable_ini = -1;
    if(GetEnv("PLAYER","player.syncenable",value)>0)
        sync_enable_ini = atoi(value);
    if(para->sync_enable != -1)
        ctrl_info->sync_enable = para->sync_enable;
    else
        ctrl_info->sync_enable = (sync_enable_ini == -1)?(ctrl_info->has_audio && ctrl_info->has_video):sync_enable_ini;
    
    ctrl_info->cur_ast_index = (para->audio_index == -1)?dtp_ctx->media_info->cur_ast_index:para->audio_index;
    ctrl_info->cur_vst_index = (para->video_index == -1)?dtp_ctx->media_info->cur_vst_index:para->video_index;
    ctrl_info->cur_sst_index = (para->sub_index == -1)?dtp_ctx->media_info->cur_sst_index:para->sub_index;
    dt_info(TAG,"ast_idx:%d vst_idx:%d sst_idx:%d \n",ctrl_info->cur_ast_index,ctrl_info->cur_vst_index,ctrl_info->cur_sst_index);

    int no_audio_ini = -1;
    int no_video_ini = -1;
    int no_sub_ini = -1;
    if(GetEnv("PLAYER","player.noaudio",value)>0)
        no_audio_ini = atoi(value);
    if(GetEnv("PLAYER","player.novideo",value)>0)
        no_video_ini = atoi(value);
    if(GetEnv("PLAYER","player.nosub",value)>0)
        no_sub_ini = atoi(value);
    dt_info(TAG,"ini noaudio:%d novideo:%d nosub:%d \n",no_audio_ini,no_video_ini,no_sub_ini);

    if(para->no_audio != -1)
        ctrl_info->has_audio = !para->no_audio;
    else
        ctrl_info->has_audio = (no_audio_ini == -1)?dtp_ctx->media_info->has_audio:!no_audio_ini;
    if(para->no_video != -1)
        ctrl_info->has_video = !para->no_video;
    else
        ctrl_info->has_video = (no_video_ini == -1)?dtp_ctx->media_info->has_video:!no_video_ini;
     if(para->no_sub != -1)
        ctrl_info->has_sub = !para->no_sub;
    else
        ctrl_info->has_sub = (no_sub_ini)?dtp_ctx->media_info->has_sub:!no_sub_ini;
   
    if(!ctrl_info->has_audio)
        ctrl_info->cur_ast_index = -1;
    if(!ctrl_info->has_video)
        ctrl_info->cur_vst_index = -1;
    if(!ctrl_info->has_sub)
        ctrl_info->cur_sst_index = -1;
  
    dt_info(TAG,"Finally, audio:%d video:%d sub:%d \n",ctrl_info->has_audio,ctrl_info->has_video,ctrl_info->has_sub);

    dtp_ctx->media_info->no_audio = !ctrl_info->has_audio;
    dtp_ctx->media_info->no_video = !ctrl_info->has_video;
    dtp_ctx->media_info->no_sub = !ctrl_info->has_sub;
    
    /*create event loop*/
    ret = pthread_create(&tid, NULL, (void *)&event_handle_loop, (void *)dtp_ctx);
    if (ret == -1) {
		dt_error(TAG"file:%s [%s:%d] player io thread start failed \n", __FILE__,__FUNCTION__, __LINE__);
		goto ERR2;
	}
    dtp_ctx->event_loop_id = tid;
    dt_info(TAG,"[%s:%d] END PLAYER INIT, RET = %d\n",__FUNCTION__,__LINE__,ret);
	set_player_status(dtp_ctx, PLAYER_STATUS_INIT_EXIT);
    player_handle_cb(dtp_ctx);
    return 0;
ERR2:
    dtdemuxer_close(dtp_ctx->demuxer_priv); 
ERR1:
	set_player_status(dtp_ctx, PLAYER_STATUS_ERROR);
    player_handle_cb(dtp_ctx);
	return -1;
}

int player_start(dtplayer_context_t * dtp_ctx)
{
	int ret = 0;
	
	set_player_status(dtp_ctx, PLAYER_STATUS_START);
    ret = player_host_init(dtp_ctx);
	if (ret < 0) {
		dt_error(TAG,"[%s:%d] player_host_init failed!\n", __FUNCTION__,__LINE__);
		goto ERR3;
	}
    ret = start_io_thread(dtp_ctx);
	if (ret == -1) {
		dt_error(TAG"file:%s [%s:%d] player io thread start failed \n", __FILE__,__FUNCTION__, __LINE__);
		goto ERR2;
	}
    
    ret = player_host_start(dtp_ctx);
	if(ret !=0)
    {
        dt_error(TAG"file:%s [%s:%d] player host start failed \n", __FILE__,__FUNCTION__, __LINE__);
		set_player_status(dtp_ctx, PLAYER_STATUS_ERROR);
		goto ERR1;
    }
    dt_info(TAG,"PLAYER START OK\n");
    set_player_status(dtp_ctx, PLAYER_STATUS_RUNNING);
    player_handle_cb(dtp_ctx);
	return 0;
ERR1:
    stop_io_thread(dtp_ctx); 
ERR2:
    player_host_stop(dtp_ctx);
ERR3:
	set_player_status(dtp_ctx, PLAYER_STATUS_ERROR);
    player_handle_cb(dtp_ctx);
    return ret;
}

int player_pause(dtplayer_context_t *dtp_ctx)
{
    if(get_player_status(dtp_ctx) == PLAYER_STATUS_PAUSED)
        return player_resume(dtp_ctx);

    if(player_host_pause(dtp_ctx)==-1)
    {
        dt_error(TAG,"PAUSE NOT HANDLED\n");
        return -1;
    }
    set_player_status(dtp_ctx, PLAYER_STATUS_PAUSED);
    player_handle_cb(dtp_ctx);
    return 0;
}

int player_resume(dtplayer_context_t *dtp_ctx)
{
    if(get_player_status(dtp_ctx) != PLAYER_STATUS_PAUSED)
        return -1;
    if(player_host_resume(dtp_ctx)==-1)
    {
        dt_error(TAG,"RESUME NOT HANDLED\n");
        return -1;
    }
    set_player_status(dtp_ctx, PLAYER_STATUS_RESUME);
    player_handle_cb(dtp_ctx);
    set_player_status(dtp_ctx, PLAYER_STATUS_RUNNING);
    return 0;
}

int player_seekto(dtplayer_context_t *dtp_ctx,int seek_time)
{
    if(get_player_status(dtp_ctx) < PLAYER_STATUS_INIT_EXIT)
        return -1;
    set_player_status(dtp_ctx, PLAYER_STATUS_SEEK_ENTER);
    pause_io_thread(dtp_ctx);
    player_host_pause(dtp_ctx);
    int ret = dtdemuxer_seekto(dtp_ctx->demuxer_priv,seek_time);
    if(ret == -1)
        goto FAIL;
    player_host_stop(dtp_ctx);
    player_host_init(dtp_ctx);
    resume_io_thread(dtp_ctx);
    player_host_start(dtp_ctx);
    set_player_status(dtp_ctx, PLAYER_STATUS_SEEK_EXIT);
    player_handle_cb(dtp_ctx);
    set_player_status(dtp_ctx, PLAYER_STATUS_RUNNING);
    return 0;
FAIL:
    //seek fail, continue running
    resume_io_thread(dtp_ctx);
    player_host_resume(dtp_ctx);
    set_player_status(dtp_ctx, PLAYER_STATUS_RUNNING);
    return 0;
}


int player_stop(dtplayer_context_t *dtp_ctx)
{
    set_player_status(dtp_ctx,PLAYER_STATUS_STOP);
    dt_info(TAG,"PLAYER STOP STATUS SET\n");
    return 0;
}

static int player_handle_event(dtplayer_context_t *dtp_ctx)
{
    event_server_t *server = (event_server_t *)dtp_ctx->player_server;
    event_t *event = dt_get_event(server);
    
    if(!event)
    {
        dt_debug(TAG,"GET EVENT NULL \n");
        return 0;
    }
    dt_info(TAG,"GET EVENT:%d \n",event->type);    
    switch(event->type)
    {
        case PLAYER_EVENT_START:
            player_start(dtp_ctx);
            break;
        case PLAYER_EVENT_PAUSE:
            player_pause(dtp_ctx);
            break;
        case PLAYER_EVENT_RESUME:
            player_resume(dtp_ctx);
            break;
        case PLAYER_EVENT_STOP:
            player_stop(dtp_ctx);
            break;
        case PLAYER_EVENT_SEEK:
            player_seekto(dtp_ctx,event->para.np);
            break;
        default:
            break;
    }
    if(event)
    {
        free(event);
        event = NULL;
    }
    return 0;
}

static void *event_handle_loop(dtplayer_context_t * dtp_ctx)
{
    while(1)
    {
        player_handle_event(dtp_ctx);
        if(get_player_status(dtp_ctx) == PLAYER_STATUS_STOP)
            goto QUIT;
        usleep(300*1000);// 1/3s update
        if(get_player_status(dtp_ctx) != PLAYER_STATUS_RUNNING)
            continue;
        if(get_player_status(dtp_ctx) == PLAYER_STATUS_RUNNING)
            player_update_state(dtp_ctx);
        player_handle_cb(dtp_ctx);
        if(!dtp_ctx->ctrl_info.eof_flag)
            continue;
        if(dthost_get_out_closed(dtp_ctx->host_priv) == 1)
            goto QUIT;
    }
    /* when playend itself ,we need to release manually*/
QUIT:
    stop_io_thread(dtp_ctx);
    player_host_stop(dtp_ctx);
    dtdemuxer_close(dtp_ctx->demuxer_priv);
    player_server_release(dtp_ctx);
    dt_event_server_release();
    dt_info(TAG,"EXIT PLAYER EVENT HANDLE LOOP\n"); 
    set_player_status(dtp_ctx,PLAYER_STATUS_EXIT);
    player_handle_cb(dtp_ctx);

    free(dtp_ctx);
    pthread_exit(NULL);
    return NULL;
}


