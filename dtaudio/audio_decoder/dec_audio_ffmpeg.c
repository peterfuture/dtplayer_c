#include "../dtaudio_decoder.h"

//include ffmpeg header
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
//#include "libavresample/avresample.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"

#define TAG "AUDIO-DECODER-FFMPEG"
static AVFrame *frame;
AVCodecContext *avctxp;

static enum AVCodecID convert_to_id(int format)
{
    switch(format)
    {
        case AUDIO_FORMAT_AAC:
            return AV_CODEC_ID_AAC;
        default:
            return 0;
    }
}

static AVCodecContext * alloc_ffmpeg_ctx(dtaudio_decoder_t *decoder)
{
    AVCodecContext *ctx = avcodec_alloc_context3(NULL);
    if(!ctx)
        return NULL;
    
    ctx->codec_type = AVMEDIA_TYPE_AUDIO;
    ctx->codec_id = convert_to_id(decoder->aparam.afmt);
    if(ctx->codec_id == 0)
    {
        av_free(ctx); 
        return NULL;
    }
    ctx->channels = decoder->aparam.channels;
    ctx->sample_rate = decoder->aparam.samplerate;
    ctx->sample_fmt = AV_SAMPLE_FMT_S16;
    ctx->internal = NULL; 
    //maybe ffmpeg not register
    av_register_all ();
    return ctx;
}

int ffmpeg_adec_init (dec_audio_wrapper_t *wrapper, void *parent)
{
    wrapper->parent = parent;
    dtaudio_decoder_t *decoder = (dtaudio_decoder_t *)parent;
    //select video decoder and call init
    dt_info (TAG, "FFMPEG AUDIO DECODER INIT ENTER\n");
    AVCodec *codec = NULL;
    if(decoder->decoder_priv)
        avctxp = (AVCodecContext *) decoder->decoder_priv;
    else
        avctxp = alloc_ffmpeg_ctx(decoder); 
    if(!avctxp)
        return -1;
    enum AVCodecID id = avctxp->codec_id;
    dt_info (TAG, "[%s:%d] param-- src channel:%d sample:%d id:%d format:%d \n", __FUNCTION__, __LINE__, avctxp->channels, avctxp->sample_rate,id,decoder->aparam.afmt);
    dt_info (TAG, "[%s:%d] param-- dst channels:%d samplerate:%d \n", __FUNCTION__, __LINE__, decoder->aparam.dst_channels,decoder->aparam.dst_samplerate);
    codec = avcodec_find_decoder (id);
    if (NULL == codec)
    {
        dt_error (TAG, "[%s:%d] video codec find failed \n", __FILE__, __FUNCTION__, __LINE__);
        return -1;
    }

    if (avcodec_open2 (avctxp, codec, NULL) < 0)
    {
        dt_error (TAG, "[%s:%d] video codec open failed \n", __FUNCTION__, __LINE__);
        return -1;
    }
    dt_info (TAG, "[%s:%d] ffmpeg dec init ok \n", __FUNCTION__, __LINE__);
    //alloc one frame for decode
    frame = av_frame_alloc ();
    return 0;
}

