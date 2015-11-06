/***************************************************************************************
 *  Genesis Plus
 *  Savestate support
 *
 *  Copyright (C) 2007-2011  Eke-Eke (GCN/Wii port)
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
 ****************************************************************************************/

#include "shared.h"

typedef union {
#ifdef LSB_FIRST
  struct { UINT8 l,h,h2,h3; } b;
  struct { UINT16 l,h; } w;
#else
  struct { UINT8 h3,h2,h,l; } b;
  struct { UINT16 h,l; } w;
#endif
  UINT32 d;
}  PAIR;

typedef struct
{
  PAIR  pc,sp,af,bc,de,hl,ix,iy,wz;
  PAIR  af2,bc2,de2,hl2;
  UINT8  r,r2,iff1,iff2,halt,im,i;
  UINT8  nmi_state;      /* nmi line state */
  UINT8  nmi_pending;    /* nmi pending */
  UINT8  irq_state;      /* irq line state */
  UINT8  after_ei;       /* are we in the EI shadow? */
  UINT32 cycles;         /* master clock cycles global counter */
  const struct z80_irq_daisy_chain *daisy;
  int    (*irq_callback)(int irqline);
}  Z80_Regs_OLD;

// for loading states saved with older RetroN 5 versions
int state_load_old(unsigned char *state)
{
	int i, bufferptr = 0x10;

	/* reset system */
	system_reset();

	// GENESIS
	if (system_hw == SYSTEM_PBC) {
		load_param(work_ram, 0x2000);
	} else {
		load_param(work_ram, sizeof(work_ram));
		load_param(zram, sizeof(zram));
		load_param(&zstate, sizeof(zstate));
		load_param(&zbank, sizeof(zbank));
		if (zstate == 3) {
			m68k_memory_map[0xa0].read8 = z80_read_byte;
			m68k_memory_map[0xa0].read16 = z80_read_word;
			m68k_memory_map[0xa0].write8 = z80_write_byte;
			m68k_memory_map[0xa0].write16 = z80_write_word;
		} else {
			m68k_memory_map[0xa0].read8 = m68k_read_bus_8;
			m68k_memory_map[0xa0].read16 = m68k_read_bus_16;
			m68k_memory_map[0xa0].write8 = m68k_unused_8_w;
			m68k_memory_map[0xa0].write16 = m68k_unused_16_w;
		}
	}

	// IO
	if (system_hw == SYSTEM_PBC) {
		load_param(&io_reg[0xf], 1);
		bufferptr+=0x10-1;
	} else {
		load_param(io_reg, sizeof(io_reg));
		io_reg[0] = region_code | 0x20 | (config.tmss & 1);
	}

	  /* VDP */
	  bufferptr += vdp_context_load_old(&state[bufferptr]);

	  /* SOUND */
	  bufferptr += sound_context_load(&state[bufferptr]);

	  /* 68000 */
	  if (system_hw != SYSTEM_PBC)
	  {
	    uint16 tmp16;
	    uint32 tmp32;
	    load_param(&tmp32, 4); m68k_set_reg(M68K_REG_D0, tmp32);
	    load_param(&tmp32, 4); m68k_set_reg(M68K_REG_D1, tmp32);
	    load_param(&tmp32, 4); m68k_set_reg(M68K_REG_D2, tmp32);
	    load_param(&tmp32, 4); m68k_set_reg(M68K_REG_D3, tmp32);
	    load_param(&tmp32, 4); m68k_set_reg(M68K_REG_D4, tmp32);
	    load_param(&tmp32, 4); m68k_set_reg(M68K_REG_D5, tmp32);
	    load_param(&tmp32, 4); m68k_set_reg(M68K_REG_D6, tmp32);
	    load_param(&tmp32, 4); m68k_set_reg(M68K_REG_D7, tmp32);
	    load_param(&tmp32, 4); m68k_set_reg(M68K_REG_A0, tmp32);
	    load_param(&tmp32, 4); m68k_set_reg(M68K_REG_A1, tmp32);
	    load_param(&tmp32, 4); m68k_set_reg(M68K_REG_A2, tmp32);
	    load_param(&tmp32, 4); m68k_set_reg(M68K_REG_A3, tmp32);
	    load_param(&tmp32, 4); m68k_set_reg(M68K_REG_A4, tmp32);
	    load_param(&tmp32, 4); m68k_set_reg(M68K_REG_A5, tmp32);
	    load_param(&tmp32, 4); m68k_set_reg(M68K_REG_A6, tmp32);
	    load_param(&tmp32, 4); m68k_set_reg(M68K_REG_A7, tmp32);
	    load_param(&tmp32, 4); m68k_set_reg(M68K_REG_PC, tmp32);
	    load_param(&tmp16, 2); m68k_set_reg(M68K_REG_SR, tmp16);
	    load_param(&tmp32, 4); m68k_set_reg(M68K_REG_USP,tmp32);
	    load_param(&tmp32, 4); m68k_set_reg(M68K_REG_ISP,tmp32);

	  	load_param(&mcycles_68k, sizeof(mcycles_68k));
	  	load_param(&tmp32, 4);
	  	m68k_irq_state = tmp32 >> 8;
	    bufferptr += 4; // skip m68k.stopped
	  }

	  /* Z80 */
	  Z80_Regs_OLD Z80Old;
	  load_param(&Z80Old, sizeof(Z80_Regs_OLD));

	  z80.af.w = Z80Old.af.w.l;
	  z80.bc.w = Z80Old.bc.w.l;
	  z80.de.w = Z80Old.de.w.l;
	  z80.hl.w = Z80Old.hl.w.l;
	  z80.af_.w = Z80Old.af2.w.l;
	  z80.bc_.w = Z80Old.bc2.w.l;
	  z80.de_.w = Z80Old.de2.w.l;
	  z80.hl_.w = Z80Old.hl2.w.l;
	  z80.ix.w= Z80Old.ix.w.l;
	  z80.iy.w = Z80Old.iy.w.l;
	  z80.i = Z80Old.i;
	  z80.sp.w = Z80Old.sp.w.l;
	  z80.pc.w = Z80Old.pc.w.l;
	  z80.iff1 = Z80Old.iff1;
	  z80.iff2 = Z80Old.iff2;
	  z80.im = Z80Old.im;
	  z80.halted = Z80Old.halt;
	  z80.after_ei = Z80Old.after_ei;

	  // TODO: verify these
	  z80.r = Z80Old.r;
	  z80.r7 = Z80Old.r2;

	  if (system_hw == SYSTEM_PBC)
	  {
	    /* MS cartridge hardware */
	    bufferptr += sms_cart_context_load(&state[bufferptr]);
	    sms_cart_switch(~io_reg[0x0E]);
	  }
	  else
	  {
	    /* MD cartridge hardware */
	    bufferptr += md_cart_context_load(&state[bufferptr]);
	  }

	return bufferptr;
}

