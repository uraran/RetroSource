LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := libcore-gba
LOCAL_CFLAGS    := 	-O3 -fvisibility=hidden -Wno-write-strings -ffast-math -funroll-loops -Wno-sign-compare -Wno-attributes \
					-DINLINE=inline -DHAVE_STDINT_H -DHAVE_INTTYPES_H -DSPEEDHAX -DLSB_FIRST -D__LIBRETRO__ -DFRONTEND_SUPPORTS_RGB565 -DTILED_RENDERING # -UNDEBUG -DDEBUG
LOCAL_CFLAGS	+= -DLOG_TAG="\"core-gba\"" -fvisibility=hidden
LOCAL_ARM_MODE := arm
LOCAL_ARM_NEON := true

LOCAL_SRC_FILES += android/gba-engine.cpp android/interframe.cpp
LOCAL_SRC_FILES += gba.cpp memory.cpp sound.cpp

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../engine $(LOCAL_PATH)/../prof
LOCAL_LDLIBS    := -lz -llog
LOCAL_LDFLAGS 	+= -Wl,--strip-all

include $(BUILD_SHARED_LIBRARY)
