#include "render.h"

#include "dtaudio.h"
#include "dtaudio_output.h"
#include "dtvideo.h"
#include "dtvideo_output.h"

/*---------------------------------------------------------- */

extern vo_wrapper_t vo_sdl2_ops;

static int vo_ex_init (vo_wrapper_t *wrapper, void *parent)
{
    wrapper->parent = parent;
    return vo_sdl2_ops.vo_init(&vo_sdl2_ops,parent);  
}

static int vo_ex_render (vo_wrapper_t *wrapper, AVPicture_t * pict)
{
    return vo_sdl2_ops.vo_render(&vo_sdl2_ops,pict);  
}

static int vo_ex_stop (vo_wrapper_t *wrapper)
{
    return vo_sdl2_ops.vo_stop(&vo_sdl2_ops);
}

vo_wrapper_t vo_ex_ops = {
    .id = VO_ID_EX,
    .name = "ex vo",
    .vo_init = vo_ex_init,
    .vo_stop = vo_ex_stop,
    .vo_render = vo_ex_render,
};



/*---------------------------------------------------------- */

extern ao_wrapper_t ao_sdl2_ops;

static int ao_ex_init (ao_wrapper_t *wrapper, void *parent)
{
    return ao_sdl2_ops.ao_init(&ao_sdl2_ops,parent);
}

static int ao_ex_play (ao_wrapper_t *wrapper, uint8_t * buf, int size)
{
    return ao_sdl2_ops.ao_write(&ao_sdl2_ops,buf,size);
}

static int ao_ex_pause (ao_wrapper_t *wrapper)
{
    return ao_sdl2_ops.ao_pause(&ao_sdl2_ops);
}

static int ao_ex_resume (ao_wrapper_t *wrapper)
{
    return ao_sdl2_ops.ao_resume(&ao_sdl2_ops);
}

static int ao_ex_level(ao_wrapper_t *wrapper)
{
    return ao_sdl2_ops.ao_level(&ao_sdl2_ops);
}

static int64_t ao_ex_get_latency (ao_wrapper_t *wrapper)
{
    return ao_sdl2_ops.ao_latency(&ao_sdl2_ops);
}

static int ao_ex_stop (ao_wrapper_t * wrapper)
{
    return ao_sdl2_ops.ao_stop(&ao_sdl2_ops);
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
    register_ext_ao(&ao_ex_ops);
    register_ext_vo(&vo_ex_ops); 

    return  0;
}

int render_stop()
{
    return 0;
}


