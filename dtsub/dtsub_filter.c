/***********************************************************************
**
**  Module : dtsub_filter.c
**  Summary: sub pp wrapper
**  Section: dtsub
**  Author : peter
**  Notes  :
**
***********************************************************************/

#include "dtsub.h"
#include "dtsub_filter.h"

#define TAG "SUB-FILTER"

#define REGISTER_SF(X,x)        \
    {                           \
        extern sf_wrapper_t sf_##x##_ops;   \
        register_sf(&sf_##x##_ops);     \
    }

static sf_wrapper_t *g_sf = NULL;


/***********************************************************************
**
** register internal filter
**
***********************************************************************/
static void register_sf(sf_wrapper_t * sf)
{
    sf_wrapper_t **p;
    p = &g_sf;
    while (*p != NULL) {
        p = &(*p)->next;
    }
    *p = sf;
    dt_info(TAG, "[%s:%d] register internal sf, name:%s \n", __FUNCTION__, __LINE__,
            (*p)->name);
    sf->next = NULL;
    return;
}

/***********************************************************************
**
** register external filter
**
***********************************************************************/
void sf_register_ext(sf_wrapper_t *sf)
{
    sf_wrapper_t **p;
    p = &g_sf;
    if (*p == NULL) {
        *p = sf;
        sf->next = NULL;
    } else {
        sf->next = *p;
        *p = sf;
    }
    dt_info(TAG, "[%s:%d]register external sf. name:%s \n", __FUNCTION__, __LINE__,
            sf->name);
    return;
}

/***********************************************************************
**
** register all internal filters
**
***********************************************************************/
void sf_register_all()
{
    return;
}

/***********************************************************************
**
** remove all internal filters
**
***********************************************************************/
void sf_remove_all()
{
    g_sf = NULL;
    return;
}

/***********************************************************************
**
** select first sf with capbilities cap
** cap: sf capbilities set
**
***********************************************************************/
static int select_sf(dtsub_filter_t *filter, sf_cap_t cap)
{
    int ret = -1;
    sf_wrapper_t *sf = g_sf;

    if (!sf) {
        return ret;
    }

    while (sf != NULL) {
        if (sf->capable(cap) == cap) { // maybe cap have several elements
            ret = 0;
            break;
        }
        sf = sf->next;
    }

    filter->wrapper = sf;
    if (sf) {
        dt_info(TAG, "[%s:%d] %s sub filter selected \n", __FUNCTION__, __LINE__,
                g_sf->name);
    } else {
        dt_info(TAG, "[%s:%d] No sub filter selected \n", __FUNCTION__, __LINE__);
    }
    return ret;
}

/***********************************************************************
**
** Init sub filter
**
***********************************************************************/
int sub_filter_init(dtsub_filter_t *filter)
{
    return 0;
}

/***********************************************************************
**
** update sub filter
** reset para in filter first
**
***********************************************************************/
int sub_filter_update(dtsub_filter_t *filter)
{
    return 0;
}

/***********************************************************************
**
** process one frame
**
***********************************************************************/
int sub_filter_process(dtsub_filter_t *filter, dt_av_frame_t *frame)
{
    return 0;
}

/***********************************************************************
**
** stop filter
**
***********************************************************************/
int sub_filter_stop(dtsub_filter_t *filter)
{
    return 0;
}
