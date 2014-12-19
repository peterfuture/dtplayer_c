#ifndef DT_SETTING_H
#define DT_SETTING_H

typedef struct dt_setting{
    // LOG
    int log_level;
    // STREAM
    int stream_cache_enable;
    int stream_cache_size;
    // DEMUXER
    int demuxer_probe;
    int demuxer_probe_size;
    // AUDIO
    int audio_downmix;
    //VIDEO
    int video_pts_mode;
    int video_out_type;
    // PLAYER
    int player_noaudio;
    int player_novideo;
    int player_threshold;
    int dump_mode;
    int drop_enable;
}dt_setting_t;

extern dt_setting_t dtp_setting;

#endif
