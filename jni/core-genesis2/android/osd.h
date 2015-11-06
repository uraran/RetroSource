#ifndef _OSD_H
#define _OSD_H

#include <stdlib.h>
#include <string.h>
#include "logging.h"

#define MAX_INPUTS 8

/* Configurable keys */
#define MAX_KEYS 8

/* Key configuration structure */
typedef struct 
{
  int8_t device;
  uint8_t port;
  uint8_t padtype;
} t_input_config;

typedef struct 
{
  char version[16];
  uint8 hq_fm;
  uint8 filter;
  uint8 psgBoostNoise;
  uint8 dac_bits;
  uint8 ym2413_enabled;
  int16 psg_preamp;
  int16 fm_preamp;
  int16 lp_range;
  int16 low_freq;
  int16 high_freq;
  int16 lg;
  int16 mg;
  int16 hg;
  float rolloff;
  uint8 region_detect;
  uint8 force_dtack;
  uint8 addr_error;
  uint8 tmss;
  uint8 lock_on;
  uint8 hot_swap;
  uint8 romtype;
  uint8 invert_mouse;
  uint8 gun_cursor[2];
  uint8 overscan;
  uint8 ntsc;
  uint8 render;
  uint8 tv_mode;
  uint8 bilinear;
  uint8 aspect;
  int16 xshift;
  int16 yshift;
  int16 xscale;
  int16 yscale;
  t_input_config input[MAX_INPUTS];
  uint16 pad_keymap[4][MAX_KEYS];
  uint8 autoload;
  uint8 autocheat;
  uint8 s_auto;
  uint8 s_default;
  uint8 s_device;
  uint8 autocheats;
  int8 bg_type;
  int8 bg_overlay;
  int16 screen_w;
  float bgm_volume;
  float sfx_volume;
} t_config;

/* Global data */
t_config config;

#define osd_input_Update(...)

#endif // _OSD_H
