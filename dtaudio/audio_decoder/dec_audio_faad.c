#ifdef ENABLE_ADEC_FAAD

#include "../dtaudio_decoder.h"

#include <faad.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define TAG "ADEC-FAAD"

#define FAAD_MIN_STREAMSIZE 768 /* 6144 bits/channel */
#define FAAD_MAX_CHANNELS 6

typedef struct{
    NeAACDecHandle faad_dec;
    NeAACDecFrameInfo faad_finfo;
    NeAACDecConfigurationPtr faad_cfg;
    int faad_failed;
    
    long bytes_into_buffer;
    long bytes_consumed;
    uint8_t *buffer;

} faad_ctx_t;

static int faad_init (dec_audio_wrapper_t *wrapper,void *parent)
{
    wrapper->parent = parent;
    faad_ctx_t *this = (faad_ctx_t *)malloc(sizeof(faad_ctx_t));
    if(!this)
        return -1;
    memset(this,0,sizeof(*this));
    wrapper->adec_priv = this;
    
    return 0;
}

static int faad_open_dec(faad_ctx_t *this)
{
    uint8_t *buf = this->buffer;
    int tagsize = 0;
    int bitrate;
    int header_type = 0;
    unsigned long samplerate;
    unsigned char channels; 
    int bread;
    NeAACDecHandle hDecoder ;
    NeAACDecConfigurationPtr config ;

    if (!memcmp(buf, "ID3", 3))
    {
        /* high bit is not used */
        tagsize = (buf[6] << 21) | (buf[7] << 14) |
            (buf[8] <<  7) | (buf[9] <<  0);

        tagsize += 10;
        this->bytes_consumed += 10;
    }

    hDecoder = NeAACDecOpen();

    /* Set the default object type and samplerate */
    /* This is useful for RAW AAC files */
    config = NeAACDecGetCurrentConfiguration(hDecoder);
    config->defSampleRate = 0;
    config->defObjectType = LC;
    config->outputFormat = FAAD_FMT_16BIT;
    config->downMatrix = 0;
    config->useOldADTSFormat = 0;
    //config->dontUpSampleImplicitSBR = 1;
    NeAACDecSetConfiguration(hDecoder, config);

    /* get AAC infos for printing */
    header_type = 0;
    if ((buf[0] == 0xFF) && ((buf[1] & 0xF6) == 0xF0))
        header_type = 1;
    if (memcmp(buf, "ADIF", 4) == 0) {
        int skip_size = (buf[4] & 0x80) ? 9 : 0;
        bitrate = ((unsigned int)(buf[4 + skip_size] & 0x0F)<<19) |
            ((unsigned int)buf[5 + skip_size]<<11) |
            ((unsigned int)buf[6 + skip_size]<<3) |
            ((unsigned int)buf[7 + skip_size] & 0xE0);
        header_type = 2;
        dt_info(TAG,"bitrate:%d \n",bitrate);
    }

    if ((bread = NeAACDecInit(hDecoder, this->buffer,this->bytes_into_buffer, &samplerate, &channels)) < 0)
    {
        /* If some error initializing occured, skip the file */
        dt_error(TAG, "Error initializing decoder library.\n");
        NeAACDecClose(hDecoder);
        return -1;
    }

    /* print AAC file info */
    switch (header_type)
    {
    case 0:
        dt_info(TAG, "RAW\n\n");
        break;
    case 1:
        dt_info(TAG, "ADTS, channels:%d, %d Hz\n\n",channels, samplerate);
        break;
    case 2:
        dt_info(TAG, "ADIF, channels:%d, %d Hz\n\n",channels, samplerate);
        break;
    }
   
    this->faad_dec = hDecoder;
    this->faad_cfg = config;
    
    dt_info(TAG,"faad open dec ok,consume:%d \n",this->bytes_consumed);
    return 0;
}

static int faad_decode (dec_audio_wrapper_t *wrapper, adec_ctrl_t *pinfo)
{
 
    faad_ctx_t *this = (faad_ctx_t *) wrapper->adec_priv;
    this->buffer = pinfo->inptr + pinfo->consume;
    this->bytes_into_buffer = pinfo->inlen;
    this->bytes_consumed = 0; 
    
    if( !this->faad_dec)
    {
        if(faad_open_dec(this) < 0)
            return -1; // skip this frame
        return this->bytes_consumed;
    }

    void *sample_buffer;
    NeAACDecHandle hDecoder = this->faad_dec;
    NeAACDecFrameInfo *frameInfo = &this->faad_finfo;
    uint8_t *data = pinfo->outptr;

    dt_debug(TAG,"start decoding %d data bytes...\n", pinfo->inlen);
    
    sample_buffer = NeAACDecDecode(hDecoder, frameInfo,this->buffer, this->bytes_into_buffer);

    if (frameInfo->error > 0)
    {
        dt_error(TAG, "Error: %s\n",NeAACDecGetErrorMessage(frameInfo->error));
        return -1; // need to skip this frame
    }
    //decode ok, but no out, maybe need more data
    if ((frameInfo->error == 0) && (frameInfo->samples == 0))
        return 0;

    if(pinfo->outsize < frameInfo->samples *2)
    {
        pinfo->outptr = realloc(pinfo->outptr,frameInfo->samples *3);
        pinfo->outsize = frameInfo->samples *3;
    }

    //set default FAAD_FMT_16BIT in faad_dec_open 
    short *sample_buffer16 = (short *)sample_buffer;
    int i;
    for(i = 0; i < frameInfo->samples; i++)
    {
        data[i*2] = (uint8_t)(sample_buffer16[i] & 0xFF);
        data[i*2+1] = (uint8_t)((sample_buffer16[i]>>8) & 0xFF);
    }
    pinfo->outlen = frameInfo->samples * 2;
    
    return frameInfo->bytesconsumed;
}

static int faad_release (dec_audio_wrapper_t * wrapper)
{
    faad_ctx_t *this = (faad_ctx_t *)wrapper->adec_priv;
    if(!this)
        return 0;
    if(this->faad_dec)
        NeAACDecClose(this->faad_dec);
    free(this);
    wrapper->adec_priv = NULL;
    return 0;
}

dec_audio_wrapper_t adec_faad_ops = {
    .name = "faad audio decoder",
    .afmt = AUDIO_FORMAT_AAC,
    .type = DT_TYPE_AUDIO,
    .init = faad_init,
    .decode_frame = faad_decode,
    .release = faad_release,
};

#endif /*ENABLE_ADEC_FAAD*/