int state_load(unsigned char *state)
{
	int i, bufferptr = 0;
	unsigned char *state_initial = state;

	/* signature check (GENPLUS-GX x.x.x) */
	char version[17];
	load_param(version, 16);
	version[16] = 0;
	if (!memcmp(version, STATE_VERSION_OLD, 16))
		return state_load_old(state_initial);

	if (memcmp(version, STATE_VERSION, 16)) {
		return 0;
	}
	/* reset system */
	system_reset();

	// GENESIS
	if (system_hw == SYSTEM_PBC) {
		load_param(work_ram, 0x2000);
	} else {
		load_param(work_ram, sizeof(work_ram));
		load_param(zram, sizeof(zram));
		load_param(&zstate, sizeof(zstate));
		load_param(&zbank, sizeof(zbank));
		if (zstate == 3) {
			m68k_memory_map[0xa0].read8 = z80_read_byte;
			m68k_memory_map[0xa0].read16 = z80_read_word;
			m68k_memory_map[0xa0].write8 = z80_write_byte;
			m68k_memory_map[0xa0].write16 = z80_write_word;
		} else {
			m68k_memory_map[0xa0].read8 = m68k_read_bus_8;
			m68k_memory_map[0xa0].read16 = m68k_read_bus_16;
			m68k_memory_map[0xa0].write8 = m68k_unused_8_w;
			m68k_memory_map[0xa0].write16 = m68k_unused_16_w;
		}
	}

	/* extended state */
	load_param(&mcycles_68k, sizeof(mcycles_68k));
	load_param(&mcycles_z80, sizeof(mcycles_z80));

	// IO
	if (system_hw == SYSTEM_PBC) {
		load_param(&io_reg[0], 1);
	} else {
		load_param(io_reg, sizeof(io_reg));
		io_reg[0] = region_code | 0x20 | (config.tmss & 1);
	}

	// VDP
	bufferptr += vdp_context_load(&state[bufferptr]);

	// SOUND
	bufferptr += sound_context_load(&state[bufferptr]);

	// 68000
	if (system_hw != SYSTEM_PBC) {
		uint16 tmp16;
		uint32 tmp32;
		load_param(&tmp32, 4);
		m68k_set_reg(M68K_REG_D0, tmp32);
		load_param(&tmp32, 4);
		m68k_set_reg(M68K_REG_D1, tmp32);
		load_param(&tmp32, 4);
		m68k_set_reg(M68K_REG_D2, tmp32);
		load_param(&tmp32, 4);
		m68k_set_reg(M68K_REG_D3, tmp32);
		load_param(&tmp32, 4);
		m68k_set_reg(M68K_REG_D4, tmp32);
		load_param(&tmp32, 4);
		m68k_set_reg(M68K_REG_D5, tmp32);
		load_param(&tmp32, 4);
		m68k_set_reg(M68K_REG_D6, tmp32);
		load_param(&tmp32, 4);
		m68k_set_reg(M68K_REG_D7, tmp32);
		load_param(&tmp32, 4);
		m68k_set_reg(M68K_REG_A0, tmp32);
		load_param(&tmp32, 4);
		m68k_set_reg(M68K_REG_A1, tmp32);
		load_param(&tmp32, 4);
		m68k_set_reg(M68K_REG_A2, tmp32);
		load_param(&tmp32, 4);
		m68k_set_reg(M68K_REG_A3, tmp32);
		load_param(&tmp32, 4);
		m68k_set_reg(M68K_REG_A4, tmp32);
		load_param(&tmp32, 4);
		m68k_set_reg(M68K_REG_A5, tmp32);
		load_param(&tmp32, 4);
		m68k_set_reg(M68K_REG_A6, tmp32);
		load_param(&tmp32, 4);
		m68k_set_reg(M68K_REG_A7, tmp32);
		load_param(&tmp32, 4);
		m68k_set_reg(M68K_REG_PC, tmp32);
		load_param(&tmp16, 2);
		m68k_set_reg(M68K_REG_SR, tmp16);
		load_param(&tmp32, 4);
		m68k_set_reg(M68K_REG_USP, tmp32);
		load_param(&tmp32, 4);
		m68k_set_reg(M68K_REG_ISP, tmp32);
	}

	// Z80
	load_param(&z80, sizeof(z80));

	// Cartridge HW
	if (system_hw == SYSTEM_PBC) {
		bufferptr += sms_cart_context_load(&state[bufferptr]);
	} else {
		bufferptr += md_cart_context_load(&state[bufferptr]);
	}

	return 1;
}

