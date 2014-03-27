#======================================================
#                   SETTING                      
#======================================================

#extlib
DT_FFMPEG = yes
DT_SDL = yes
DT_SDL2 = no
DT_ALSA = yes

#module
DT_STREAM=yes
DT_DEMUXER=yes
DT_UTIL=yes
DT_AUDIO=yes
DT_VIDEO=yes
DT_PORT=yes
DT_HOST=yes
DT_PLAYER=yes

#target
DTM_PLAYER=yes
DTM_INFO=
DTM_CONVERT=
DTM_SERVER=

#======================================================
#                   MACRO                      
#======================================================

#stream
DT_CFLAGS += -DENABLE_STREAM_FILE=1
ifeq ($(DT_FFMPEG),yes)
	DT_CFLAGS += -DENABLE_STREAM_FFMPEG=1
else
	DT_CFLAGS += -DENABLE_STREAM_FFMPEG=0
endif

#demuxer
DT_CFLAGS += -DENABLE_DEMUXER_AAC=1
ifeq ($(DT_FFMPEG),yes)
	DT_CFLAGS += -DENABLE_DEMUXER_FFMPEG=1
else
	DT_CFLAGS += -DENABLE_DEMUXER_FFMPEG=0
endif

#video 
DT_CFLAGS += -DENABLE_VDEC_NULL=0
ifeq ($(DT_FFMPEG),yes)
	DT_CFLAGS += -DENABLE_VDEC_FFMPEG=1
else
	DT_CFLAGS += -DENABLE_VDEC_FFMPEG=0
endif

DT_CFLAGS += -DENABLE_VO_NULL=0

ifeq ($(DT_SDL),yes)
	DT_CFLAGS += -DENABLE_VO_SDL=1
else
	DT_CFLAGS += -DENABLE_VO_SDL=0
endif

ifeq ($(DT_SDL2),yes)
	DT_CFLAGS += -DENABLE_VO_SDL2=1
else
	DT_CFLAGS += -DENABLE_VO_SDL2=0
endif

DT_CFLAGS += -DENABLE_VO_FB=0
DT_CFLAGS += -DENABLE_VO_OPENGL=0

#audio
DT_CFLAGS += -DENABLE_ADEC_NULL=0

ifeq ($(DT_FFMPEG),yes)
	DT_CFLAGS += -DENABLE_ADEC_FFMPEG=1
else
	DT_CFLAGS += -DENABLE_ADEC_FFMPEG=0
endif

DT_CFLAGS += -DENABLE_AO_NULL=0
DT_CFLAGS += -DENABLE_AO_OSS=0

ifeq ($(DT_SDL),yes)
	DT_CFLAGS += -DENABLE_AO_SDL=0
else
	DT_CFLAGS += -DENABLE_AO_SDL=0
endif

ifeq ($(DT_ALSA),yes)
	DT_CFLAGS += -DENABLE_AO_ALSA=1
else
	DT_CFLAGS += -DENABLE_AO_ALSA=0
endif

#======================================================
#                   FFMPEG                      
#======================================================

#DT_FFMPEG_DIR = ./ffmpeg 
FFMPEGPARTS_ALL = libavfilter libavformat libavcodec libswscale libswresample libavutil 
FFMPEGPARTS = $(foreach part, $(FFMPEGPARTS_ALL), $(if $(wildcard $(DT_FFMPEG_DIR)/$(part)), $(part)))
FFMPEGLIBS  = $(foreach part, $(FFMPEGPARTS), $(DT_FFMPEG_DIR)/$(part)/$(part).a)

DT_CFLAGS-$(DT_FFMPEG)   += -I$(DT_FFMPEG_DIR)
COMMON_LIBS-$(DT_FFMPEG) += $(FFMPEGLIBS)
