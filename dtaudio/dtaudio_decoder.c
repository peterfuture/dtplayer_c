#include "dtaudio.h"

//#define DTAUDIO_DECODER_DUMP 0
#define TAG "AUDIO-DEC"

#define REGISTER_ADEC(X,x)	 	\
	if( ENABLE_ADEC_##X ) 		\
	{							\
		extern dec_audio_wrapper_t adec_##x##_ops; 	\
		register_adec(&adec_##x##_ops); 	\
	}
static dec_audio_wrapper_t *first_adec = NULL;

static void register_adec (dec_audio_wrapper_t * adec)
{
    dec_audio_wrapper_t **p;
    p = &first_adec;
    while (*p != NULL)
        p = &(*p)->next;
    *p = adec;
    dt_info (TAG, "[%s:%d] register adec, name:%s fmt:%d \n", __FUNCTION__, __LINE__, (*p)->name, (*p)->afmt);
    adec->next = NULL;
}

void adec_register_all ()
{
    /*Register all audio_decoder */
#ifdef ENABLE_ADEC_FAAD
    REGISTER_ADEC (FAAD, faad);
#endif

#ifdef ENABLE_ADEC_FFMPEG
    REGISTER_ADEC (FFMPEG, ffmpeg);
#endif
    return;
}

static int select_audio_decoder (dtaudio_decoder_t * decoder)
{
    dec_audio_wrapper_t **p;
    dtaudio_para_t *para = &(decoder->aparam);
    p = &first_adec;
    while (*p != NULL)
    {
        if ((*p)->afmt != para->afmt && (*p)->afmt != AUDIO_FORMAT_UNKOWN)
            p = &(*p)->next;
        else                    //fmt found, or ffmpeg found
            break;
    }
    if (!*p)
    {
        dt_info (DTAUDIO_LOG_TAG, "[%s:%d]no valid audio decoder found afmt:%d\n", __FUNCTION__, __LINE__, para->afmt);
        return -1;
    }
    decoder->dec_wrapper = *p;
    dt_info (TAG, "[%s:%d] select--%s audio decoder \n", __FUNCTION__, __LINE__, (*p)->name);
    return 0;
}

/*Just for pcm test case*/
int transport_direct (char *inbuf, int *inlen, char *outbuf, int *outlen)
{
    int ret = *inlen;
    memcpy (outbuf, inbuf, *inlen);
    *outlen = *inlen;
    return ret;
}

static int64_t pts_exchange (dtaudio_decoder_t * decoder, int64_t pts)
{
    return pts;
}

#define MAX_ONE_FRAME_OUT_SIZE 192000
static void *audio_decode_loop (void *arg)
{
    int ret;
    dtaudio_decoder_t *decoder = (dtaudio_decoder_t *) arg;
    dtaudio_para_t *para = &decoder->aparam;
    dt_av_frame_t frame;
    dec_audio_wrapper_t *dec_wrapper = decoder->dec_wrapper;
    dtaudio_context_t *actx = (dtaudio_context_t *) decoder->parent;
    dt_buffer_t *out = &actx->audio_decoded_buf;
    int declen, fill_size;
    
    //for some type audio, can not read completly frame 
    uint8_t *frame_data = NULL; // point to frame start
    uint8_t *rest_data = NULL;
    int frame_size = 0;
    int rest_size = 0;

    int used;                   // used size after every decode ops

    adec_ctrl_t *pinfo = &decoder->info;
    memset(pinfo,0,sizeof(*pinfo));
    pinfo->channels = para->channels;
    pinfo->samplerate = para->samplerate;
    pinfo->outptr = malloc(MAX_ONE_FRAME_OUT_SIZE);
    pinfo->outsize = MAX_ONE_FRAME_OUT_SIZE;

    dt_info (TAG, "[%s:%d] AUDIO DECODE START \n", __FUNCTION__, __LINE__);
    do
    {
        if (decoder->status == ADEC_STATUS_IDLE)
        {
            dt_info (TAG, "[%s:%d] Idle status ,please wait \n", __FUNCTION__, __LINE__);
            usleep (1000);
            continue;
        }
        if (decoder->status == ADEC_STATUS_EXIT)
        {
            dt_debug (TAG, "[%s:%d] receive decode loop exit cmd \n", __FUNCTION__, __LINE__);
            if (frame_data)
                free (frame_data);
            if (rest_data)
                free (rest_data);
            break;
        }

        /*read frame */
        if (!decoder->parent)
        {
            usleep (100000);
            dt_info (TAG, "[%s:%d] decoder parent is NULL \n", __FUNCTION__, __LINE__);
            continue;
        }
        ret = audio_read_frame (decoder->parent, &frame);
        if (ret < 0 || frame.size <= 0)
        {
            usleep (1000 * 10);
            dt_debug (TAG, "[%s:%d] dtaudio decoder loop read frame failed \n", __FUNCTION__, __LINE__);
            continue;
        }
        //read ok,update current pts, clear the buffer size
        if (frame.pts >= 0)
        {
            if (decoder->pts_first == -1)
            {
                decoder->pts_first = pts_exchange (decoder, frame.pts);
                dt_info (TAG, "first frame pts:%lld dts:%lld duration:%d size:%d\n", decoder->pts_first, frame.dts, frame.duration, frame.size);
            }
            decoder->pts_current = pts_exchange (decoder, frame.pts);
            dt_debug (TAG, "pkt pts:%lld current:%lld duration:%d pts_s:%lld dts:%lld buf_size:%d \n", frame.pts, decoder->pts_current, frame.duration, frame.pts / 90000, frame.dts, decoder->pts_buffer_size);
            decoder->pts_last_valid = decoder->pts_current;
            decoder->pts_buffer_size = 0;
        }
        //repack the frame
        if (frame_data)
        {
            free (frame_data);
            frame_data = NULL;
            frame_size = 0;
        }
        if (rest_size)
            frame_data = malloc (frame.size + rest_size);
        else
            frame_data = frame.data;
        if (!frame_data)
        {
            dt_error (TAG, "malloc audio frame failed ,we will lost one frame\n");
            if (rest_data)
                free (rest_data);
            rest_size = 0;
            continue;
        }

        if (rest_size)          // no rest data
        {
            dt_debug (TAG, "left %d byet last time\n", rest_size);
            memcpy (frame_data, rest_data, rest_size);
            free (rest_data);
            rest_data = NULL;
            memcpy (frame_data + rest_size, frame.data, frame.size);
        }

        frame_size = frame.size + rest_size;
        rest_size = 0;
        used = 0;
        declen = 0;

        pinfo->inptr = frame_data;
        pinfo->inlen = frame_size;
        pinfo->outlen = 0; 

        //free pkt
        frame.data = NULL;
        frame.size = 0;

      DECODE_LOOP:
        if (decoder->status == ADEC_STATUS_EXIT)
        {
            dt_info (TAG, "[%s:%d] receive decode loop exit cmd \n", __FUNCTION__, __LINE__);
            if (frame_data)
                free (frame_data);
            break;
        }
        /*decode frame */
        pinfo->consume = declen;
        used = dec_wrapper->decode_frame (dec_wrapper, pinfo);
        if (used < 0)
        {
            decoder->decode_err_cnt++;
            /*
             * if decoder is ffmpeg,do not restore data if decode failed
             * if decoder is not ffmpeg, restore raw stream packet if decode failed
             * */
            if (!strcmp (dec_wrapper->name, "ffmpeg audio decoder"))
            {
                dt_error (TAG, "[%s:%d] ffmpeg failed to decode this frame, just break\n", __FUNCTION__, __LINE__);
                decoder->decode_offset += pinfo->inlen;
            }
            continue;
        }
        else if (used == 0)
        {
            //maybe need more data
            rest_data = malloc (pinfo->inlen);
            if (rest_data == NULL)
            {
                dt_error ("[%s:%d] rest_data malloc failed\n", __FUNCTION__, __LINE__);
                rest_size = 0;  //skip this frame
                continue;
            }
            memcpy (rest_data, pinfo->inptr, pinfo->inlen);
            rest_size = pinfo->inlen;
            dt_info (TAG, "Maybe we need more data\n");
            continue;
        }
        declen += used;
        pinfo->inlen -= used;
        decoder->decode_offset += used;
        decoder->pts_cache_size = pinfo->outlen;
        decoder->pts_buffer_size += pinfo->outlen;
        if (pinfo->outlen == 0)      //get no data, maybe first time for init
            dt_info (TAG, "GET NO PCM DECODED OUT,used:%d \n",used);

        fill_size = 0;
      REFILL_BUFFER:
        if (decoder->status == ADEC_STATUS_EXIT)
            goto EXIT;
        /*write pcm */
        if (buf_space (out) < pinfo->outlen)
        {
            dt_debug (TAG, "[%s:%d] output buffer do not left enough space ,space=%d level:%d outsie:%d \n", __FUNCTION__, __LINE__, buf_space (out), buf_level (out), pinfo->outlen);
            usleep (1000 * 10);
            goto REFILL_BUFFER;
        }
        ret = buf_put (out, pinfo->outptr + fill_size, pinfo->outlen);
        fill_size += ret;
        pinfo->outlen -= ret;
        decoder->pts_cache_size = pinfo->outlen;
        if (pinfo->outlen > 0)
            goto REFILL_BUFFER;

        if (pinfo->inlen)
            goto DECODE_LOOP;
    }
    while (1);
  EXIT:
    dt_info (TAG, "[file:%s][%s:%d]decoder loop thread exit ok\n", __FILE__, __FUNCTION__, __LINE__);
    /* free adec_ctrl_t buf */
    if(pinfo->outptr)
        free(pinfo->outptr);
    pinfo->outlen = pinfo->outsize = 0;
    pthread_exit (NULL);
    return NULL;
}

int audio_decoder_init (dtaudio_decoder_t * decoder)
{
    int ret = 0;
    pthread_t tid;
    /*select decoder */
    ret = select_audio_decoder (decoder);
    if (ret < 0)
    {
        ret = -1;
        goto ERR0;
    }
    /*init decoder */
    decoder->pts_current = decoder->pts_first = -1;
    decoder->decoder_priv = decoder->aparam.avctx_priv;
    if (decoder->aparam.num == 0 || decoder->aparam.den == 0)
        decoder->aparam.num = decoder->aparam.den = 1;
    else
        dt_info (TAG, "[%s:%d] param: num:%d den:%d\n", __FUNCTION__, __LINE__, decoder->aparam.num, decoder->aparam.den);
    
    dec_audio_wrapper_t *wrapper = decoder->dec_wrapper;
    ret = wrapper->init (wrapper,decoder);
    if (ret < 0)
    {
        ret = -1;
        goto ERR0;
    }
    dt_info (TAG, "[%s:%d] audio decoder init ok\n", __FUNCTION__, __LINE__);
    /*init pcm buffer */
    dtaudio_context_t *actx = (dtaudio_context_t *) decoder->parent;
    int size = DTAUDIO_PCM_BUF_SIZE;
    ret = buf_init (&actx->audio_decoded_buf, size);
    if (ret < 0)
    {
        ret = -1;
        goto ERR1;
    }
    /*create thread */
    ret = pthread_create (&tid, NULL, audio_decode_loop, (void *) decoder);
    if (ret != 0)
    {
        dt_info (DTAUDIO_LOG_TAG, "create audio decoder thread failed\n");
        ret = -1;
        goto ERR2;
    }
    decoder->audio_decoder_pid = tid;
    audio_decoder_start (decoder);
    return ret;
  ERR2:
    buf_release (&actx->audio_decoded_buf);
  ERR1:
    wrapper->release (wrapper);
  ERR0:
    return ret;
}

int audio_decoder_start (dtaudio_decoder_t * decoder)
{
    decoder->status = ADEC_STATUS_RUNNING;
    return 0;
}

int audio_decoder_stop (dtaudio_decoder_t * decoder)
{
    dec_audio_wrapper_t *wrapper = decoder->dec_wrapper;
    /*Decode thread exit */
    decoder->status = ADEC_STATUS_EXIT;
    pthread_join (decoder->audio_decoder_pid, NULL);
    wrapper->release (wrapper);
    /*uninit buf */
    dtaudio_context_t *actx = (dtaudio_context_t *) decoder->parent;
    buf_release (&actx->audio_decoded_buf);
    return 0;
}

//====status & pts 
#define DTAUDIO_PTS_FREQ    90000
int64_t audio_decoder_get_pts (dtaudio_decoder_t * decoder)
{
    int64_t pts, delay_pts;
    int frame_num;
    int channels, sample_rate, bps;

    int len;
    float pts_ratio;
    dtaudio_context_t *actx = (dtaudio_context_t *) decoder->parent;
    dt_buffer_t *out = &actx->audio_decoded_buf;

    if (decoder->status == ADEC_STATUS_IDLE || decoder->status == ADEC_STATUS_EXIT)
        return -1;
    channels = decoder->aparam.dst_channels;
    sample_rate = decoder->aparam.samplerate;
    bps = decoder->aparam.bps;
    pts_ratio = (float) DTAUDIO_PTS_FREQ / sample_rate;
    pts = 0;
    if (-1 == decoder->pts_first)
        return -1;
    if (-1 == decoder->pts_current) //case 1 current_pts valid
    {
        //if(decoder->pts_last_valid)
        //    pts=decoder->pts_last_valid;
        //len = decoder->pts_cache_size + decoder->pts_buffer_size - out->level;
        len = decoder->pts_buffer_size;
        frame_num = (len) / (bps * channels / 8);
        pts += (frame_num) * pts_ratio;
        pts += decoder->pts_first;
        dt_debug (TAG, "[%s:%d] first_pts:%llu pts:%llu pts_s:%d frame_num:%d len:%d pts_ratio:%5.1f\n", __FUNCTION__, __LINE__, decoder->pts_first, pts, pts / 90000, frame_num, len, pts_ratio);
        return pts;
    }
    //case 2 current_pts invalid,calc pts mentally
    pts = decoder->pts_current;
    len = decoder->pts_buffer_size - out->level - decoder->pts_cache_size;
    frame_num = (len) / (bps * channels / 8);
    delay_pts = (frame_num) * pts_ratio;
    dt_debug (TAG, "[%s:%d] current_pts:%lld delay_pts:%lld  pts_s:%lld frame_num:%d pts_ratio:%f\n", __FUNCTION__, __LINE__, decoder->pts_current, delay_pts, pts / 90000, frame_num, pts_ratio);
    pts += delay_pts;
    if (pts < 0)
        pts = 0;
    return pts;
}
