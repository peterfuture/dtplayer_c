#======================================================
#                   MACRO                      
#======================================================

#stream
DT_CFLAGS += -DENABLE_STREAM_FILE=1
DT_CFLAGS += -DENABLE_STREAM_FFMPEG=1
#demuxer
DT_CFLAGS += -DENABLE_DEMUXER_AAC=1
DT_CFLAGS += -DENABLE_DEMUXER_FFMPEG=1

#video 
DT_CFLAGS += -DENABLE_VDEC_NULL=0
DT_CFLAGS += -DENABLE_VDEC_FFMPEG=1

DT_CFLAGS += -DENABLE_VO_NULL=0
DT_CFLAGS += -DENABLE_VO_SDL=1
DT_CFLAGS += -DENABLE_VO_SDL2=0
DT_CFLAGS += -DENABLE_VO_FB=0
DT_CFLAGS += -DENABLE_VO_OPENGL=0

#audio
DT_CFLAGS += -DENABLE_ADEC_NULL=0
DT_CFLAGS += -DENABLE_ADEC_FFMPEG=1

DT_CFLAGS += -DENABLE_AO_NULL=0
DT_CFLAGS += -DENABLE_AO_SDL=0
DT_CFLAGS += -DENABLE_AO_ALSA=1
DT_CFLAGS += -DENABLE_AO_OSS=0

#======================================================
#                   SETTING                      
#======================================================

#module
DT_STREAM=yes
DT_STREAM_FILE=yes
DT_STREAM_FFMPEG=yes
DT_DEMUXER=yes
DT_DEMUXER_AAC=yes
DT_DEMUXER_FFMPEG=yes
DT_UTIL=yes
DT_AUDIO=yes
DT_AUDIO_FFMPEG=yes
DT_AUDIO_ALSA=yes
DT_VIDEO=yes
DT_VIDEO_FFMPEG=yes
DT_VIDEO_SDL=yes
DT_VIDEO_SDL2=no
DT_PORT=yes
DT_HOST=yes
DT_PLAYER=yes

#target
DTM_PLAYER=yes
DTM_INFO=
DTM_CONVERT=
DTM_SERVER=


#======================================================
#                   FFMPEG                      
#======================================================

#DT_FFMPEG_DIR = ./ffmpeg 
FFMPEG_A = $(DT_DEMUXER_FFMPEG)
FFMPEGPARTS_ALL = libavfilter libavformat libavcodec libswscale libswresample libavutil 
FFMPEGPARTS = $(foreach part, $(FFMPEGPARTS_ALL), $(if $(wildcard $(DT_FFMPEG_DIR)/$(part)), $(part)))
FFMPEGLIBS  = $(foreach part, $(FFMPEGPARTS), $(DT_FFMPEG_DIR)/$(part)/$(part).a)

DT_CFLAGS-$(FFMPEG_A)   += -I$(DT_FFMPEG_DIR)
COMMON_LIBS-$(FFMPEG_A) += $(FFMPEGLIBS)
