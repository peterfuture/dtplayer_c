#if ENABLE_ADEC_FAAD

#include "../dtaudio_decoder.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define TAG "ADEC-FAAD"

#define FAAD_MIN_STREAMSIZE 768 /* 6144 bits/channel */
#define FAAD_MAX_CHANNELS 6

typedef struct faad_decoder_s {
  /* faad2 stuff */
  NeAACDecHandle           faac_dec;
  NeAACDecConfigurationPtr faac_cfg;
  NeAACDecFrameInfo        faac_finfo;
  int                     faac_failed;
  
  int              raw_mode;

  unsigned char   *buf;
  int              size;
  int              rec_audio_src_size;
  int              max_audio_src_size;
  int64_t          pts;

  unsigned char   *dec_config;
  int              dec_config_size;

  unsigned long    rate;
  int              bits_per_sample;
  unsigned char    num_channels;
  int              sbr;

  int              output_open;

  unsigned long    total_time;
  unsigned long    total_data;
} faad_decoder_t;

static int faad_init (dec_audio_wrapper_t *wrapper,void *parent)
{
    wrapper->parent = parent;
    dtaudio_decoder_t *decoder = (dtaudio_decoder_t *)parent;
    
    faad_decoder_t *this = (faad_decoder_t *)malloc(sizeof(faad_decoder_t));
    if(!this)
        return -1;
    memset(this,0,sizeof(*this));
#if 0
    this->buf = malloc(FAAD_MAX_CHANNELS * FAAD_MIN_STREAMSIZE);
    if(!this->buf)
    {
        free(this);
        return -1;
    }
#endif
    wrapper->adec_priv = this;
    
    return 0;
}


static int faad_open_dec( faad_decoder_t *this ) {
  int used;

  this->faac_dec = NeAACDecOpen();
  if( !this->faac_dec ) {
        dt_error(TAG,"libfaad: libfaad NeAACDecOpen() failed.\n");
    this->faac_failed++;
  } else {
    if( this->dec_config ) {
      used = NeAACDecInit2(this->faac_dec, this->dec_config, this->dec_config_size,
                          &this->rate, &this->num_channels);

      if( used < 0 ) {
        dt_error(TAG,"libfaad: libfaad NeAACDecInit2 failed.\n");
        this->faac_failed++;
      } else
        dt_info(TAG, "NeAACDecInit2 returned rate=%"PRId32" channels=%d\n",this->rate, this->num_channels );
    } else {
      used = NeAACDecInit(this->faac_dec, this->buf, this->size,
                        &this->rate, &this->num_channels);

      if( used < 0 ) {
        dt_error(TAG,"libfaad: libfaad NeAACDecInit failed.\n");
        this->faac_failed++;
      } else {
        dt_info(TAG,"NeAACDecInit() returned rate=%"PRId32" channels=%d (used=%d)\n",this->rate, this->num_channels, used);

        this->size -= used;
        //memmove( this->buf, &this->buf[used], this->size );
      }
    }
  }

  if( !this->bits_per_sample )
    this->bits_per_sample = 16;

  if( this->faac_failed ) {
    if( this->faac_dec ) {
      NeAACDecClose( this->faac_dec );
      this->faac_dec = NULL;
    }
  }
  return this->faac_failed;
}

