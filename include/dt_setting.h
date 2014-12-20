#ifndef DT_SETTING_H
#define DT_SETTING_H

typedef struct dt_setting{
    // LOG
    int log_level;
    // STREAM
    int stream_cache;
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
    int player_nosub;
    int player_sync_enable;
    int player_dump_mode;

    // HOST
    int host_drop;
    int host_drop_thres;
    int host_sync_thres;
}dt_setting_t;

extern dt_setting_t dtp_setting;

int dt_update_setting();

#endif
