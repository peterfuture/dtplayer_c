#include "dtplayer/dtplayer_api.h"
#include "dtplayer/dtplayer.h"

#include "version.h"

#include <stdio.h>
#include <string.h>

#define TAG "DTM-PLAYER"

static int exit_flag = 0;
static void event_loop(void *arg)
{
	char buf[128];
	int len;
    int exit_flag = 0;
	while (1) {
        if(exit_flag == 1)
            break;
		len = read(0, buf, 16);
		if (2 == len) {
			dt_info(TAG,"[%s:%d]get CMD :%s \n", __FUNCTION__, __LINE__,buf);
			switch (buf[0]) {
			case 'q':
			case 27:	/* esc */
				dtplayer_stop(arg);
                exit_flag = 1;
                break;
     			case 32:	/* space */
				dtplayer_pause(arg);
				break;
					}
		} else if (len > 2) {
			/* <- , -> , pgup , pgdown */
			if (0x1b == buf[0] && 0x5b == buf[1] && 0x44 == buf[2]) {
				dt_info(TAG"[%s:%d]enter < key \n", __FUNCTION__,__LINE__);
				dtplayer_seek(arg, -10);
			}
			if (0x1b == buf[0] && 0x5b == buf[1] && 0x43 == buf[2]) {
				dt_info(TAG"[%s:%d]enter > key \n", __FUNCTION__,__LINE__);
				dtplayer_seek(arg, 10);
			}
			if (0x1b == buf[0] && 0x5b == buf[1] && 0x35 == buf[2]) {
				dt_info(TAG,"[%s:%d]enter pgup key \n", __FUNCTION__,__LINE__);
				dtplayer_seek(arg, -60);
			}
			if (0x1b == buf[0] && 0x5b == buf[1] && 0x36 == buf[2]) {
				dt_info(TAG,"[%s:%d]enter pgdown key \n",__FUNCTION__, __LINE__);
				dtplayer_seek(arg, 10);
			}
		}
        usleep(10000); 
	}

}

int update_cb(player_state_t *state)
{
    if(state->cur_status == PLAYER_STATUS_EXIT)
    {
        dt_info(TAG,"RECEIVE EXIT CMD\n");
        exit_flag = 1;
    }
    dt_debug(TAG,"UPDATECB CURSTATUS:%x \n",state->cur_status);
    return 0;
}

int main(int argc, char **argv)
{
    int ret = 0;
	version_info();
	if (argc < 2) {
		dt_info(""," no enough args\n");
		show_usage();
		return 0;
	}

	void *player_priv;
	dtplayer_para_t *para = dtplayer_alloc_para();
	if(!para)
        return -1;
    strcpy(para->file_name,argv[1]);
    para->update_cb = (void *)update_cb;
	para->no_audio=1;
	//para->no_video=1;
	para->width = 720;
    para->height = 480;
    ret = dtplayer_init(&player_priv, para);
    if(ret <0)
        return -1;
    
    dtplayer_release_para(para);
	dtplayer_start(player_priv);
    //here enter cmd loop
	event_loop(player_priv);
	dt_info("","QUIT DTPLAYER-TEST\n");
    return 0;
}
