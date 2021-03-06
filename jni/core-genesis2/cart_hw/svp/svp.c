/*
 * The SVP chip emulator
 *
 * Copyright (c) Gražvydas "notaz" Ignotas, 2008
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the organization nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <string.h>
#include "deps.h"
#include "svp.h"

svp_t *svp;
int16 SVP_cycles = 800; 

void svp_init(void)
{
  svp = (void *) ((char *)cart.rom + 0x200000);
  memset(svp, 0, sizeof(*svp));
}

void svp_reset(void)
{
  memcpy(svp->iram_rom + 0x800, cart.rom + 0x800, 0x20000 - 0x800);
  ssp1601_reset(&svp->ssp1601);
}

void svp_write_dram(uint32 address, uint32 data)
{
  *(uint16 *)(svp->dram + (address & 0x1fffe)) = data;
  if ((address == 0x30fe06) && data) svp->ssp1601.emu_status &= ~SSP_WAIT_30FE06;
  if ((address == 0x30fe08) && data) svp->ssp1601.emu_status &= ~SSP_WAIT_30FE08;
}

uint32 svp_read_cell_1(uint32 address)
{
  address >>= 1;
  address = (address & 0x7001) | ((address & 0x3e) << 6) | ((address & 0xfc0) >> 5);
  return *(uint16 *)(svp->dram + (address & 0x1fffe));
}

uint32 svp_read_cell_2(uint32 address)
{
  address >>= 1;
  address = (address & 0x7801) | ((address & 0x1e) << 6) | ((address & 0x7e0) >> 4);
  return *(uint16 *)(svp->dram + (address & 0x1fffe));
}