static void audio_convert (dtaudio_decoder_t *decoder, AVFrame * dst, AVFrame * src)
{
    int nb_sample;
    int dst_buf_size;
    int out_channels;
    //for audio post processor
    //struct SwsContext *m_sws_ctx = NULL;
    struct SwrContext *m_swr_ctx = NULL;
    //ResampleContext *m_resample_ctx=NULL;
    enum AVSampleFormat src_fmt = avctxp->sample_fmt;
    enum AVSampleFormat dst_fmt = AV_SAMPLE_FMT_S16;
    
    dst->linesize[0] = src->linesize[0];
    *dst = *src;

    dst->data[0] = NULL;
    out_channels = decoder->aparam.dst_channels;
    nb_sample = frame->nb_samples;
    dst_buf_size = nb_sample * av_get_bytes_per_sample (dst_fmt) * out_channels;
    dst->data[0] = (uint8_t *) av_malloc (dst_buf_size);

    avcodec_fill_audio_frame (dst, out_channels, dst_fmt, dst->data[0], dst_buf_size, 0);
    dt_debug (TAG, "SRCFMT:%d dst_fmt:%d \n", src_fmt, dst_fmt);
    /* resample toAV_SAMPLE_FMT_S16 */
    if (src_fmt != dst_fmt || out_channels != decoder->aparam.channels)
    {
        if (!m_swr_ctx)
        {
            uint64_t in_channel_layout = av_get_default_channel_layout (avctxp->channels);
            uint64_t out_channel_layout = av_get_default_channel_layout (out_channels);
            m_swr_ctx = swr_alloc_set_opts (NULL, out_channel_layout, dst_fmt, avctxp->sample_rate, in_channel_layout, src_fmt, avctxp->sample_rate, 0, NULL);
            swr_init (m_swr_ctx);
        }
        uint8_t **out = (uint8_t **) & dst->data;
        const uint8_t **in = (const uint8_t **) src->extended_data;
        if (m_swr_ctx)
        {
            int ret, out_count;
            out_count = nb_sample;
            ret = swr_convert (m_swr_ctx, out, out_count, in, nb_sample);
            if (ret < 0)
            {
                //set audio mute
                memset (dst->data[0], 0, dst_buf_size);
                printf ("audio convert failed, set mute data\n");
            }
        }
    }
    else                        // no need to convert ,just copy
    {
        memcpy (dst->data[0], src->data[0], src->linesize[0]);
    }
    //free context
    if (m_swr_ctx != NULL)
        swr_free (&m_swr_ctx);
    //if(m_resample_ctx!=NULL)
    //    audio_resample_close(m_resample_ctx);
}

//1 get one frame 0 failed -1 err
int ffmpeg_adec_decode (dec_audio_wrapper_t *wrapper, adec_ctrl_t *pinfo)
{
    int got_samples = 0;
    int ret = 0;
    int data_size = 0;
    AVFrame frame_tmp;
    //AVFrame frame;
    AVPacket pkt;
    pkt.data = pinfo->inptr;
    pkt.size = pinfo->inlen;
    pkt.side_data_elems = 0;

    dtaudio_decoder_t *decoder = (dtaudio_decoder_t *)wrapper->parent;
    memset (&frame_tmp, 0, sizeof (frame_tmp));
    dt_debug (TAG, "start decode size:%d %02x %02x \n", pkt.size, pkt.data[0], pkt.data[1]);
    ret = avcodec_decode_audio4 (avctxp, frame, &got_samples, &pkt);
    dt_debug (TAG, "start decode size:%d %02x %02x %02x %02x \n", pkt.size, pkt.data[0], pkt.data[1], pkt.data[2], pkt.data[3]);
    if (ret < 0)
    {
        dt_error (TAG, "decode failed ret:%d \n", ret);
        goto EXIT;
    }

    if (!got_samples)           //decode return 0 
    {
        dt_error (TAG, "get no samples out \n");
        pinfo->outlen = 0;
        goto EXIT;
    }
    data_size = av_samples_get_buffer_size (frame->linesize, avctxp->channels, frame->nb_samples, avctxp->sample_fmt, 1);
    if (data_size > 0)
    {
        audio_convert (decoder, &frame_tmp, frame);
        //out frame too large, realloc out buf
        if(pinfo->outsize < frame_tmp.linesize[0])
        {
            pinfo->outptr = realloc(pinfo->outptr,frame_tmp.linesize[0] * 2);
            pinfo->outsize = frame_tmp.linesize[0] * 2;
        }
        
        memcpy (pinfo->outptr, frame_tmp.data[0], frame_tmp.linesize[0]);
        pinfo->outlen = frame_tmp.linesize[0];
    }
    else
    {
        dt_error (TAG, "data_size invalid: size:%d outlen:%d \n", data_size, pinfo->outlen);
        pinfo->outlen = 0;
    }

  EXIT:
    if (frame_tmp.data[0])
        free (frame_tmp.data[0]);
    frame_tmp.data[0] = NULL;
    return ret;
}

int ffmpeg_adec_release (dec_audio_wrapper_t *wrapper)
{
    avcodec_close (avctxp);
    avctxp = NULL;
    av_frame_free (&frame);
    return 0;
}

dec_audio_wrapper_t adec_ffmpeg_ops = {
    .name = "ffmpeg audio decoder",
    .afmt = AUDIO_FORMAT_UNKOWN, //support all afmt
    .type = DT_TYPE_AUDIO,
    .init = ffmpeg_adec_init,
    .decode_frame = ffmpeg_adec_decode,
    .release = ffmpeg_adec_release,
};
