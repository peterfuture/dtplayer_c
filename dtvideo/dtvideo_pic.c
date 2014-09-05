#include "dt_av.h"
dt_av_pic_t *dtav_new_pic()
{
    dt_av_pic_t *pic = (dt_av_pic_t *)malloc(sizeof(dt_av_pic_t));
    return pic;
}

//clean data
int dtav_unref_pic(dt_av_pic_t *pic)
{
    if(!pic)
        return 0;
    if (pic->data)
        free (pic->data[0]);
    return 0;
}

int dtav_free_pic(dt_av_pic_t *pic)
{
    if(!pic)
        return 0;
    if (pic->data)
        free (pic->data[0]);
    free(pic);
    return 0;
}
