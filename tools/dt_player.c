#include "dt_player.h"
#include "version.h"
#include "render.h"
#include "stream_wrapper.h"
#include "demuxer_wrapper.h"
#include "ao_wrapper.h"
#include "ad_wrapper.h"
#include "vo_wrapper.h"
#include "vd_wrapper.h"
#include "ui.h"

#include "dtplayer_api.h"
#include "dtplayer.h"

#include <stdio.h>
#include <string.h>

#define TAG "DT-PLAYER"

static ply_ctx_t ply_ctx;

#ifdef ENABLE_DTAP
int dtap_change_effect(ao_wrapper_t *wrapper);
#endif

#ifdef ENABLE_AO_SDL2 
void ao_sdl2_setup(ao_wrapper_t *wrapper);
#endif
#ifdef ENABLE_AO_SDL
void ao_sdl_setup(ao_wrapper_t *wrapper);
void vo_sdl_setup(vo_wrapper_t *wrapper);
#endif

static int update_cb (void *cookie, player_state_t * state)
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
    para->disable_audio = para->disable_video = para->disable_sub = -1;
    para->height = para->width = -1;
    para->loop_mode = 0;
    para->audio_index = para->video_index = para->sub_index = -1;
    para->update_cb = NULL;
    para->disable_avsync = 0;

    //init para with argv
    para->file_name = argv[1];
    para->update_cb = (void *) update_cb;
    //para->no_audio=1;
    //para->no_video=1;
    para->width = -1;
    para->height = -1;
    para->cookie = NULL;
    return 0;
}

extern vd_wrapper_t vd_ex_ops;
static void register_ex_all()
{
    //player_register_ex_stream();
    //player_register_ex_demuxer();
    //player_register_ex_ao();
    //player_register_ex_ad();
    //player_register_ex_vo();
    //dtplayer_register_ext_vd(&vd_ex_ops);
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

#ifdef ENABLE_AO_SDL2 
    ao_sdl2_setup(&ply_ctx.ao);
    dtplayer_register_ext_ao(&ply_ctx.ao);
#endif
#ifdef ENABLE_AO_SDL 
    ao_sdl_setup(&ply_ctx.ao);
    dtplayer_register_ext_ao(&ply_ctx.ao);
    vo_sdl_setup(&ply_ctx.vo);
    dtplayer_register_ext_vo(&ply_ctx.vo);
#endif

    player_register_all();
    register_ex_all();

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

    //set display win size
    int width = 720;
    int height = 480;
    if(info.has_video)
    {
        vstream_info_t *vstream = info.vstreams[0];
        width = vstream->width;
        height = vstream->height;
    }
    if(width <= 0 || width > 1280)
        width = 720;
    if(height <= 0 || height >720)
        height = 480;

    ply_ctx.disp_width = width;
    ply_ctx.disp_height = height;

    ui_init(width,height); 

	dtplayer_start (player_priv);

    //event handle
    player_event_t event = EVENT_NONE;
    args_t arg;
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
                dtplayer_seekto (player_priv, arg.arg1);
                break;
            case EVENT_STOP:
                dtplayer_stop (player_priv);
                goto QUIT_CHECK;
                break;
            case EVENT_RESIZE:
                dt_info(TAG,"resize, w:%d h:%d \n",arg.arg1, arg.arg2);
                ui_window_resize(arg.arg1, arg.arg2);
                ply_ctx.disp_width = arg.arg1;
                ply_ctx.disp_height = arg.arg2;
                //dtplayer_set_video_size(player_priv, arg.arg1, arg.arg2);
                break;
            case EVENT_VOLUME_ADD:
                dt_info(TAG, " volume add \n");
                ply_ctx.volume++;
                ply_ctx.volume = ply_ctx.volume % 10;
                ply_ctx.ao.ao_set_volume(&ply_ctx.ao, ply_ctx.volume);
                break;
#ifdef ENABLE_DTAP
            case EVENT_AE:
                dtap_change_effect(&ply_ctx.ao);
                break;
#endif
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
    ui_stop(); 
    dt_info ("", "QUIT DTPLAYER-TEST\n");
    return 0;
}
