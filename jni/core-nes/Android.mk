LOCAL_PATH_NES:= $(call my-dir)
LOCAL_PATH:=$(LOCAL_PATH_NES)

include $(CLEAR_VARS)

LOCAL_MODULE    := libcore-nes
LOCAL_CFLAGS    += -O3 -fno-inline -Wno-write-strings -Wno-sign-compare -DLOCAL_LE=1 -DLSB_FIRST=1 -D__LIBRETRO__ -DSOUND_QUALITY=0 \
					-DINLINE=inline -DPSS_STYLE=1 -DFCEU_VERSION_NUMERIC=9813 -DFRONTEND_SUPPORTS_RGB565 -DHAVE_ASPRINTF
LOCAL_CFLAGS	+= -DLOG_TAG="\"core-nes\"" -fvisibility=hidden # -UNDEBUG -DDEBUG
LOCAL_C_INCLUDES := $(LOCAL_PATH)/cpu $(LOCAL_PATH)/sound $(LOCAL_PATH)/../engine
LOCAL_LDLIBS    := -lz -llog
LOCAL_LDFLAGS 	+= -Wl,--strip-all

FCEU_SRC_DIRS := . ./boards ./input
FCEU_CSRCS := $(subst $(LOCAL_PATH_NES),,$(foreach dir,$(FCEU_SRC_DIRS),$(wildcard $(LOCAL_PATH_NES)/$(dir)/*.c)))
LOCAL_SRC_FILES += android/nes-engine.cpp $(FCEU_CSRCS)

include $(BUILD_SHARED_LIBRARY)
