
LOCAL_PATH_GB:= $(call my-dir)
LOCAL_PATH:=$(LOCAL_PATH_GB)

include $(CLEAR_VARS)

LOCAL_MODULE    := libcore-gameboy
LOCAL_CFLAGS    := -fno-rtti -O3 -fvisibility=hidden -Wno-write-strings -ffast-math -funroll-loops -Wno-sign-compare -Wno-attributes -DC_CORE -DFINAL_VERSION -DNO_PNG -DNO_LINK # -DSDL
LOCAL_CFLAGS	+= -DLOG_TAG="\"core-gameboy\"" -fvisibility=hidden -DBLARGG_EXPORT="" # -UNDEBUG -DDEBUG
LOCAL_ARM_MODE := arm
LOCAL_ARM_NEON := true

LOCAL_SRC_FILES += android/gameboy-engine.cpp
LOCAL_SRC_FILES += apu/Blip_Buffer.cpp apu/Effects_Buffer.cpp apu/Gb_Apu.cpp apu/Gb_Apu_State.cpp apu/Gb_Oscs.cpp apu/Multi_Buffer.cpp common/memgzio.c
LOCAL_SRC_FILES += gb/GB.cpp gb/gbCheats.cpp gb/gbGfx.cpp gb/gbGlobals.cpp gb/gbMemory.cpp gb/gbPrinter.cpp gb/gbSGB.cpp gb/gbSound.cpp
LOCAL_SRC_FILES += gba/Globals.cpp gba/RTC.cpp gba/Sound.cpp Util.cpp
LOCAL_SRC_FILES += fex/fex.cpp fex/File_Extractor.cpp fex/Binary_Extractor.cpp fex/Data_Reader.cpp fex/blargg_common.cpp fex/blargg_errors.cpp

LOCAL_C_INCLUDES := $(LOCAL_PATH)/apu $(LOCAL_PATH)/common $(LOCAL_PATH)/gb $(LOCAL_PATH)/gba $(LOCAL_PATH)/../engine $(LOCAL_PATH)/../prof
LOCAL_LDLIBS    := -lz -llog
LOCAL_LDFLAGS 	+= -Wl,--strip-all -fno-rtti

include $(BUILD_SHARED_LIBRARY)
