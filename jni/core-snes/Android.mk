LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := user
LOCAL_ARM_MODE := arm
LOCAL_ARM_NEON := true
LOCAL_MODULE := libcore-snes
LOCAL_CFLAGS += -O3 -fvisibility=hidden -Wno-write-strings -ffast-math -funroll-loops -DINLINE=inline -DRIGHTSHIFT_IS_SAR -DANDROID_ARM -DFRONTEND_SUPPORTS_RGB565 -D__LIBRETRO__
LOCAL_CFLAGS += -DLOG_TAG="\"core-snes\"" # -UNDEBUG -DDEBUG
LOCAL_LDFLAGS 	+= -Wl,--strip-all

LOCAL_SRC_FILES := \
	android/snes-engine.cpp \
	apu.c \
	bsx.c \
	c4emu.c \
	cheats.c \
	controls.c \
	cpu.c \
	cpuexec.c \
	dsp.c \
	fxemu.c \
	globals.c \
	memmap.c \
	obc1.c \
	ppu.c \
	sa1.c \
	sdd1.c \
	seta.c \
	seta010.cpp \
	seta011.cpp \
	seta018.cpp \
	snapshot.c \
	spc7110.c \
	srtc.c \
	tile.c \
	memstream.c

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../engine
LOCAL_LDLIBS    := -lz -llog

include $(BUILD_SHARED_LIBRARY)
