// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#include <stdlib.h>
#include <stdint.h>
#include <memory.h>

/*
 * Thanks to Kawaks' Mr. K for the code

   Incorporated into vba by Anthony Di Franco
*/

static uint8_t *frm1 = NULL;
static uint8_t *frm2 = NULL;
static uint8_t *frm3 = NULL;

int RGB_LOW_BITS_MASK=0x821;

void interframeInit()
{
  frm1 = (uint8_t *)calloc(322*242,4);
  // 1 frame ago
  frm2 = (uint8_t *)calloc(322*242,4);
  // 2 frames ago
  frm3 = (uint8_t *)calloc(322*242,4);
  // 3 frames ago
}

void interframeCleanup()
{
  if(frm1)
    free(frm1);
  if(frm2)
    free(frm2);
  if(frm3)
    free(frm3);
  frm1 = frm2 = frm3 = NULL;
}

void SmartIB(uint8_t *srcPtr, uint32_t srcPitch, int width, int starty, int height)
{
  if(frm1 == NULL) {
    interframeInit();
  }

  uint16_t colorMask = ~RGB_LOW_BITS_MASK;

  uint16_t *src0 = (uint16_t *)srcPtr + starty * srcPitch / 2;
  uint16_t *src1 = (uint16_t *)frm1 + srcPitch * starty / 2;
  uint16_t *src2 = (uint16_t *)frm2 + srcPitch * starty / 2;
  uint16_t *src3 = (uint16_t *)frm3 + srcPitch * starty / 2;

  int sPitch = srcPitch >> 1;

  int pos = 0;
  for (int j = 0; j < height;  j++)
    for (int i = 0; i < sPitch; i++) {
      uint16_t color = src0[pos];
      src0[pos] =
        (src1[pos] != src2[pos]) &&
        (src3[pos] != color) &&
        ((color == src2[pos]) || (src1[pos] == src3[pos]))
        ? (((color & colorMask) >> 1) + ((src1[pos] & colorMask) >> 1)) :
        color;
      src3[pos] = color; /* oldest buffer now holds newest frame */
      pos++;
    }

  /* Swap buffers around */
  uint8_t *temp = frm1;
  frm1 = frm3;
  frm3 = frm2;
  frm2 = temp;
}

void SmartIB(uint8_t *srcPtr, uint32_t srcPitch, int width, int height)
{
  SmartIB(srcPtr, srcPitch, width, 0, height);
}

void MotionBlurIB(uint8_t *srcPtr, uint32_t srcPitch, int width, int starty, int height)
{
  if(frm1 == NULL) {
    interframeInit();
  }

  uint16_t colorMask = ~RGB_LOW_BITS_MASK;

  uint16_t *src0 = (uint16_t *)srcPtr + starty * srcPitch / 2;
  uint16_t *src1 = (uint16_t *)frm1 + starty * srcPitch / 2;

  int sPitch = srcPitch >> 1;

  int pos = 0;
  for (int j = 0; j < height;  j++)
    for (int i = 0; i < sPitch; i++) {
      uint16_t color = src0[pos];
      src0[pos] =
        (((color & colorMask) >> 1) + ((src1[pos] & colorMask) >> 1));
      src1[pos] = color;
      pos++;
    }
}

void MotionBlurIB(uint8_t *srcPtr, uint32_t srcPitch, int width, int height)
{
  MotionBlurIB(srcPtr, srcPitch, width, 0, height);
}

