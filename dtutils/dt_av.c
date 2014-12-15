#include "dt_av.h"

void *dt_malloc(size_t size)
{
    void *ptr = malloc(size);
    if(!ptr) return ptr;
    memset(ptr, 0, size);
    return ptr;
}

void dt_free(void *ptr)
{
    free(ptr);
}

dt_av_frame_t *dtav_new_frame()
{
    dt_av_frame_t *frame = (dt_av_frame_t *)malloc(sizeof(dt_av_frame_t));
    return frame;
}

//clean data
int dtav_unref_frame(dt_av_frame_t *frame)
{
    if(!frame)
        return 0;
    if (frame->data)
        free (frame->data[0]);
    return 0;
}

int dtav_free_frame(dt_av_frame_t *frame)
{
    if(!frame)
        return 0;
    if (frame->data)
        free (frame->data[0]);
    free(frame);
    return 0;
}