int state_save(unsigned char *state)
{
	/* buffer size */
	int bufferptr = 0;

	/* version string */
	char version[16];
	strncpy(version, STATE_VERSION, 16);
	save_param(version, 16);

	// GENESIS
	if (system_hw == SYSTEM_PBC) {
		save_param(work_ram, 0x2000);
	} else {
		save_param(work_ram, sizeof(work_ram));
		save_param(zram, sizeof(zram));
		save_param(&zstate, sizeof(zstate));
		save_param(&zbank, sizeof(zbank));
	}
	save_param(&mcycles_68k, sizeof(mcycles_68k));
	save_param(&mcycles_z80, sizeof(mcycles_z80));

	// IO
	if (system_hw == SYSTEM_PBC) {
		save_param(&io_reg[0], 1);
	} else {
		save_param(io_reg, sizeof(io_reg));
	}

	// VDP
	bufferptr += vdp_context_save(&state[bufferptr]);

	// SOUND
	bufferptr += sound_context_save(&state[bufferptr]);

	// 68000
	if (system_hw != SYSTEM_PBC) {
		uint16 tmp16;
		uint32 tmp32;
		tmp32 = m68k_get_reg(NULL, M68K_REG_D0);
		save_param(&tmp32, 4);
		tmp32 = m68k_get_reg(NULL, M68K_REG_D1);
		save_param(&tmp32, 4);
		tmp32 = m68k_get_reg(NULL, M68K_REG_D2);
		save_param(&tmp32, 4);
		tmp32 = m68k_get_reg(NULL, M68K_REG_D3);
		save_param(&tmp32, 4);
		tmp32 = m68k_get_reg(NULL, M68K_REG_D4);
		save_param(&tmp32, 4);
		tmp32 = m68k_get_reg(NULL, M68K_REG_D5);
		save_param(&tmp32, 4);
		tmp32 = m68k_get_reg(NULL, M68K_REG_D6);
		save_param(&tmp32, 4);
		tmp32 = m68k_get_reg(NULL, M68K_REG_D7);
		save_param(&tmp32, 4);
		tmp32 = m68k_get_reg(NULL, M68K_REG_A0);
		save_param(&tmp32, 4);
		tmp32 = m68k_get_reg(NULL, M68K_REG_A1);
		save_param(&tmp32, 4);
		tmp32 = m68k_get_reg(NULL, M68K_REG_A2);
		save_param(&tmp32, 4);
		tmp32 = m68k_get_reg(NULL, M68K_REG_A3);
		save_param(&tmp32, 4);
		tmp32 = m68k_get_reg(NULL, M68K_REG_A4);
		save_param(&tmp32, 4);
		tmp32 = m68k_get_reg(NULL, M68K_REG_A5);
		save_param(&tmp32, 4);
		tmp32 = m68k_get_reg(NULL, M68K_REG_A6);
		save_param(&tmp32, 4);
		tmp32 = m68k_get_reg(NULL, M68K_REG_A7);
		save_param(&tmp32, 4);
		tmp32 = m68k_get_reg(NULL, M68K_REG_PC);
		save_param(&tmp32, 4);
		tmp16 = m68k_get_reg(NULL, M68K_REG_SR);
		save_param(&tmp16, 2);
		tmp32 = m68k_get_reg(NULL, M68K_REG_USP);
		save_param(&tmp32, 4);
		tmp32 = m68k_get_reg(NULL, M68K_REG_ISP);
		save_param(&tmp32, 4);
	}

	// Z80
	save_param(&z80, sizeof(z80));

	// Cartridge HW
	if (system_hw == SYSTEM_PBC) {
		bufferptr += sms_cart_context_save(&state[bufferptr]);
	} else {
		bufferptr += md_cart_context_save(&state[bufferptr]);
	}

	/* return total size */
	return bufferptr;
}
