#include "shared.h"
#include <android/log.h>
#include "logging.h"

#define MAX_CHEATS (150)

typedef struct 
{
	uint8_t applied;
	uint16_t data;
	uint16_t old;
	uint32_t address;
} CHEATENTRY;

static CHEATENTRY cheatlist[MAX_CHEATS];
static uint8_t RAMcheatlist[MAX_CHEATS];
static int cheatCount = 0;
static int ramCheatCount = 0;
static char ggvalidchars[] = "ABCDEFGHJKLMNPRSTVWXYZ0123456789";
static char arvalidchars[] = "0123456789ABCDEF";

static uint32_t decode_cheat(char *string, uint32_t *address, uint32_t *data)
{
	char *p;
	int i,n;

	/* reset address & data values */
	*address = 0;
	*data = 0;

	/* Game Genie type code (XXXX-YYYY) */
	if ((strlen(string) >= 9) && (string[4] == '-'))
	{
		if(system_hw == SYSTEM_PBC || system_hw == SYSTEM_GAMEGEAR) return 0;

		for (i = 0; i < 8; i++)
		{
			if (i == 4) string++;
			p = strchr (ggvalidchars, *string++);
			if (p == NULL) return 0;
			n = p - ggvalidchars;

			switch (i)
			{
			case 0:
				*data |= n << 3;
				break;

			case 1:
				*data |= n >> 2;
				*address |= (n & 3) << 14;
				break;

			case 2:
				*address |= n << 9;
				break;

			case 3:
				*address |= (n & 0xF) << 20 | (n >> 4) << 8;
				break;

			case 4:
				*data |= (n & 1) << 12;
				*address |= (n >> 1) << 16;
				break;

			case 5:
				*data |= (n & 1) << 15 | (n >> 1) << 8;
				break;

			case 6:
				*data |= (n >> 3) << 13;
				*address |= (n & 7) << 5;
				break;

			case 7:
				*address |= n;
				break;
			}
		}

		/* return code length */
		return 9;
	}

	/* Action Replay type code (AAAAAA:DDDD) */
	else if (string[6] == ':')
	{
		if(system_hw != SYSTEM_PBC && system_hw != SYSTEM_GAMEGEAR)
		{
			/* decode address */
			for (i=0; i<6; i++)
			{
				p = strchr (arvalidchars, *string++);
				if (p == NULL) return 0;
				n = (p - arvalidchars) & 15;
				*address |= (n << ((5 - i) * 4));
			}

			/* decode data */
			string++;
			for (i=0; i<4; i++)
			{
				p = strchr (arvalidchars, *string++);
				if (p == NULL) return 0;
				n = p - arvalidchars;
				*data |= (n & 15) << ((3 - i) * 4);
			}

			/* return code length */
			return 11;
		}
		else
		{
			/* decode address */
			string+=2;
			for (i=0; i<4; i++)
			{
				p = strchr (arvalidchars, *string++);
				if (p == NULL) return 0;
				n = (p - arvalidchars) & 15;
				*address |= (n << ((3 - i) * 4));
			}
			if (*address < 0xC000) return 0;

			// convert to RAM addr
			*address = 0xFF0000 | (*address & 0x1FFF);

			/* decode data */
			string++;
			for (i=0; i<2; i++)
			{
				p = strchr (arvalidchars, *string++);
				if (p == NULL) return 0;
				n = p - arvalidchars;
				*data |= (n  << ((1 - i) * 4));
			}

			/* return code length */
			return 9;

		}
	}

	return 0;
}

void apply_cheats(void)
{
	int i;

	ramCheatCount = 0;

	for (i = 0; i < cheatCount; i++)
	{
		if (cheatlist[i].address < cart.romsize && !cheatlist[i].applied)
		{
			/* patch ROM data */
			cheatlist[i].old = *(uint16_t *)(cart.rom + (cheatlist[i].address & 0xFFFFFE));
			*(uint16_t *)(cart.rom + (cheatlist[i].address & 0xFFFFFE)) = cheatlist[i].data;
			cheatlist[i].applied = 1;
		}
		else if (cheatlist[i].address >= 0xFF0000)
		{
			/* patch RAM data */
			RAMcheatlist[ramCheatCount++] = i;
		}
	}
}

int add_cheat(const char *codeStr)
{
	uint32_t address = 0, data = 0;
	int ret = decode_cheat((char *)codeStr, &address, &data);
	if(ret != 0)
	{
		cheatlist[cheatCount].address = address;
		cheatlist[cheatCount].data = data;
		cheatlist[cheatCount].applied = 0;
		cheatCount++;
	}
	LOGI("add cheat \'%s\' result: %d\n", codeStr, ret);
	apply_cheats();
	return ret;
}


void clear_cheats(void)
{
	int i = cheatCount;

	/* disable cheats in reversed order in case the same address is used by multiple patches */
	while (i > 0)
	{
		/* restore original ROM data */
		if (cheatlist[i-1].applied && (cheatlist[i-1].address < cart.romsize))
		{
			*(uint16_t *)(cart.rom + (cheatlist[i-1].address & 0xFFFFFE)) = cheatlist[i-1].old;
			cheatlist[i-1].applied = 0;
		}

		i--;
	}

	cheatCount = 0;
	ramCheatCount = 0;
}

void RAMCheatUpdate(void)
{
	int index, cnt = ramCheatCount;

	while (cnt)
	{
		/* get cheat index */
		index = RAMcheatlist[--cnt];

		/* apply RAM patch */
		if (cheatlist[index].data & 0xFF00)
		{
			/* word patch */
			*(uint16_t *)(work_ram + (cheatlist[index].address & 0xFFFE)) = cheatlist[index].data;
		}
		else
		{
			/* byte patch */
			work_ram[cheatlist[index].address & 0xFFFF] = cheatlist[index].data;
		}
	}
}
