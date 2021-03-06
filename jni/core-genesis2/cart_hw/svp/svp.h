#ifndef _SVP_H_
#define _SVP_H_

#include "shared.h"
#include "ssp16.h"

typedef struct {
	unsigned char iram_rom[0x20000]; // IRAM (0-0x7ff) and program ROM (0x800-0x1ffff)
	unsigned char dram[0x20000];
	ssp1601_t ssp1601;
} svp_t;

extern svp_t *svp;
extern int16 SVP_cycles; 

extern void svp_init(void);
extern void svp_reset(void);
extern void svp_write_dram(uint32 address, uint32 data);
extern uint32 svp_read_cell_1(uint32 address);
extern uint32 svp_read_cell_2(uint32 address);

#endif
