#======================================================
#                   VERSION                      
#======================================================

DT_VERSION = v1.0

#======================================================
#                   SETTING                      
#======================================================

#extlib
DT_FFMPEG = yes
DT_SDL = no
DT_SDL2 = yes
DT_ALSA = yes
DT_FAAD = no
DT_TSDEMUX = no

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
endif

#demuxer
DT_CFLAGS += -DENABLE_DEMUXER_AAC=1

ifeq ($(DT_TSDEMUX),yes)
	DT_CFLAGS += -DENABLE_DEMUXER_TS=1
endif

ifeq ($(DT_FFMPEG),yes)
	DT_CFLAGS += -DENABLE_DEMUXER_FFMPEG=1
endif

#video 
DT_CFLAGS += -DENABLE_VDEC_NULL=0
ifeq ($(DT_FFMPEG),yes)
	DT_CFLAGS += -DENABLE_VDEC_FFMPEG=1
endif

#DT_CFLAGS += -DENABLE_VO_NULL=0

ifeq ($(DT_SDL),yes)
	DT_CFLAGS += -DENABLE_VO_SDL=1
endif

ifeq ($(DT_SDL2),yes)
	DT_CFLAGS += -DENABLE_VO_SDL2=1
endif

#DT_CFLAGS += -DENABLE_VO_FB=0
#DT_CFLAGS += -DENABLE_VO_OPENGL=0

#audio
#DT_CFLAGS += -DENABLE_ADEC_NULL=0

ifeq ($(DT_FAAD),yes)
	DT_CFLAGS += -DENABLE_ADEC_FAAD=1
endif

ifeq ($(DT_FFMPEG),yes)
	DT_CFLAGS += -DENABLE_ADEC_FFMPEG=1
endif

#DT_CFLAGS += -DENABLE_AO_NULL=0
#DT_CFLAGS += -DENABLE_AO_OSS=0

ifeq ($(DT_SDL),yes)
	DT_CFLAGS += -DENABLE_AO_SDL=1
endif

ifeq ($(DT_SDL2),yes)
	DT_CFLAGS += -DENABLE_AO_SDL2=1
endif

ifeq ($(DT_ALSA),yes)
	DT_CFLAGS += -DENABLE_AO_ALSA=1
endif

#======================================================
#                   FFMPEG                      
#======================================================

ifeq ($(DT_FFMPEG_DIR),)
	DT_FFMPEG_DIR = /usr/local/#default ffmpeg install dir 
endif

FFMPEGPARTS_ALL = libavfilter libavformat libavcodec libswscale libswresample libavutil 
FFMPEGPARTS = $(foreach part, $(FFMPEGPARTS_ALL), $(if $(wildcard $(DT_FFMPEG_DIR)/lib), $(part)))
FFMPEGLIBS  = $(foreach part, $(FFMPEGPARTS), $(DT_FFMPEG_DIR)/lib/$(part).so)

DT_CFLAGS-$(DT_FFMPEG)   += -I$(DT_FFMPEG_DIR)/include
COMMON_LIBS-$(DT_FFMPEG) += $(FFMPEGLIBS)

#======================================================
#                   EXT LIB                      
#======================================================

LDFLAGS-$(DT_SDL)  += -lSDL
LDFLAGS-$(DT_SDL2) += -lSDL2 -Wl,-rpath=/usr/local/lib
LDFLAGS-$(DT_ALSA) += -lasound
LDFLAGS-$(DT_FAAD) += -lfaad
