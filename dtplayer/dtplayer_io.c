#include "dtplayer.h"
#include "dtdemuxer_api.h"

#define TAG "PLAYER-IO"
//status
#define IO_LOOP_PAUSED 0
#define IO_LOOP_RUNNING 1
#define IO_LOOP_QUIT 2
//flag
#define IO_FLAG_NULL 0
#define IO_FLAG_PAUSE 1

static void *player_io_thread (dtplayer_context_t * dtp_ctx);

static int player_read_frame (dtplayer_context_t * dtp_ctx, dt_av_frame_t * frame)
{
	return dtdemuxer_read_frame (dtp_ctx->demuxer_priv, frame);
}

static int player_write_frame (dtplayer_context_t * dtp_ctx, dt_av_frame_t * frame)
{
	return dthost_write_frame (dtp_ctx->host_priv, frame, frame->type);
}

int start_io_thread (dtplayer_context_t * dtp_ctx)
{
	pthread_t tid;
	dtp_ctx->io_loop.status = IO_LOOP_PAUSED;
	dtp_ctx->io_loop.flag = IO_FLAG_NULL;
	int ret = pthread_create (&tid, NULL, (void *) &player_io_thread, (void *) dtp_ctx);
	if (ret != 0)
	{
		dt_error (TAG "file:%s [%s:%d] player io thread crate failed \n", __FILE__, __FUNCTION__, __LINE__);
		return -1;
	}
	dtp_ctx->io_loop.tid = tid;
	dtp_ctx->io_loop.status = IO_LOOP_RUNNING;
	dt_info (TAG, "IO Thread start ok\n");
	return 0;
}

int pause_io_thread (dtplayer_context_t * dtp_ctx)
{
	dtp_ctx->io_loop.flag = IO_FLAG_PAUSE;
	while (dtp_ctx->io_loop.status != IO_LOOP_PAUSED)
		usleep (1000);
	dtp_ctx->io_loop.flag = IO_FLAG_NULL;

	return 0;
}

int resume_io_thread (dtplayer_context_t * dtp_ctx)
{
	dtp_ctx->io_loop.flag = IO_FLAG_NULL;
	dtp_ctx->io_loop.status = IO_LOOP_RUNNING;
	return 0;
}

int stop_io_thread (dtplayer_context_t * dtp_ctx)
{
	dtp_ctx->io_loop.flag = IO_FLAG_NULL;
	dtp_ctx->io_loop.status = IO_LOOP_QUIT;
	pthread_join (dtp_ctx->io_loop.tid, NULL);
	//dt_info(TAG,"io thread quit ok\n");
	return 0;
}

static void *player_io_thread (dtplayer_context_t * dtp_ctx)
{
	io_loop_t *io_ctl = &dtp_ctx->io_loop;
	dt_av_frame_t frame;
	int frame_valid = 0;
	int ret = 0;
	do
	{
		usleep (10000);
		if (io_ctl->flag == IO_FLAG_PAUSE)
			io_ctl->status = IO_LOOP_PAUSED;
		if (io_ctl->status == IO_LOOP_QUIT)
			goto QUIT;
		if (io_ctl->status == IO_LOOP_PAUSED)
		{
			//when pause read thread,we need skip currnet pkt
			if (frame_valid == 1)
				free (frame.data);
			frame_valid = 0;
			usleep (10000);
			continue;
		}
		/*io ops */
		if (frame_valid == 1)
			goto WRITE_FRAME;
		ret = player_read_frame (dtp_ctx, &frame);
		if (ret == DTERROR_NONE)
			frame_valid = 1;
		else
		{
			if (ret == DTERROR_READ_EOF)
			{
				io_ctl->status = IO_LOOP_QUIT;
				dtp_ctx->ctrl_info.eof_flag = 1;
			}
			usleep (1000);
			continue;
		}
WRITE_FRAME:
		ret = player_write_frame (dtp_ctx, &frame);
		if (ret == DTERROR_NONE)
			frame_valid = 0;
		else
			dt_warning (TAG, "write frame failed , write again \n");
	}
	while (1);
QUIT:
	dt_info (TAG, "io thread quit ok\n");
	pthread_exit (NULL);
	return 0;
}
