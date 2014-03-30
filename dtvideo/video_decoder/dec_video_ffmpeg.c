#ifdef ENABLE_VDEC_FFMPEG

/*
 *video decoder interface using ffmpeg
 * */

#include "libavcodec/avcodec.h"
#include "../dtvideo_decoder.h"

//include ffmpeg header
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"

#define TAG "VDEC-FFMPEG"
static AVFrame *frame;
static struct SwsContext *pSwsCtx = NULL;;

static int img_convert (AVPicture * dst, int dst_pix_fmt, AVFrame * src, int src_pix_fmt, int src_width, int src_height, int dest_width, int dest_height)
{
    //pSwsCtx = sws_getContext(src_width, src_height, src_pix_fmt,dest_width, dest_height, dst_pix_fmt,SWS_BICUBIC, NULL, NULL, NULL);
    pSwsCtx = sws_getCachedContext (pSwsCtx, src_width, src_height, src->format, dest_width, dest_height, dst_pix_fmt, SWS_BICUBIC, NULL, NULL, NULL);
    sws_scale (pSwsCtx, src->data, src->linesize, 0, src_height, dst->data, dst->linesize);
    return 0;
}

#if 0
static void SaveFrame (AVFrame * pFrame, int width, int height, int iFrame)
{
    FILE *pFile;
    char szFilename[32];
    int y;

    // Open file
    sprintf (szFilename, "frame%d.ppm", iFrame);
    pFile = fopen (szFilename, "wb");
    if (pFile == NULL)
        return;

    // Write header
    fprintf (pFile, "P6\n%d %d\n255\n", width, height);

    // Write pixel data
    for (y = 0; y < height; y++)
        fwrite (pFrame->data[0] + y * pFrame->linesize[0], 1, width * 3, pFile);

    // Close file
    fclose (pFile);
}
#endif

int ffmpeg_vdec_init (dtvideo_decoder_t * decoder)
{
    //select video decoder and call init
    AVCodec *codec = NULL;
    AVCodecContext *avctxp = (AVCodecContext *) decoder->decoder_priv;
    avctxp->thread_count = 1;   //do not use multi thread,may crash
    enum AVCodecID id = avctxp->codec_id;
    codec = avcodec_find_decoder (id);
    if (NULL == codec)
    {
        dt_error (TAG, "[%s:%d] video codec find failed \n", __FUNCTION__, __LINE__);
        return -1;
    }
    if (avcodec_open2 (avctxp, codec, NULL) < 0)
    {
        dt_error (TAG, "[%s:%d] video codec open failed \n", __FUNCTION__, __LINE__);
        return -1;
    }
    dt_info (TAG, " [%s:%d] ffmpeg dec init ok \n", __FUNCTION__, __LINE__);
    //alloc one frame for decode
    frame = av_frame_alloc ();
    return 0;
}

static int output_picture (dtvideo_decoder_t * decoder, AVFrame * src_frame, int64_t pts, AVPicture_t ** p_pict)
{
    uint8_t *buffer;
    int numBytes;
    AVPicture_t *pict = malloc (sizeof (AVPicture_t));
    memset (pict, 0, sizeof (AVPicture_t));
    AVPicture *dest_pic = (AVPicture *) (pict);
    // Allocate an AVFrame structure
    numBytes = avpicture_get_size (decoder->para.d_pixfmt, decoder->para.d_width, decoder->para.d_height);
    buffer = (uint8_t *) malloc (numBytes * sizeof (uint8_t));
    avpicture_fill ((AVPicture *) dest_pic, buffer, decoder->para.d_pixfmt, decoder->para.d_width, decoder->para.d_height);

    // Convert the image from its native format to YV12
    img_convert ((AVPicture *) dest_pic, decoder->para.d_pixfmt, src_frame, decoder->para.s_pixfmt, decoder->para.s_width, decoder->para.s_height, decoder->para.d_width, decoder->para.d_height);
    pict->pts = pts;
    *p_pict = pict;
    return 0;
}

/*
 *decode one frame using ffmpeg
 *
 * return value
 * 1  dec one frame and get one frame
 * 0  dec one frame without out
 * -1 err occured while decoding 
 *
 * */

int ffmpeg_vdec_decode (dtvideo_decoder_t * decoder, dt_av_frame_t * dt_frame, AVPicture_t ** pic)
{
    int ret = 0;
    AVCodecContext *avctxp = (AVCodecContext *) decoder->decoder_priv;
    dt_debug (TAG, "[%s:%d] param-- w:%d h:%d  extr_si:%d \n", __FUNCTION__, __LINE__, avctxp->width, avctxp->height, avctxp->extradata_size);
    int got_picture = 0;
    AVPacket pkt;

    pkt.data = dt_frame->data;
    pkt.size = dt_frame->size;
    pkt.pts = dt_frame->pts;
    pkt.dts = dt_frame->dts;
    pkt.side_data_elems = 0;
    pkt.buf = NULL;
    avcodec_decode_video2 (avctxp, frame, &got_picture, &pkt);
    if (got_picture)
    {
        ret = output_picture (decoder, frame, av_frame_get_best_effort_timestamp (frame), pic);
        if (ret == -1)
            ret = 0;
        else
            ret = 1;
        //dt_info(TAG,"==got picture pts:%llu timestamp:%lld \n",frame->pkt_pts,frame->best_effort_timestamp);
    }
    av_frame_unref (frame);
    //no need to free dt_frame
    //will be freed outside
    return ret;
}

int ffmpeg_vdec_release (dtvideo_decoder_t * decoder)
{
    AVCodecContext *avctxp = (AVCodecContext *) decoder->decoder_priv;
    avcodec_close (avctxp);
    av_frame_free (&frame);
    if (pSwsCtx)
        sws_freeContext (pSwsCtx);
    pSwsCtx = NULL;
    return 0;
}

dec_video_wrapper_t vdec_ffmpeg_ops = {
    .name = "ffmpeg video decoder",
    .vfmt = VIDEO_FORMAT_UNKOWN, //support all vfmt
    .type = DT_TYPE_VIDEO,
    .init = ffmpeg_vdec_init,
    .decode_frame = ffmpeg_vdec_decode,
    .release = ffmpeg_vdec_release,
};

#endif /*ENABLE_VDEC_FFMPEG*/
