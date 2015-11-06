LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := libcore-pce
LOCAL_CFLAGS    := -O3 -Wno-write-strings -Wno-sign-compare -DANDROID_ARM -DWANT_PCE_FAST_EMU -DWANT_STEREO_SOUND -DWANT_CRC32 \
 					-DSIZEOF_DOUBLE=8 $(WARNINGS) -DMEDNAFEN_VERSION=\"0.9.26\" -DPACKAGE=\"mednafen\" -DMEDNAFEN_VERSION_NUMERIC=926 -DPSS_STYLE=1 \
 					-DMPC_FIXED_POINT $(CORE_DEFINE) -DSTDC_HEADERS -D__STDC_LIMIT_MACROS -D__LIBRETRO__ -DNDEBUG -D_LOW_ACCURACY_ $(SOUND_DEFINE) -DLSB_FIRST \
 					-DFRONTEND_SUPPORTS_RGB565 -DWANT_32BPP # -UNDEBUG -DDEBUG
LOCAL_CFLAGS	+= -DLOG_TAG="\"core-pce\"" -fexceptions -fvisibility=hidden

LOCAL_SRC_FILES	+= android/pce-engine.cpp \
				mednafen/pce_fast/huc.cpp \
				mednafen/pce_fast/huc6280.cpp \
				mednafen/pce_fast/input.cpp \
				mednafen/pce_fast/pce.cpp \
				mednafen/pce_fast/tsushin.cpp \
				mednafen/pce_fast/vdc.cpp \
				mednafen/hw_cpu/huc6280/cpu_huc6280.cpp \
				mednafen/hw_misc/arcade_card/arcade_card.cpp \
				mednafen/hw_sound/pce_psg/pce_psg.cpp \
				mednafen/hw_video/huc6270/vdc_video.cpp \
				mednafen/okiadpcm.cpp \
				mednafen/mednafen.cpp \
				mednafen/error.cpp \
				mednafen/math_ops.cpp \
				mednafen/settings.cpp \
				mednafen/general.cpp \
				mednafen/FileWrapper.cpp \
				mednafen/FileStream.cpp \
				mednafen/MemoryStream.cpp \
				mednafen/Stream.cpp \
				mednafen/state.cpp \
				mednafen/endian.cpp \
				mednafen/mempatcher.cpp \
				mednafen/video/Deinterlacer.cpp \
				mednafen/video/surface.cpp \
				mednafen/sound/Blip_Buffer.cpp \
				mednafen/sound/Stereo_Buffer.cpp \
				mednafen/file.cpp \
				mednafen/md5.cpp	\
				mednafen/trio/trio.c \
				mednafen/trio/triostr.c \
				scrc32.cpp \
				stubs.cpp

#				mednafen/cdrom/pcecd.cpp \
				
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../engine $(LOCAL_PATH)/mednafen $(LOCAL_PATH)/mednafen/include $(LOCAL_PATH)/mednafen/intl $(LOCAL_PATH)/mednafen/hw_cpu $(LOCAL_PATH)/mednafen/hw_sound $(LOCAL_PATH)/mednafen/hw_misc $(LOCAL_PATH)/mednafen/hw_video
LOCAL_LDLIBS    := -lz -llog
LOCAL_LDFLAGS 	+= -Wl,--strip-all

include $(BUILD_SHARED_LIBRARY)
