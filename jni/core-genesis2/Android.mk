DEBUG = 0
PROFILING = 0

LOCAL_PATH:= $(call my-dir)

ifeq ($(PROFILING), 1)
include $(LOCAL_PATH)/../prof/android-ndk-profiler.mk
endif

include $(CLEAR_VARS)

GENPLUS_SRC_DIR := .
LOCAL_MODULE    := libcore-genesis2
LOCAL_CFLAGS	+= -O3 -ffast-math -Wno-write-strings -Wno-sign-compare -DINLINE="static inline" -DUSE_16BPP_RENDERING \
		-DLSB_FIRST -funroll-loops -DALT_RENDERER -DALIGN_LONG # -fomit-frame-pointer
LOCAL_CFLAGS	+= -DLOG_TAG="\"core-genesis\"" -fvisibility=hidden

LOCAL_SRC_FILES += \
			$(GENPLUS_SRC_DIR)/android/genesis-engine.cpp \
			$(GENPLUS_SRC_DIR)/cheats.c \
			$(GENPLUS_SRC_DIR)/genesis.c \
			$(GENPLUS_SRC_DIR)/vdp_ctrl.c \
			$(GENPLUS_SRC_DIR)/vdp_render.c \
			$(GENPLUS_SRC_DIR)/system.c \
			$(GENPLUS_SRC_DIR)/io_ctrl.c \
			$(GENPLUS_SRC_DIR)/loadrom.c \
			$(GENPLUS_SRC_DIR)/mem68k.c \
			$(GENPLUS_SRC_DIR)/memz80.c \
			$(GENPLUS_SRC_DIR)/membnk.c \
			$(GENPLUS_SRC_DIR)/input_hw/activator.c \
			$(GENPLUS_SRC_DIR)/input_hw/gamepad.c \
			$(GENPLUS_SRC_DIR)/input_hw/input.c \
			$(GENPLUS_SRC_DIR)/input_hw/lightgun.c \
			$(GENPLUS_SRC_DIR)/input_hw/mouse.c \
			$(GENPLUS_SRC_DIR)/input_hw/paddle.c \
			$(GENPLUS_SRC_DIR)/input_hw/sportspad.c \
			$(GENPLUS_SRC_DIR)/input_hw/teamplayer.c \
			$(GENPLUS_SRC_DIR)/input_hw/xe_a1p.c \
			$(GENPLUS_SRC_DIR)/cart_hw/md_cart.c \
			$(GENPLUS_SRC_DIR)/cart_hw/sms_cart.c \
			$(GENPLUS_SRC_DIR)/cart_hw/eeprom.c \
			$(GENPLUS_SRC_DIR)/cart_hw/sram.c \
			$(GENPLUS_SRC_DIR)/cart_hw/svp/ssp16.c \
			$(GENPLUS_SRC_DIR)/cart_hw/svp/svp.c \
			$(GENPLUS_SRC_DIR)/sound/blip_buf.c \
			$(GENPLUS_SRC_DIR)/sound/eq.c \
			$(GENPLUS_SRC_DIR)/sound/sound.c \
			$(GENPLUS_SRC_DIR)/sound/ym2612.c \
			$(GENPLUS_SRC_DIR)/sound/ym2413.c \
			$(GENPLUS_SRC_DIR)/sound/sn76489.c \
			$(GENPLUS_SRC_DIR)/z80/z80.c \
			$(GENPLUS_SRC_DIR)/z80/z80_ops.c \
			$(GENPLUS_SRC_DIR)/m68k/m68kcpu.c \
			$(GENPLUS_SRC_DIR)/m68k/m68kops.c \
			$(GENPLUS_SRC_DIR)/state.c

LOCAL_C_INCLUDES += $(LOCAL_PATH)/$(GENPLUS_SRC_DIR) \
			$(LOCAL_PATH)/$(GENPLUS_SRC_DIR)/android \
			$(LOCAL_PATH)/$(GENPLUS_SRC_DIR)/sound \
			$(LOCAL_PATH)/$(GENPLUS_SRC_DIR)/input_hw \
			$(LOCAL_PATH)/$(GENPLUS_SRC_DIR)/cd_hw \
			$(LOCAL_PATH)/$(GENPLUS_SRC_DIR)/cart_hw \
			$(LOCAL_PATH)/$(GENPLUS_SRC_DIR)/cart_hw/svp \
			$(LOCAL_PATH)/$(GENPLUS_SRC_DIR)/m68k \
			$(LOCAL_PATH)/$(GENPLUS_SRC_DIR)/z80 \
			$(LOCAL_PATH)/$(GENPLUS_SRC_DIR)/ntsc \
			$(LOCAL_PATH)/$(GENPLUS_SRC_DIR)/libretro \
			$(LOCAL_PATH)/../engine \
			$(LOCAL_PATH)/../prof

LOCAL_LDLIBS    := -lz -llog

ifeq ($(DEBUG), 1)
LOCAL_CFLAGS			+= -UNDEBUG -DDEBUG
else
LOCAL_CFLAGS			+= -DNDEBUG -fvisibility=hidden
LOCAL_LDFLAGS 			+= -Wl,--strip-all
endif

ifeq ($(PROFILING), 1)
LOCAL_CFLAGS			+= -pg -DPROFILE
LOCAL_LDFLAGS			+= -pg
LOCAL_STATIC_LIBRARIES 	:= andprof
endif

include $(BUILD_SHARED_LIBRARY)
