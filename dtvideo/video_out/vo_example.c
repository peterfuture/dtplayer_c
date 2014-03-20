/**********************************************
 * Dawn Light Player
 *
 *   vo_example.c
 *
 * Created by kf701
 * 14:11:50 02/26/08 CST
 *
 * $Id: vo_example.c 45 2008-04-11 06:27:45Z kf701 $
 **********************************************/

#if ENABLE_VO_NULL

#include "avcodec.h"
#include "avoutput.h"
#include "global.h"

static pthread_mutex_t vo_mutex;

static void vo_lock_init ()
{
    pthread_mutex_init (&vo_mutex, NULL);
}

static void vo_lock_free ()
{
    pthread_mutex_destroy (&vo_mutex);
}

static void vo_lock ()
{
    pthread_mutex_lock (&vo_mutex);
}

static void vo_unlock ()
{
    pthread_mutex_unlock (&vo_mutex);
}

static int vo_example_init (void)
{
    dlpctxp->novideo = 1;
    /* ... */
    vo_lock_init ();
    return 0;
}

static int vo_example_uninit (void)
{
    vo_lock ();
    /* ... */
    vo_lock_free ();
    return 0;
}

static void vo_example_display (AVPicture * pict)
{
    vo_lock ();
    /* ... */
    vo_unlock ();
    return;
}

static void vo_example_event_loop (void)
{
    while (1)
        sleep (10);
}

static int vo_example_control (int cmd, void *arg)
{
    switch (cmd)
    {
    default:
        av_log (NULL, AV_LOG_ERROR, "VO NULL not support control now!\n");
        break;
    }
    return 0;
}

vo_t vo_null = {
    VO_ID_EXAMPLE,
    "null",
    vo_example_init,
    vo_example_uninit,
    vo_example_display,
    vo_example_control,
};

#endif
