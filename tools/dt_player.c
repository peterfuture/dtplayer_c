#include "dt_player.h"
#include "version.h"
#include "render.h"
#include "ui.h"

#include "dtplayer_api.h"
#include "dtplayer.h"

#include <stdio.h>
#include <string.h>

#define TAG "DT-PLAYER"

static ply_ctx_t ply_ctx;

static int update_cb (player_state_t * state)
{
    if (state->cur_status == PLAYER_STATUS_EXIT)
    {
        dt_info (TAG, "RECEIVE EXIT CMD\n");
        ply_ctx.exit_flag = 1;
    }
    ply_ctx.cur_time = state->cur_time;
    ply_ctx.cur_time_ms = state->cur_time_ms;
    ply_ctx.duration = state->full_time;

    dt_debug (TAG, "UPDATECB CURSTATUS:%x \n", state->cur_status);
    dt_info(TAG,"CUR TIME %lld S  FULL TIME:%lld  \n",state->cur_time,state->full_time);
    return 0;
}

static int parse_cmd(int argc,char **argv,dtplayer_para_t *para)
{
    //first reset para
    para->no_audio = para->no_video = para->no_sub = -1;
    para->height = para->width = -1;
    para->loop_mode = 0;
    para->audio_index = para->video_index = para->sub_index = -1;
    para->update_cb = NULL;
    para->sync_enable = -1;

    //init para with argv
    para->file_name = argv[1];
    para->update_cb = (void *) update_cb;
    //para->no_audio=1;
    //para->no_video=1;
    para->width = ply_ctx.disp_width;
    para->height = ply_ctx.disp_height;

}

int main (int argc, char **argv)
{
    int ret = 0;
    version_info ();
    if (argc < 2)
    {
        dt_info ("", " no enough args\n");
        show_usage ();
        return 0;
    }
    memset(&ply_ctx,0,sizeof(ply_ctx_t));
    ply_ctx.disp_width = 720;
    ply_ctx.disp_height = 480;

    player_register_all();

    ui_init();    
    render_init();

    dtplayer_para_t para;
    parse_cmd(argc,argv,&para);
    
    void *player_priv = dtplayer_init (&para);
    if (!player_priv)
        return -1;

    //get media info
	dt_media_info_t info;
	ret = dtplayer_get_mediainfo(player_priv, &info);
	if(ret < 0)
	{
		dt_info(TAG,"Get mediainfo failed, quit \n");
		return -1;
	}
	dt_info(TAG,"Get Media Info Ok,filesize:%lld fulltime:%lld S \n",info.file_size,info.duration);


	dtplayer_start (player_priv);

    //event handle
    player_event_t event = EVENT_NONE;
    int arg = -1;
    while(1)
    {
        if(ply_ctx.exit_flag == 1)
            break;
        event = get_event(&arg,&ply_ctx);
        if(event == EVENT_NONE)
        {
            usleep(100);
            continue;
        }
        
        switch(event)
        {
            case EVENT_PAUSE:
                dtplayer_pause (player_priv);
                break;
            case EVENT_RESUME:
                dtplayer_resume (player_priv);
                break;
            case EVENT_SEEK:
                dtplayer_seekto (player_priv, arg);
                break;
            case EVENT_STOP:
                dtplayer_stop (player_priv);
                goto QUIT_CHECK;
                break;
            default:
                dt_info(TAG,"UNKOWN CMD, IGNORE \n");
                break;
        }

    }
QUIT_CHECK:
    while(!ply_ctx.exit_flag)
    {
        usleep(100);
        break;
    }
    render_stop();
    ui_stop(); 
    dt_info ("", "QUIT DTPLAYER-TEST\n");
    return 0;
}
