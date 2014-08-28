#include "render.h"

#include "dtaudio.h"
#include "dtaudio_output.h"
#include "dtvideo.h"
#include "dtvideo_output.h"

/*---------------------------------------------------------- */
#ifdef ENABLE_VO_SDL2
extern vo_wrapper_t vo_sdl2_ops;
#endif

vo_wrapper_t vo_ex_ops;

static int vo_ex_init (dtvideo_output_t *vout)
{
    int ret = 0;
#ifdef ENABLE_VO_SDL2 
    ret = vo_sdl2_ops.vo_init(vout);  
#endif
    return ret; 
}

static int vo_ex_render (dtvideo_output_t *vout,AVPicture_t * pict)
{
    int ret = 0;
#ifdef ENABLE_VO_SDL2
    ret = vo_sdl2_ops.vo_render(vout,pict);
#endif
    return ret;
}

static int vo_ex_stop (dtvideo_output_t *vout)
{
    int ret = 0;
#ifdef ENABLE_VO_SDL2
    ret = vo_sdl2_ops.vo_stop(vout);
#endif
    return ret;
}

vo_wrapper_t vo_ex_ops = {
    .id = VO_ID_EX,
    .name = "ex vo",
    .vo_init = vo_ex_init,
    .vo_stop = vo_ex_stop,
    .vo_render = vo_ex_render,
};



/*---------------------------------------------------------- */
#ifdef ENABLE_VO_SDL2 
extern ao_wrapper_t ao_sdl2_ops;
#endif

ao_wrapper_t ao_ex_ops;
ao_wrapper_t *ao_wrapper = &ao_ex_ops;
static int ao_ex_init (dtaudio_output_t *aout, dtaudio_para_t *para)
{
    int ret = 0;
#ifdef ENABLE_VO_SDL2 
    ret = ao_sdl2_ops.ao_init(aout,para);
#endif
    return ret;
}

static int ao_ex_play (dtaudio_output_t *aout, uint8_t * buf, int size)
{
    int ret = 0;
#ifdef ENABLE_VO_SDL2 
    ret = ao_sdl2_ops.ao_write(aout,buf,size);
#endif
    return ret;
}

static int ao_ex_pause (dtaudio_output_t *aout)
{
    int ret = 0;
#ifdef ENABLE_VO_SDL2 
    ret = ao_sdl2_ops.ao_pause(aout);
#endif
    return ret;
}

static int ao_ex_resume (dtaudio_output_t *aout)
{
    int ret = 0;
#ifdef ENABLE_VO_SDL2 
    ret = ao_sdl2_ops.ao_resume(aout);
#endif
    return ret;
}

static int ao_ex_level(dtaudio_output_t *aout)
{
    int ret = 0;
#ifdef ENABLE_VO_SDL2 
    ret = ao_sdl2_ops.ao_level(aout);
#endif
    return ret;
}

static int64_t ao_ex_get_latency (dtaudio_output_t *aout)
{
    int ret = 0;
#ifdef ENABLE_VO_SDL2 
    ret = ao_sdl2_ops.ao_latency(aout);
#endif
    return ret;
}

static int ao_ex_stop (dtaudio_output_t *aout)
{
    int ret = 0;
#ifdef ENABLE_VO_SDL2 
    ret = ao_sdl2_ops.ao_stop(aout);
#endif
    return ret;
}

ao_wrapper_t ao_ex_ops = {
    .id = AO_ID_EX,
    .name = "ex ao",
    .ao_init = ao_ex_init,
    .ao_pause = ao_ex_pause,
    .ao_resume = ao_ex_resume,
    .ao_stop = ao_ex_stop,
    .ao_write = ao_ex_play,
    .ao_level = ao_ex_level,
    .ao_latency = ao_ex_get_latency,
};


int render_init()
{
    dtplayer_register_ext_ao(&ao_ex_ops);
    dtplayer_register_ext_vo(&vo_ex_ops); 

    return  0;
}

int render_stop()
{
    return 0;
}


