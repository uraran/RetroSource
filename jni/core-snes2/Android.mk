LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := user
LOCAL_ARM_MODE := arm
LOCAL_ARM_NEON := true
LOCAL_MODULE := libcore-snes2
LOCAL_CFLAGS += -O3 -fvisibility=hidden -Wno-write-strings -ffast-math -funroll-loops -DINLINE=inline -DRIGHTSHIFT_IS_SAR -DANDROID_ARM -DFRONTEND_SUPPORTS_RGB565 -D__LIBRETRO__ -U__linux
LOCAL_CFLAGS += -DLOG_TAG="\"core-snes\"" # -UNDEBUG -DDEBUG
LOCAL_LDFLAGS 	+= -Wl,--strip-all

LOCAL_SRC_FILES := \
	android/snes-engine.cpp \
	apu/apu.cpp \
	apu/bapu/dsp/sdsp.cpp \
	apu/bapu/dsp/SPC_DSP.cpp \
	apu/bapu/smp/smp.cpp \
	apu/bapu/smp/smp_state.cpp \
	bsx.cpp \
	c4.cpp \
	c4emu.cpp \
	cheats.cpp \
	cheats2.cpp \
	clip.cpp \
	conffile.cpp \
	controls.cpp \
	cpu.cpp \
	cpuexec.cpp \
	cpuops.cpp \
	crosshairs.cpp \
	dma.cpp \
	dsp.cpp \
	dsp1.cpp \
	dsp2.cpp \
	dsp3.cpp \
	dsp4.cpp \
	fxinst.cpp \
	fxemu.cpp \
	gfx.cpp \
	globals.cpp \
	logger.cpp \
	memmap.cpp \
	movie.cpp \
	obc1.cpp \
	ppu.cpp \
	reader.cpp \
	sa1.cpp \
	sa1cpu.cpp \
	screenshot.cpp \
	sdd1.cpp \
	sdd1emu.cpp \
	seta.cpp \
	seta010.cpp \
	seta011.cpp \
	seta018.cpp \
	snapshot.cpp \
	snes9x.cpp \
	spc7110.cpp \
	srtc.cpp \
	tile.cpp

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../engine $(LOCAL_PATH)/apu $(LOCAL_PATH)/apu/bapu
LOCAL_LDLIBS    := -lz -llog

include $(BUILD_SHARED_LIBRARY)