static void faad_decode_audio ( faad_decoder_t *this) {
  int used, decoded, outsize;
  uint8_t *sample_buffer;
  uint8_t *inbuf;
  int sample_size = this->size;

  if( !this->faac_dec )
    return;

  inbuf = this->buf;
  if(this->size < 10)
      return;
    
  sample_buffer = NeAACDecDecode(this->faac_dec,&this->faac_finfo, inbuf, sample_size);

    if( !sample_buffer ) {
        dt_error(TAG,"libfaad: %s\n", NeAACDecGetErrorMessage(this->faac_finfo.error));
      used = 1;
    } else {
      used = this->faac_finfo.bytesconsumed;

      /* raw AAC parameters might only be known after decoding the first frame */
      if( !this->dec_config &&
          (this->num_channels != this->faac_finfo.channels ||
           this->rate != this->faac_finfo.samplerate) ) {

        this->num_channels = this->faac_finfo.channels;
        this->rate = this->faac_finfo.samplerate;

        dtinfo(TAG,"NeAACDecDecode() returned rate=%"PRId32" channels=%d used=%d\n",
                this->rate, this->num_channels, used);
      }

      /* faad doesn't tell us about sbr until after the first frame */
      if (this->sbr != this->faac_finfo.sbr) {
        this->sbr = this->faac_finfo.sbr;
      }

      /* estimate bitrate */
      this->total_time += (1000*this->faac_finfo.samples/(this->rate*this->num_channels));
      this->total_data += 8*used;

      if ((this->total_time > LONG_MAX) || (this->total_data > LONG_MAX)) {
        this->total_time >>= 2;
        this->total_data >>= 2;
      }

      if (this->total_time)

      decoded = this->faac_finfo.samples * 2; /* 1 sample = 2 bytes */

      dt_info(TAG,"decoded %d/%d output %ld\n",used, this->size, this->faac_finfo.samples );

      /* Performing necessary channel reordering because aac uses a different
       * layout than alsa:
       *
       *  aac 5.1 channel layout: c l r ls rs lfe
       * alsa 5.1 channel layout: l r ls rs c lfe
       *
       * Reordering is only necessary for 5.0 and above. Currently only 5.0
       * and 5.1 is being taken care of, the rest will stay in the wrong order
       * for now.
       *
       * WARNING: the following needs a output format of 16 bits per sample.
       *    TODO: - reorder while copying (in the while() loop) and optimizing
       */
      if(this->num_channels == 5 || this->num_channels == 6)
      {
        int i         = 0;
        uint16_t* buf = (uint16_t*)(sample_buffer);

        for(; i < this->faac_finfo.samples; i += this->num_channels) {
          uint16_t center         = buf[i];
          *((uint64_t*)(buf + i)) = *((uint64_t*)(buf + i + 1));
          buf[i + 4]              = center;
        }
      }

      while( decoded ) {
        audio_buffer = this->stream->audio_out->get_buffer (this->stream->audio_out);

        if( decoded < audio_buffer->mem_size )
          outsize = decoded;
        else
          outsize = audio_buffer->mem_size;

        xine_fast_memcpy( audio_buffer->mem, sample_buffer, outsize );

        audio_buffer->num_frames = outsize / (this->num_channels*2);
        audio_buffer->vpts = this->pts;

        this->stream->audio_out->put_buffer (this->stream->audio_out, audio_buffer, this->stream);

        this->pts = 0;
        decoded -= outsize;
        sample_buffer += outsize;
      }
    }

    if(used >= this->size){
      this->size = 0;
    } else {
      this->size -= used;
      inbuf += used;
    }

    if( !this->raw_mode )
      this->size = 0;
  }

  if( this->size )
    memmove( this->buf, inbuf, this->size);

}

static int faad_decode (dec_audio_wrapper_t *wrapper, uint8_t * inbuf, int *inlen, uint8_t * outbuf, int *outlen)
{
 
  faad_decoder_t *this = (faad_decoder_t *) wrapper->adec_priv;

    this->buf = inbuf;
    this->size = *inlen;
  /* store config information from ESDS mp4/qt atom */
  if( !this->faac_dec) {
    if( faad_open_dec(this) )
      return 0; // maybe need more data
    
    this->raw_mode = 0;
  }
    
    dt_info(TAG,"decoding %d data bytes...\n", this->size);

    if( this->size <= 0)
      return *inlen;

    faad_decode_audio(this);
   
    return (*inlen - this->size);
}

static int faad_release (dec_audio_wrapper_t * wrapper)
{
  faad_decoder_t *this = (faad_decoder_t *) wrapper->adec_priv;

  if( this->buf )
    free(this->buf);
  this->buf = NULL;
  this->size = 0;
  this->max_audio_src_size = 0;

  if( this->dec_config )
    free(this->dec_config);
  this->dec_config = NULL;
  this->dec_config_size = 0;

  if( this->faac_dec )
    NeAACDecClose(this->faac_dec);
  this->faac_dec = NULL;
  this->faac_failed = 0;

  free (this);

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

#endif /*ENABLE_ADEC_FFMPEG*/
