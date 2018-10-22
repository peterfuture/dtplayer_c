#include "dt_setting.h"
#include "dt_error.h"
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

static void *player_io_thread(dtplayer_context_t * dtp_ctx);

static int player_read_frame(dtplayer_context_t * dtp_ctx, dt_av_pkt_t ** pkt)
{
    return dtdemuxer_read_frame(dtp_ctx->demuxer_priv, pkt);
}

static int player_write_frame(dtplayer_context_t * dtp_ctx, dt_av_pkt_t * frame)
{
    return dthost_write_frame(dtp_ctx->host_priv, frame, frame->type);
}

int start_io_thread(dtplayer_context_t * dtp_ctx)
{
    pthread_t tid;
    dtp_ctx->io_loop.status = IO_LOOP_PAUSED;
    dtp_ctx->io_loop.flag = IO_FLAG_NULL;
    int ret = pthread_create(&tid, NULL, (void *) &player_io_thread,
                             (void *) dtp_ctx);
    if (ret != 0) {
        dt_error(TAG, "file:%s [%s:%d] player io thread crate failed \n", __FILE__,
                 __FUNCTION__, __LINE__);
        return -1;
    }
    dtp_ctx->io_loop.tid = tid;
    dtp_ctx->io_loop.status = IO_LOOP_RUNNING;
    dt_info(TAG, "IO Thread start ok\n");
    return 0;
}

int pause_io_thread(dtplayer_context_t * dtp_ctx)
{
    dtp_ctx->io_loop.flag = IO_FLAG_PAUSE;
    while (dtp_ctx->io_loop.status != IO_LOOP_PAUSED) {
        usleep(100);
    }
    dtp_ctx->io_loop.flag = IO_FLAG_NULL;

    return 0;
}

int resume_io_thread(dtplayer_context_t * dtp_ctx)
{
    dtp_ctx->io_loop.flag = IO_FLAG_NULL;
    dtp_ctx->io_loop.status = IO_LOOP_RUNNING;
    return 0;
}

int stop_io_thread(dtplayer_context_t * dtp_ctx)
{
    dtp_ctx->io_loop.flag = IO_FLAG_NULL;
    dtp_ctx->io_loop.status = IO_LOOP_QUIT;
    pthread_join(dtp_ctx->io_loop.tid, NULL);
    //dt_info(TAG,"io thread quit ok\n");
    return 0;
}

int io_thread_running(dtplayer_context_t *dtp_ctx)
{
    return (dtp_ctx->io_loop.status != IO_LOOP_QUIT);
}

static void *player_io_thread(dtplayer_context_t * dtp_ctx)
{
    player_ctrl_t *play_ctrl = &dtp_ctx->ctrl_info;
    io_loop_t *io_ctl = &dtp_ctx->io_loop;
    dt_av_pkt_t *pkt = NULL;
    int pkt_valid = 0;
    int ret = 0;

    int dump_mode = dtp_setting.player_dump_mode;
    dt_info(TAG, "dump mode:%d 0 nodump 1dumpaudio 2dumpvideo 3dump sub \n",
            dump_mode);

    do {
        //usleep (1000);
        if (io_ctl->flag == IO_FLAG_PAUSE) {
            io_ctl->status = IO_LOOP_PAUSED;
        }
        if (io_ctl->status == IO_LOOP_QUIT) {
            goto QUIT;
        }
        if (io_ctl->status == IO_LOOP_PAUSED) {
            //when pause read thread,we need skip currnet pkt
            if (pkt_valid == 1) {
                dtp_packet_free(pkt);
            }
            pkt_valid = 0;
            usleep(100);
            continue;
        }
        /*io ops */
        if (pkt_valid == 1) {
            goto WRITE_PACKET;
        }
        pkt = NULL;
        ret = player_read_frame(dtp_ctx, &pkt);
        if (ret == DTERROR_NONE) {
            pkt_valid = 1;
        } else {
            if (ret == DTERROR_READ_EOF) {
                io_ctl->status = IO_LOOP_QUIT;
                dtp_ctx->ctrl_info.eof_flag = 1;
            }
            usleep(100);
            continue;
        }

        // key frame check
        if (play_ctrl->ctrl_wait_key_frame == 1) {
            if (pkt->type == DTP_MEDIA_TYPE_VIDEO && pkt->key_frame == 1) {
                play_ctrl->ctrl_wait_key_frame = 0;
            } else {
                dt_debug(TAG, "wait key frame skip:type:%s pts:%lld (%lld ms)\n",
                         (pkt->type == DTP_MEDIA_TYPE_VIDEO) ? "VIDEO" : "AUDIO", pkt->pts,
                         pkt->pts / 90);
                if (pkt_valid == 1) {
                    dtp_packet_free(pkt);
                    pkt = NULL;
                }
                pkt_valid = 0;
                continue;
            }
        }

        dt_debug(TAG, "read ok size:%d pts:%llx \n", pkt->size, pkt->pts);
WRITE_PACKET:
        ret = player_write_frame(dtp_ctx, pkt);
        // dump code
        if (ret == DTERROR_NONE) {
            dt_debug(TAG, "player write ok \n");
            pkt_valid = 0;

            if (pkt->type == DTP_MEDIA_TYPE_AUDIO && dump_mode == 1) {
                FILE *fp = fopen("./raw_audio.out", "ab+");
                fwrite(pkt->data, 1, pkt->size, fp);
                fclose(fp);
            }
            if (pkt->type == DTP_MEDIA_TYPE_VIDEO && dump_mode == 2) {
                FILE *fp = fopen("./raw_video.out", "ab+");
                fwrite(pkt->data, 1, pkt->size, fp);
                fclose(fp);
            }
            if (pkt->type == DTP_MEDIA_TYPE_VIDEO && dump_mode == 3) {
                FILE *fp = fopen("./raw_sub.out", "ab+");
                fwrite(pkt->data, 1, pkt->size, fp);
                fclose(fp);
            }
        } else {
            dt_debug(TAG, "write pkt failed , write again \n");
            usleep(10000);
        }
    } while (1);
QUIT:
    dt_info(TAG, "io thread quit ok\n");
    pthread_exit(NULL);
    return 0;
}
