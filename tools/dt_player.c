#include "version.h"
#include "render.h"
#include "ui.h"

#include "dtplayer_api.h"
#include "dtplayer.h"

#include <stdio.h>
#include <string.h>

#define TAG "DTM-PLAYER"

static int exit_flag = 0;

int update_cb (player_state_t * state)
{
    if (state->cur_status == PLAYER_STATUS_EXIT)
    {
        dt_info (TAG, "RECEIVE EXIT CMD\n");
        exit_flag = 1;
    }
    dt_debug (TAG, "UPDATECB CURSTATUS:%x \n", state->cur_status);
    return 0;
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

    player_register_all();

    ui_init();    
    render_init();

    void *player_priv;
    dtplayer_para_t *para = dtplayer_alloc_para ();
    if (!para)
        return -1;
    strcpy (para->file_name, argv[1]);
    para->update_cb = (void *) update_cb;
    //para->no_audio=1;
    //para->no_video=1;
    para->width = 720;
    para->height = 480;
    ret = dtplayer_init (&player_priv, para);
    if (ret < 0)
        return -1;

    dtplayer_release_para (para);
    dtplayer_start (player_priv);

    //event handle
    player_event_t event = EVENT_NONE;
    int arg = -1;
    while(1)
    {
        if(exit_flag == 1)
            break;
        event = get_event(&arg);
        if(event == EVENT_NONE)
        {
            usleep(100000);
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
                dtplayer_seek (player_priv, arg);
                break;
            case EVENT_STOP:
                dtplayer_stop (player_priv);
                break;
            default:
                dt_info(TAG,"UNKOWN CMD, IGNORE \n");
                break;
        }

    }

    render_stop();
    ui_stop(); 
    dt_info ("", "QUIT DTPLAYER-TEST\n");
    return 0;
}
