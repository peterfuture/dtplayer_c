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

static int img_convert2(AVPicture * dst, int dst_pix_fmt, const AVPicture * src,
			int src_pix_fmt, int src_width, int src_height,
			int dest_width, int dest_height)
{
	int ret = 0;
	int w;
	int h;
	static struct SwsContext *pSwsCtx;
	w = src_width;
	h = src_height;
	//pSwsCtx = sws_getContext(w, h, src_pix_fmt,dest_width, dest_height, dst_pix_fmt,SWS_BICUBIC, NULL, NULL, NULL);
	pSwsCtx =sws_getCachedContext(pSwsCtx, w, h, src_pix_fmt, dest_width,
				 dest_height, dst_pix_fmt, SWS_BICUBIC, NULL,
				 NULL, NULL);
	ret =sws_scale(pSwsCtx, src->data, src->linesize, 0, h, dst->data,dst->linesize);
	//sws_freeContext(pSwsCtx);
	if (ret > 0)
		return 0;
	else
		return -1;
}

void SaveFrame(AVFrame * pFrame, int width, int height, int iFrame)
{
	FILE *pFile;
	char szFilename[32];
	int y;

	// Open file
	sprintf(szFilename, "frame%d.ppm", iFrame);
	pFile = fopen(szFilename, "wb");
	if (pFile == NULL)
		return;

	// Write header
	fprintf(pFile, "P6\n%d %d\n255\n", width, height);

	// Write pixel data
	for (y = 0; y < height; y++)
		fwrite(pFrame->data[0] + y * pFrame->linesize[0], 1, width * 3,
		       pFile);

	// Close file
	fclose(pFile);
}

int ffmpeg_vdec_init(dtvideo_decoder_t * decoder)
{
	//select video decoder and call init
	AVCodec *codec = NULL;
	AVCodecContext *avctxp = (AVCodecContext *) decoder->decoder_priv;
    avctxp->thread_count = 1; //do not use multi thread,may crash
	//printf("file:%s [%s:%d] param-- w:%d h:%d id:%d extr_si:%d \n",__FILE__,__FUNCTION__,__LINE__,avctxp->width,avctxp->height,id,avctxp->extradata_size);
	enum AVCodecID id = avctxp->codec_id;
	codec = avcodec_find_decoder(id);
	if (NULL == codec) {
		dt_error(TAG,"file:%s [%s:%d] video codec find failed \n", __FILE__,
		       __FUNCTION__, __LINE__);
		return -1;
	}
	if (avcodec_open2(avctxp, codec, NULL) < 0) {
		dt_error(TAG,"file:%s [%s:%d] video codec open failed \n", __FILE__,
		       __FUNCTION__, __LINE__);
		return -1;
	}
	dt_info(TAG," [%s:%d] ffmpeg dec init ok \n",__FUNCTION__,__LINE__);
	//alloc one frame for decode
	frame = avcodec_alloc_frame();
	return 0;
}

static void output_picture(dtvideo_decoder_t * decoder, AVFrame * src_frame,
			   int64_t pts, AVPicture_t ** p_pict)
{
	int ret;
	uint8_t *buffer;
	int numBytes;
	AVPicture_t *pict;
	pict = malloc(sizeof(AVPicture_t));
	memset(pict, 0, sizeof(AVPicture_t));
	AVPicture *dest_pic = (AVPicture *) (pict);
	// Allocate an AVFrame structure
	numBytes =avpicture_get_size(decoder->para.d_pixfmt, decoder->para.d_width,decoder->para.d_height);
	buffer = (uint8_t *) malloc(numBytes * sizeof(uint8_t));
	avpicture_fill((AVPicture *) dest_pic, buffer, decoder->para.d_pixfmt,decoder->para.d_width, decoder->para.d_height);

	// Convert the image from its native format to RGB
	ret =img_convert2((AVPicture *) dest_pic, decoder->para.d_pixfmt,
			 (AVPicture *) src_frame, decoder->para.s_pixfmt,
			 decoder->para.s_width, decoder->para.s_height,
			 decoder->para.d_width, decoder->para.d_height);
	if (ret == -1)
		goto FAIL;
	(pict)->pts = pts;
	*p_pict = pict;
	dt_debug(TAG,"==line  %d %d %d %d \n",(pict)->linesize[0],(pict)->linesize[1],(pict)->linesize[2],(pict)->linesize[3]);
	return;
FAIL:
	av_free(pict);
	*p_pict = NULL;
	return;

}

//1 get one frame 0 failed -1 err
int ffmpeg_vdec_decode(dtvideo_decoder_t * decoder, dt_av_frame_t * dt_frame,
		       AVPicture_t ** pic)
{
	AVCodecContext *avctxp = (AVCodecContext *) decoder->decoder_priv;
	dt_debug(TAG,"file:%s [%s:%d] param-- w:%d h:%d  extr_si:%d \n",__FILE__,__FUNCTION__,__LINE__,avctxp->width,avctxp->height,avctxp->extradata_size);
	int got_picture = 0;
	AVPacket pkt;

    pkt.data = dt_frame->data;
	pkt.size = dt_frame->size;
	pkt.pts = dt_frame->pts;
	pkt.dts = dt_frame->dts;
	pkt.side_data_elems = 0;
    pkt.buf = NULL;	
    avcodec_decode_video2(avctxp, frame, &got_picture, &pkt);
	if (got_picture) {
		//output_picture(decoder, frame, frame->best_effort_timestamp,pic);
		output_picture(decoder, frame, av_frame_get_best_effort_timestamp(frame),pic);
		//dt_info(TAG,"==got picture pts:%llu timestamp:%lld \n",frame->pkt_pts,frame->best_effort_timestamp);
        return 1;
	}
	//inbuf will be freed outside
	return 0;
}

int ffmpeg_vdec_release(dtvideo_decoder_t * decoder)
{
	AVCodecContext *avctxp = (AVCodecContext *) decoder->decoder_priv;
	avcodec_close(avctxp);
	avcodec_free_frame(&frame);
	return 0;
}

dec_video_wrapper_t vdec_ffmpeg_ops = {
	.name = "ffmpeg video decoder",
	.vfmt = VIDEO_FORMAT_UNKOWN,	//support all vfmt
	.type = DT_TYPE_VIDEO,
	.init = ffmpeg_vdec_init,
	.decode_frame = ffmpeg_vdec_decode,
	.release = ffmpeg_vdec_release,
};
