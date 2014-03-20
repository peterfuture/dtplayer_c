#ifndef DTAUDIO_FILTER_H
#define DTAUDIO_FILTER_H
typedef struct
{
    int filter;
} dtaudio_filter_t;

int audio_filter_init ();
int audio_filter_start ();
int audio_filter_stop ();
int audio_filter_release ();
#endif
