/***************************************************************************************
 *  Genesis Plus
 *
 *  Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003  Charles Mac Donald (original code)
 *  Eke-Eke (2007,2008,2009), additional code & fixes for the GCN/Wii port
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Sound Hardware
 ****************************************************************************************/

#include "shared.h"

static int fm_buffer[4096];
static int fm_last[2];
static int *fm_ptr;

/* Cycle-accurate samples */
static unsigned int fm_cycles_ratio;
static unsigned int fm_cycles_start;
static unsigned int fm_cycles_count;

/* YM chip function pointers */
static void (*YM_Reset)(void);
static void (*YM_Update)(int *buffer, int length);
static void (*YM_Write)(unsigned int a, unsigned int v);

/* Run FM chip for required M-cycles */
static inline void fm_update(unsigned int cycles)
{
  if (cycles > fm_cycles_count)
  {
    /* period to run */
    cycles -= fm_cycles_count;

    /* update cycle count */
    fm_cycles_count += cycles;

    /* number of samples during period */
    unsigned int cnt = cycles / fm_cycles_ratio;

    /* remaining cycles */
    unsigned int remain = cycles % fm_cycles_ratio;
    if (remain)
    {
      /* one sample ahead */
      fm_cycles_count += fm_cycles_ratio - remain;
      cnt++;
    }

    YM_Update(fm_ptr, cnt);    
    fm_ptr += (cnt << 1);
  }
}

/* Initialize sound chips emulation */
void sound_init(void)
{
  if (system_hw != SYSTEM_PBC)
  {
    YM2612Init();
    YM2612Config(config.dac_bits);
    YM_Reset = YM2612ResetChip;
    YM_Update = YM2612Update;
    YM_Write = YM2612Write;
    fm_cycles_ratio = 144 * 7;
  }
  else
  {
    YM2413Init();
    YM_Reset = YM2413ResetChip;
    YM_Update = YM2413Update;
    YM_Write = YM2413Write;
    fm_cycles_ratio = 72 * 15;
  }
  SN76489_Config(0, config.psg_preamp, config.psgBoostNoise, 0xff);
}

/* Reset sound chips emulation */
void sound_reset(void)
{
  YM_Reset();
  SN76489_Reset();
  fm_last[0] = fm_last[1] = 0;
  fm_ptr = fm_buffer;
  fm_cycles_start = fm_cycles_count = 0;
}

int sound_context_save(uint8 *state)
{
  int bufferptr = 0;

  if (system_hw != SYSTEM_PBC)
  {
    bufferptr = YM2612SaveContext(state);
  }
  else
  {
    save_param(YM2413GetContextPtr(),YM2413GetContextSize());
  }

  save_param(SN76489_GetContextPtr(),SN76489_GetContextSize());
  save_param(&fm_cycles_start,sizeof(fm_cycles_start));

  return bufferptr;
}

int sound_context_load(uint8 *state)
{
  int bufferptr = 0;

  if (system_hw != SYSTEM_PBC)
  {
    bufferptr = YM2612LoadContext(state);
  }
  else
  {
    load_param(YM2413GetContextPtr(),YM2413GetContextSize());
  }

  load_param(SN76489_GetContextPtr(),SN76489_GetContextSize());

  load_param(&fm_cycles_start,sizeof(fm_cycles_start));
  fm_cycles_count = fm_cycles_start;

  return bufferptr;
}

/* End of frame update, return the number of samples run so far.  */
int sound_update(unsigned int cycles)
{
  int delta, preamp, time, l, r, *ptr;

  /* Run PSG & FM chips until end of frame */
  SN76489_Update(cycles);
  fm_update(cycles);

	/* FM output pre-amplification */
  preamp = config.fm_preamp;

  /* FM frame initial timestamp */
  time = fm_cycles_start;

  /* Restore last FM outputs from previous frame */
  l = fm_last[0];
  r = fm_last[1];

  /* FM buffer start pointer */
  ptr = fm_buffer;

  do
  {
    delta = ((*ptr++ * preamp) / 100) - l;
    l += delta;
    blip_add_delta(snd.blips[0], time, delta);      
    delta = ((*ptr++ * preamp) / 100) - r;
    r += delta;
    blip_add_delta(snd.blips[1], time, delta);
    time += fm_cycles_ratio;
  }
  while (time < cycles);

  fm_ptr = fm_buffer;

  fm_last[0] = l;
  fm_last[1] = r;

  fm_cycles_count = fm_cycles_start = time - cycles;
	
  blip_end_frame(snd.blips[0], cycles);
  blip_end_frame(snd.blips[1], cycles);

  return blip_samples_avail(snd.blips[0]);
}

/* Reset FM chip */
void fm_reset(unsigned int cycles)
{
  fm_update(cycles);
  YM_Reset();
}

/* Write FM chip */
void fm_write(unsigned int cycles, unsigned int address, unsigned int data)
{
  if (address & 1) fm_update(cycles);
  YM_Write(address, data);
}

/* Read FM status (YM2612 only) */
unsigned int fm_read(unsigned int cycles, unsigned int address)
{
  fm_update(cycles);
  return YM2612Read();
}

/* Write PSG chip */
void psg_write(unsigned int cycles, unsigned int data)
{
  SN76489_Write(cycles, data);
}
