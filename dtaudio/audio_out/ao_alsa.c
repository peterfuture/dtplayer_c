#if ENABLE_AO_ALSA

#include "../dtaudio_output.h"

#include <alsa/asoundlib.h>

#define TAG "AUDIO-OUT-ALSA"
#define ALSA_PCM_NEW_HW_PARAMS_API
#define DEFAULT_TIME_SIZE 3000	//3s

static int alsa_can_pause = 0;
static int default_buf_size_threshold = 0;	// default,alsa buffer size, 1s

static int ao_alsa_get_space (dtaudio_output_t * ao);

static snd_pcm_format_t format_to_alsa (int fmt)
{
	switch (fmt)
	{
	case 8:
		return SND_PCM_FORMAT_S8;
	case 16:
		return SND_PCM_FORMAT_S16_LE;
	case 24:
		return SND_PCM_FORMAT_S24_LE;
	case 32:
		return SND_PCM_FORMAT_S32_LE;
	default:
		return SND_PCM_FORMAT_S16_LE;
	}
}

static int ao_alsa_init (dtaudio_output_t * ao)
{
	snd_pcm_hw_params_t *alsa_hwparams = NULL;
	snd_pcm_sw_params_t *alsa_swparams = NULL;
	snd_pcm_uframes_t size, boundary;
	snd_pcm_t *alsa_handle;
	int err, afmt;
	unsigned int channels, sr;
	int buffersize, outburst;
	int bytes_per_sample;
	ao_operations_t *aout_ops = ao->aout_ops;
	afmt = format_to_alsa (ao->para.bps);
	channels = ao->para.channels;
	sr = ao->para.samplerate;
	dt_info (TAG, "[%s:%d] ao_alsa start init. channel:%d samplerate:%d afmt:%d \n", __FUNCTION__, __LINE__, ao->para.channels, ao->para.samplerate, ao->para.bps);
	err = snd_pcm_open (&alsa_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (err < 0)
	{
		dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
		return -1;
	}
	aout_ops->handle = (void *) alsa_handle;

	snd_pcm_hw_params_alloca (&alsa_hwparams);
	snd_pcm_sw_params_alloca (&alsa_swparams);

	err = snd_pcm_hw_params_any (alsa_handle, alsa_hwparams);
	if (err < 0)
	{
		dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
		return -1;
	}
	err = snd_pcm_hw_params_set_access (alsa_handle, alsa_hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0)
	{
		dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
		return -1;
	}
	err = snd_pcm_hw_params_set_format (alsa_handle, alsa_hwparams, afmt);
	if (err < 0)
	{
		dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
		return -1;
	}
	err = snd_pcm_hw_params_set_channels_near (alsa_handle, alsa_hwparams, &channels);
	if (err < 0)
	{
		dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
		return -1;
	}

	err = snd_pcm_hw_params_set_rate_near (alsa_handle, alsa_hwparams, &sr, NULL);
	if (err < 0)
	{
		dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
		return -1;
	}

	bytes_per_sample = snd_pcm_format_physical_width (afmt) / 8;
	bytes_per_sample *= channels;
	err = snd_pcm_hw_params (alsa_handle, alsa_hwparams);
	if (err < 0)
	{
		dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
		return -1;
	}
	/******************************************************************
	 *  Set HW Params Finish
	******************************************************************/

	err = snd_pcm_hw_params_get_buffer_size (alsa_hwparams, &size);
	if (err < 0)
	{
		dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
		return -1;
	}
	buffersize = size * bytes_per_sample;

	err = snd_pcm_hw_params_get_period_size (alsa_hwparams, &size, NULL);
	if (err < 0)
	{
		dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
		return -1;
	}
	outburst = size * bytes_per_sample;

	dt_info (TAG, "outburst: %d, bytes_per_sample: %d, buffersize: %d\n", outburst, bytes_per_sample, buffersize);

	/******************************************************************
	* set sw params
	******************************************************************/
	err = snd_pcm_sw_params_current (alsa_handle, alsa_swparams);
	if (err < 0)
	{
		dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
		return -1;
	}
#if SND_LIB_VERSION >= 0x000901
	err = snd_pcm_sw_params_get_boundary (alsa_swparams, &boundary);
#else
	boundary = 0x7fffffff;
#endif
	err = snd_pcm_sw_params_set_start_threshold (alsa_handle, alsa_swparams, size);
	if (err < 0)
	{
		dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
		return -1;
	}
	err = snd_pcm_sw_params_set_stop_threshold (alsa_handle, alsa_swparams, boundary);
	if (err < 0)
	{
		dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
		return -1;
	}
#if SND_LIB_VERSION >= 0x000901
	err = snd_pcm_sw_params_set_silence_size (alsa_handle, alsa_swparams, boundary);
	if (err < 0)
	{
		dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
		return -1;
	}
#endif
	err = snd_pcm_sw_params (alsa_handle, alsa_swparams);
	if (err < 0)
	{
		dt_error (TAG, "%s,%d: %s\n", __func__, __LINE__, snd_strerror (err));
		return -1;
	}

	alsa_can_pause = snd_pcm_hw_params_can_pause (alsa_hwparams);
	dt_info (TAG, "ALSA PAUSE SUPPORT:%d RESUME_SUPPORT:%d \n", alsa_can_pause, snd_pcm_hw_params_can_resume (alsa_hwparams));
	outburst = size * bytes_per_sample;
	ao->state.aout_buf_size = buffersize;
	ao->state.aout_buf_level = 0;

	//buf size limit
	default_buf_size_threshold = bytes_per_sample * sr * DEFAULT_TIME_SIZE / 1000;

	dt_info (TAG, "alsa audio init ok! outburst:%d thres:%d \n", outburst, default_buf_size_threshold);

	return 0;
}

static int ao_alsa_stop (dtaudio_output_t * ao)
{
	ao_operations_t *aout_ops = ao->aout_ops;
	snd_pcm_t *alsa_handle = (snd_pcm_t *) aout_ops->handle;
	if (alsa_handle)
	{
		snd_pcm_drop (alsa_handle);
		snd_pcm_drain (alsa_handle);
		snd_pcm_close (alsa_handle);
	}
	return 0;
}

static int ao_alsa_get_level (dtaudio_output_t * ao)
{
	return ao->state.aout_buf_size - ao_alsa_get_space (ao);
}

#define ALSA_RESERVE_THRESHOLD 10*1024
static int ao_alsa_play (dtaudio_output_t * ao, uint8_t * buf, int size)
{
	ao_operations_t *aout_ops = ao->aout_ops;
	snd_pcm_t *alsa_handle = (snd_pcm_t *) aout_ops->handle;
	int bytes_per_sample = ao->para.bps * ao->para.channels / 8;
	int num_frames = size / bytes_per_sample;
	snd_pcm_sframes_t res = 0;
	uint8_t *data = buf;

	if (!alsa_handle)
		return -1;

	if (num_frames == 0)
		return 0;

	if (ao_alsa_get_level (ao) >= default_buf_size_threshold)
	{
		dt_debug (TAG, "ALSA EXCEED THRESHOLD,size:%d  thres:%d \n", ao_alsa_get_level (ao), default_buf_size_threshold);
		return -1;
	}
	//alsa need reserve some space
#if 0
	int reserve = ao_alsa_get_space (ao);
	if (reserve <= ALSA_RESERVE_THRESHOLD)
	{
		dt_debug (TAG, "ALSA NO SPACE LEFT \n");
		return -1;
	}
#endif
	res = snd_pcm_writei (alsa_handle, data, num_frames);
	if (res == -EINTR)
	{
		dt_info (TAG, "ALSA HAS NO SPACE, WRITE AGAIN\n");
		return res;
	}
	if (res == -ESTRPIPE)
	{
		snd_pcm_resume (alsa_handle);
		return res;
	}
	if (res == -EBADFD)
	{
		snd_pcm_reset (alsa_handle);
		return res;
	}
	if (res < 0)
	{
		snd_pcm_prepare (alsa_handle);
		dt_info (TAG, "snd pcm write failed prepare!\n");
		return -1;
		//goto rewrite;
	}
	if (res < num_frames)
	{
		data += res * bytes_per_sample;
		num_frames -= res;
		//goto rewrite;
	}
	return res * bytes_per_sample;
}

static int ao_alsa_get_space (dtaudio_output_t * ao)
{
	ao_operations_t *aout_ops = ao->aout_ops;
	snd_pcm_t *alsa_handle = (snd_pcm_t *) aout_ops->handle;
	int ret;
	snd_pcm_status_t *status;
	int bytes_per_sample = ao->para.bps * ao->para.channels / 8;
	snd_pcm_status_alloca (&status);

	if ((ret = snd_pcm_status (alsa_handle, status)) < 0)
	{
		printf ("%s,%d: %s\n", __FUNCTION__, __LINE__, snd_strerror (ret));
		return 0;
	}
	ret = snd_pcm_status_get_avail (status) * bytes_per_sample;
	//ret = snd_pcm_avail_update(alsa_handle) * bytes_per_sample;
	if (ret > ao->state.aout_buf_size)
	{
		//printf("==alsa underrun==ret:%d \n",ret);
		ret = ao->state.aout_buf_size;
	}
	//printf("[%s:%d] alsa space :%d \n",__FUNCTION__,__LINE__,ret);
	return ret;
}

static int ao_alsa_pause (dtaudio_output_t * ao)
{
	snd_pcm_t *handle = (snd_pcm_t *) ao->aout_ops->handle;
	int ret = 0;
	if (alsa_can_pause == 1)
	{
		ret = snd_pcm_pause (handle, 1);
		dt_info (TAG, "ALSA PAUSED SUPPORT PAUSE DIRECTLY  ret:%d status:%d\n", ret, snd_pcm_state (handle));
	}
	else
		snd_pcm_drop (ao->aout_ops->handle);
	dt_info (TAG, "ALSA PAUSED \n");
	return 0;
}

static int ao_alsa_resume (dtaudio_output_t * ao)
{
	snd_pcm_t *handle = (snd_pcm_t *) ao->aout_ops->handle;
	int ret = 0;
	if (alsa_can_pause == 1)
	{
		ret = snd_pcm_pause (handle, 0);
		dt_info (TAG, "ALSA PAUSED SUPPORT RESUME DIRECTLY  ret:%d status:%d \n", ret, snd_pcm_state (handle));
	}
	else
		snd_pcm_prepare (handle);
	dt_info (TAG, "ALSA RESUME \n");
	return 0;
}

static int64_t ao_alsa_get_latency (dtaudio_output_t * ao)
{
	unsigned int buffered_data;
	unsigned int sample_num;
	uint64_t latency, latency_s;
	float pts_ratio = 0.0;
	pts_ratio = (double) 90000 / ao->para.samplerate;
	//ao->state.aout_buf_level=ao_alsa_get_space(ao);
	ao->state.aout_buf_level = ao->state.aout_buf_size - ao_alsa_get_space (ao);
	buffered_data = (ao->state.aout_buf_level);
	sample_num = buffered_data / (ao->para.channels * ao->para.bps / 8);
	latency = (sample_num * pts_ratio);
	latency_s = sample_num / ao->para.samplerate;

	dt_debug (TAG, "[%s:%d]==alsa_level:%d thres:%d sample_num:%d buffersize:%d latency:%llu latency_s:%llu \n", __FUNCTION__, __LINE__, buffered_data, default_buf_size_threshold, sample_num, ao->state.aout_buf_size, latency, latency_s);
	return latency;
}

ao_operations_t ao_alsa_ops = {
	.id = AO_ID_ALSA,
	.name = "alsa",
	.ao_init = ao_alsa_init,
	.ao_pause = ao_alsa_pause,
	.ao_resume = ao_alsa_resume,
	.ao_stop = ao_alsa_stop,
	.ao_write = ao_alsa_play,
	.ao_latency = ao_alsa_get_latency,
	.ao_level = ao_alsa_get_level,
};
#endif /* ENABLE_AO_ALSA */
