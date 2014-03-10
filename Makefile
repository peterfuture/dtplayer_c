.PHONY: all clean

#======================================================
#                   HEADER                
#======================================================
include config.mk
include rules.mk

#======================================================
#                   MACRO 
#======================================================
export MAKEROOT := $(shell pwd)
AR       = ar
CC       = gcc
CXX      = gcc
STRIP    = strip 

#CFLAGS   =  -Wall -Wimplicit-function-declaration
CFLAGS   = -g -Wall 
CFLAGS  += $(DT_CFLAGS)
DT_DEBUG = -g

LDFLAGS                  += -lpthread -lz -lm  
LDFLAGS-$(DT_VIDEO_SDL)  += -lSDL
LDFLAGS-$(DT_AUDIO_ALSA) += -lasound
LDFLAGS                  += $(LDFLAGS-yes)

#======================================================
#                   SOURCECODE
#======================================================
#dtutils
SRCS_COMMON-$(DT_UTIL) += dtutils/dt_log.c
SRCS_COMMON-$(DT_UTIL) += dtutils/dt_lock.c
SRCS_COMMON-$(DT_UTIL) += dtutils/dt_ini.c
SRCS_COMMON-$(DT_UTIL) += dtutils/dt_time.c
SRCS_COMMON-$(DT_UTIL) += dtutils/dt_event.c
SRCS_COMMON-$(DT_UTIL) += dtutils/dt_buffer.c
SRCS_COMMON-$(DT_UTIL) += dtutils/dt_queue.c
SRCS_COMMON-$(DT_UTIL) += dtutils/dt_pthread.c

#dtdemuxer
SRCS_COMMON-$(DT_DEMUXER) +=dtdemux/dtdemuxer_api.c
SRCS_COMMON-$(DT_DEMUXER) +=dtdemux/dtdemuxer.c
SRCS_COMMON-$(DT_DEMUXER_FFMPEG) +=dtdemux/demuxer/demuxer_ffmpeg.c

#dtaudio
SRCS_COMMON-$(DT_AUDIO) += dtaudio/dtaudio_api.c
SRCS_COMMON-$(DT_AUDIO) += dtaudio/dtaudio.c
SRCS_COMMON-$(DT_AUDIO) += dtaudio/dtaudio_decoder.c
SRCS_COMMON-$(DT_AUDIO) += dtaudio/dtaudio_filter.c
SRCS_COMMON-$(DT_AUDIO) += dtaudio/dtaudio_output.c
SRCS_COMMON-$(DT_AUDIO_FFMPEG) += dtaudio/audio_decoder/dec_audio_ffmpeg.c # dec
SRCS_COMMON-$(DT_AUDIO_ALSA) += dtaudio/audio_out/ao_alsa.c                # out

#dtvideo
SRCS_COMMON-$(DT_VIDEO) += dtvideo/dtvideo_api.c
SRCS_COMMON-$(DT_VIDEO) += dtvideo/dtvideo.c
SRCS_COMMON-$(DT_VIDEO) += dtvideo/dtvideo_decoder.c
SRCS_COMMON-$(DT_VIDEO) += dtvideo/dtvideo_output.c
SRCS_COMMON-$(DT_VIDEO_FFMPEG) += dtvideo/video_decoder/dec_video_ffmpeg.c  #dec
SRCS_COMMON-$(DT_VIDEO_SDL) += dtvideo/video_out/vo_sdl.c                   #out

#dtport
SRCS_COMMON-$(DT_PORT) += dtport/dt_packet_queue.c
SRCS_COMMON-$(DT_PORT) += dtport/dtport_api.c
SRCS_COMMON-$(DT_PORT) += dtport/dtport.c

#dthost
SRCS_COMMON-$(DT_HOST) += dthost/dthost.c
SRCS_COMMON-$(DT_HOST) += dthost/dthost_api.c

#dtplayer
SRCS_COMMON-$(DT_PLAYER) +=dtplayer/dtplayer_api.c
SRCS_COMMON-$(DT_PLAYER) +=dtplayer/dtplayer.c
SRCS_COMMON-$(DT_PLAYER) +=dtplayer/dtplayer_util.c
SRCS_COMMON-$(DT_PLAYER) +=dtplayer/dtplayer_io.c
SRCS_COMMON-$(DT_PLAYER) +=dtplayer/dtplayer_update.c

SRCS_COMMON +=$(SRCS_COMMON-yes)
OBJS_COMMON += $(addsuffix .o, $(basename $(SRCS_COMMON)))

DIRS =  . \
        dtcommon \
		dtutils \
		dtdemux \
		dtdemux/demuxer \
        dtport \
        dtaudio \
        dtaudio/audio_decoder \
        dtaudio/audio_out \
        dtvideo \
        dtvideo/video_decoder \
        dtvideo/video_out \
        dthost \
		dtplayer   
#header
INCLUDE_DIR += -I$(MAKEROOT)/dtutils 
INCLUDE_DIR += -I$(MAKEROOT)/dtdemux 
INCLUDE_DIR += -I$(MAKEROOT)/dtaudio  
INCLUDE_DIR += -I$(MAKEROOT)/dtvideo 
INCLUDE_DIR += -I$(MAKEROOT)/dtport 
INCLUDE_DIR += -I$(MAKEROOT)/dthost 
INCLUDE_DIR += -I$(MAKEROOT)/dtplayer 
CFLAGS      +=   $(INCLUDE_DIR)

ADDSUFFIXES  = $(foreach suf,$(1),$(addsuffix $(suf),$(2)))
ALL_DIRS     = $(call ADDSUFFIXES,$(1),$(DIRS))

#======================================================
#                   TARGET BUILD                      
#======================================================
EXESUF             = .exe 
PRG-$(DT_PLAYER)  += dtm_player$(EXESUF)
PRG-$(DT_PLAYER)  += dtm_player_g$(EXESUF)
ALL_PRG += $(PRG-yes)

SRCS_DTPLAYER   += dtm_player.c version.c
OBJS_DTPLAYER   += $(addsuffix .o, $(basename $(SRCS_DTPLAYER)))
DTM_PLAYER_DEPS  = $(OBJS_DTPLAYER)  $(OBJS_COMMON) $(COMMON_LIBS)

dtm_player$(EXESUF): $(DTM_PLAYER_DEPS)
	$(CC) -g -o $@ $^ $(LDFLAGS)  

dtm_player_g$(EXESUF): $(DTM_PLAYER_DEPS)
	$(CC) -g -o $@ $^ $(LDFLAGS)  

all: $(ALL_PRG)

clean:
	-rm -f $(call ALL_DIRS,/*.o /*.a /*.ho /*~ /*.exe)
