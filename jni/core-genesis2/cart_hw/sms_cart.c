
/****************************************************************************
 *  Genesis Plus
 *  Master System cartridge hardware support
 *
 *  
 *  Copyright (C) 1998-2007  Charles MacDonald (SMS Plus original  code)
 *  Eke-Eke (2007-2011), additional code & fixes for the GCN/Wii port
 *
 *  Most cartridge protections documented by Haze
 *  (http://haze.mameworld.info/)
 *
 *  Realtec mapper documented by TascoDeluxe
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
 ***************************************************************************/

#include "shared.h"

#define MAPPER_NONE   (0)
#define MAPPER_SEGA   (1)
#define MAPPER_CODIES (2)
#define MAPPER_KOREA  (3)
#define MAPPER_MSX    (4)

#define GAME_DATABASE_CNT (sizeof(game_list)/sizeof(rominfo_t))

typedef struct
{
  uint32 crc;
  uint8 glasses_3d;
  uint8 support_fm;
  uint8 peripheral;
  uint8 mapper;
  uint8 region;
} rominfo_t;

static struct
{
  uint8 fcr[4];
  uint8 mapper;
} slot;

/* SMS game database */
static const rominfo_t game_list[] =
{
  /* games requiring CODEMASTER mapper (NOTE: extended video modes don't work on Genesis VDP !) */
  {0x29822980, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_CODIES,      REGION_EUROPE}, /* Cosmic Spacehead */
  {0xA577CE46, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_CODIES,      REGION_EUROPE}, /* Micro Machines */
  {0xF7C524F6, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_CODIES,      REGION_EUROPE}, /* Micro Machines [BAD DUMP] */
  {0xDBE8895C, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_CODIES,      REGION_EUROPE}, /* Micro Machines 2 - Turbo Tournament */
  {0xC1756BEE, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_CODIES,      REGION_EUROPE}, /* Pete Sampras Tennis */
  {0x8813514B, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_CODIES,      REGION_EUROPE}, /* Excellent Dizzy Collection, The [Proto] */
  {0xEA5C3A6F, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_CODIES,         REGION_USA}, /* Dinobasher - Starring Bignose the Caveman [Proto] */
  {0x152F0DCC, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_CODIES,         REGION_USA}, /* Drop Zone" */
  {0xAA140C9C, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_CODIES,         REGION_USA}, /* Excellent Dizzy Collection, The [SMS-GG] */
  {0xB9664AE1, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_CODIES,         REGION_USA}, /* Fantastic Dizzy */
  {0xC888222B, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_CODIES,         REGION_USA}, /* Fantastic Dizzy [SMS-GG] */
  {0x76C5BDFB, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_CODIES,         REGION_USA}, /* Jang Pung 2 [SMS-GG] */
  {0xD9A7F170, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_CODIES,         REGION_USA}, /* Man Overboard! */

  /* games requiring KOREA mappers (NOTE: TMS9918 video modes don't work on Genesis VDP !) */
  {0x17AB6883, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_NONE,    REGION_JAPAN_NTSC}, /* FA Tetris (KR) */
  {0x61E8806F, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_NONE,    REGION_JAPAN_NTSC}, /* Flash Point (KR) */
  {0x445525E2, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_MSX,     REGION_JAPAN_NTSC}, /* Penguin Adventure (KR) */
  {0x83F0EEDE, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_MSX,     REGION_JAPAN_NTSC}, /* Street Master (KR) */
  {0xA05258F5, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_MSX,     REGION_JAPAN_NTSC}, /* Won-Si-In (KR) */
  {0x06965ED9, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_MSX,     REGION_JAPAN_NTSC}, /* F-1 Spirit - The way to Formula-1 (KR) */
  {0x89B79E77, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_KOREA,   REGION_JAPAN_NTSC}, /* Dodgeball King (KR) */
  {0x18FB98A3, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_KOREA,   REGION_JAPAN_NTSC}, /* Jang Pung 3 (KR) */
  {0x97D03541, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_KOREA,   REGION_JAPAN_NTSC}, /* Sangokushi 3 (KR)"} */
  {0x67C2F0FF, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_KOREA,   REGION_JAPAN_NTSC}, /* Super Boy 2 (KR) */

  /* games requiring PAL timings */
  {0x72420F38, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,        REGION_EUROPE}, /* Addams Familly */
  {0x2D48C1D3, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,        REGION_EUROPE}, /* Back to the Future Part III */
  {0x1CBB7BF1, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,        REGION_EUROPE}, /* Battlemaniacs (BR) */
  {0x1B10A951, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,        REGION_EUROPE}, /* Bram Stoker's Dracula */
  {0xC0E25D62, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,        REGION_EUROPE}, /* California Games II */
  {0x45C50294, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,        REGION_EUROPE}, /* Jogos de Verao II (BR) */
  {0xC9DBF936, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,        REGION_EUROPE}, /* Home Alone */
  {0x0047B615, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,        REGION_EUROPE}, /* Predator2 */
  {0xF42E145C, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,        REGION_EUROPE}, /* Quest for the Shaven Yak Starring Ren Hoek & Stimpy (BR) */
  {0x9F951756, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,        REGION_EUROPE}, /* RoboCop 3 */
  {0xF8176918, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,        REGION_EUROPE}, /* Sensible Soccer */
  {0x1575581D, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,        REGION_EUROPE}, /* Shadow of the Beast */
  {0x96B3F29E, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,        REGION_EUROPE}, /* Sonic Blast (BR) */
  {0x5B3B922C, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,        REGION_EUROPE}, /* Sonic the Hedgehog 2 [V0] */
  {0xD6F2BFCA, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,        REGION_EUROPE}, /* Sonic the Hedgehog 2 [V1] */
  {0xCA1D3752, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,        REGION_EUROPE}, /* Space Harrier [50 Hz] */
  {0x85CFC9C9, 0, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,        REGION_EUROPE}, /* Taito Chase H.Q. */

  /* games requiring 3-D Glasses */
  {0x871562b0, 1, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,    REGION_JAPAN_NTSC}, /* Maze Walker */
  {0x156948f9, 1, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,    REGION_JAPAN_NTSC}, /* Space Harrier 3-D (J) */
  {0x6BD5C2BF, 1, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,           REGION_USA}, /* Space Harrier 3-D */
  {0x8ECD201C, 1, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,           REGION_USA}, /* Blade Eagle 3-D */
  {0xFBF96C81, 1, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,           REGION_USA}, /* Blade Eagle 3-D (BR) */
  {0x58D5FC48, 1, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,           REGION_USA}, /* Blade Eagle 3-D [Proto] */
  {0x31B8040B, 1, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,           REGION_USA}, /* Maze Hunter 3-D */
  {0xABD48AD2, 1, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,           REGION_USA}, /* Poseidon Wars 3-D */
  {0xA3EF13CB, 1, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,           REGION_USA}, /* Zaxxon 3-D */
  {0xBBA74147, 1, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,           REGION_USA}, /* Zaxxon 3-D [Proto] */
  {0xD6F43DDA, 1, 0, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,           REGION_USA}, /* Out Run 3-D */

  /* games requiring 3-D Glasses & Sega Light Phaser */
  {0xFBE5CFBB, 1, 0, SYSTEM_LIGHTPHASER, MAPPER_SEGA,          REGION_USA}, /* Missile Defense 3D */
  {0xE79BB689, 1, 0, SYSTEM_LIGHTPHASER, MAPPER_SEGA,          REGION_USA}, /* Missile Defense 3D [BIOS] */

  /* games requiring Sega Light Phaser */
  {0x861B6E79, 0, 0, SYSTEM_LIGHTPHASER, MAPPER_SEGA,          REGION_USA}, /* Assault City [Light Phaser] */
  {0x5FC74D2A, 0, 0, SYSTEM_LIGHTPHASER, MAPPER_SEGA,          REGION_USA}, /* Gangster Town */
  {0xE167A561, 0, 0, SYSTEM_LIGHTPHASER, MAPPER_SEGA,          REGION_USA}, /* Hang-On / Safari Hunt */
  {0xC5083000, 0, 0, SYSTEM_LIGHTPHASER, MAPPER_SEGA,          REGION_USA}, /* Hang-On / Safari Hunt [BAD DUMP] */
  {0x91E93385, 0, 0, SYSTEM_LIGHTPHASER, MAPPER_SEGA,          REGION_USA}, /* Hang-On / Safari Hunt [BIOS] */
  {0xE8EA842C, 0, 0, SYSTEM_LIGHTPHASER, MAPPER_SEGA,          REGION_USA}, /* Marksman Shooting / Trap Shooting */
  {0xE8215C2E, 0, 0, SYSTEM_LIGHTPHASER, MAPPER_SEGA,          REGION_USA}, /* Marksman Shooting / Trap Shooting / Safari Hunt */
  {0x205CAAE8, 0, 0, SYSTEM_LIGHTPHASER, MAPPER_SEGA,          REGION_USA}, /* Operation Wolf (can be played with gamepad in port B)*/
  {0x23283F37, 0, 0, SYSTEM_LIGHTPHASER, MAPPER_SEGA,          REGION_USA}, /* Operation Wolf [A] (can be played with gamepad in port B) */
  {0xDA5A7013, 0, 0, SYSTEM_LIGHTPHASER, MAPPER_SEGA,          REGION_USA}, /* Rambo 3 */
  {0x79AC8E7F, 0, 0, SYSTEM_LIGHTPHASER, MAPPER_SEGA,          REGION_USA}, /* Rescue Mission */
  {0x4B051022, 0, 0, SYSTEM_LIGHTPHASER, MAPPER_SEGA,          REGION_USA}, /* Shooting Gallery */
  {0xA908CFF5, 0, 0, SYSTEM_LIGHTPHASER, MAPPER_SEGA,          REGION_USA}, /* Spacegun */
  {0x5359762D, 0, 0, SYSTEM_LIGHTPHASER, MAPPER_SEGA,          REGION_USA}, /* Wanted */
  {0x0CA95637, 0, 0, SYSTEM_LIGHTPHASER, MAPPER_SEGA,          REGION_USA}, /* Laser Ghost */

  /* games requiring Sega Paddle */
  {0xF9DBB533, 0, 0, SYSTEM_PADDLE, MAPPER_SEGA,        REGION_JAPAN_NTSC}, /* Alex Kidd BMX Trial */
  {0xA6FA42D0, 0, 0, SYSTEM_PADDLE, MAPPER_SEGA,        REGION_JAPAN_NTSC}, /* Galactic Protector */
  {0x29BC7FAD, 0, 0, SYSTEM_PADDLE, MAPPER_SEGA,        REGION_JAPAN_NTSC}, /* Megumi Rescue */
  {0x315917D4, 0, 0, SYSTEM_PADDLE, MAPPER_SEGA,        REGION_JAPAN_NTSC}, /* Woody Pop */

    /* games requiring Sega Sport Pad */
  {0x0CB7E21F, 0, 0, SYSTEM_SPORTSPAD, MAPPER_SEGA,            REGION_USA}, /* Great Ice Hockey */
  {0xE42E4998, 0, 0, SYSTEM_SPORTSPAD, MAPPER_SEGA,            REGION_USA}, /* Sports Pad Football */
  {0x41C948BF, 0, 0, SYSTEM_SPORTSPAD, MAPPER_SEGA,            REGION_USA},  /* Sports Pad Soccer */

  /* other */
  {0x6BD5C2BF, 1, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Space Harrier 3-D */
  {0x8ECD201C, 1, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Blade Eagle 3-D */
  {0xFBF96C81, 1, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Blade Eagle 3-D (BR) */
  {0x58D5FC48, 1, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Blade Eagle 3-D [Proto] */
  {0x31B8040B, 1, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Maze Hunter 3-D */
  {0xABD48AD2, 1, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Poseidon Wars 3-D */
  {0xA3EF13CB, 1, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Zaxxon 3-D */
  {0xBBA74147, 1, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Zaxxon 3-D [Proto] */
  {0xD6F43DDA, 1, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Out Run 3-D */
  {0x871562b0, 1, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Maze Walker */
  {0x156948f9, 1, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Space Harrier 3-D (J) */
  {0x32759751, 0, 1, SYSTEM_MS_GAMEPAD, MAPPER_SEGA,    REGION_JAPAN_NTSC}, /* Y's (J) */
  {0x1C951F8E, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* After Burner */
  {0xC13896D5, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Alex Kidd: The Lost Stars */
  {0x5CBFE997, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Alien Syndrome */
  {0xBBA2FE98, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Altered Beast */
  {0xFF614EB3, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Aztec Adventure */
  {0x3084CF11, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Bomber Raid */
  {0xAC6009A7, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* California Games */
  {0xA4852757, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Captain Silver */
  {0xB81F6FA5, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Captain Silver (U) */
  {0x3CFF6E80, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Casino Games */
  {0xE7F62E6D, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Cloud Master */
  {0x908E7524, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Cyborg Hunter */
  {0xA55D89F3, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Double Dragon */
  {0xB8B141F9, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Fantasy Zone II */
  {0xD29889AD, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Fantasy Zone: The Maze */
  {0xA4AC35D8, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Galaxy Force */
  {0x6C827520, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Galaxy Force (U) */
  {0x1890F407, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Game Box Série Esportes Radicais (BR) */
  {0xB746A6F5, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Global Defense */
  {0x91A0FC4E, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Global Defense [Proto] */
  {0x48651325, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Golfamania */
  {0x5DABFDC3, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Golfamania [Proto] */
  {0xA51376FE, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Golvellius - Valley of Doom */
  {0x98E4AE4A, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Great Golf */
  {0x516ED32E, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Kenseiden */
  {0xE8511B08, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Lord of The Sword */
  {0x0E333B6E, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Miracle Warriors - Seal of The Dark Lord */
  {0x301A59AA, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Miracle Warriors - Seal of The Dark Lord [Proto] */
  {0x01D67C0B, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Mônica no Castelo do Dragão (BR) */
  {0x5589D8D2, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Out Run */
  {0xE030E66C, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Parlour Games */
  {0xF97E9875, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Penguin Land */
  {0x4077EFD9, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Power Strike */
  {0xBB54B6B0, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* R-Type */
  {0x42FC47EE, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Rampage */
  {0xC547EB1B, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Rastan */
  {0x9A8B28EC, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Scramble Spirits */
  {0xAAB67EC3, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Shanghai */
  {0x0C6FAC4E, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Shinobi */
  {0x4752CAE7, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* SpellCaster */
  {0x1A390B93, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Tennis Ace */
  {0xAE920E4B, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Thunder Blade */
  {0x51BD14BE, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Time Soldiers */
  {0x22CCA9BB, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Turma da Mônica em: O Resgate (BR) */
  {0xB52D60C8, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Ultima IV */
  {0xDE9F8517, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Ultima IV [Proto] */
  {0xDFB0B161, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Vigilante */
  {0x679E1676, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Wonder Boy III: The Dragon's Trap */
  {0x8CBEF0C1, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Wonder Boy in Monster Land */
  {0x2F2E3BC9, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_USA}, /* Zillion II - The Tri Formation */
  {0x48D44A13, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_NONE,   REGION_JAPAN_NTSC}, /* BIOS (J) */
  {0xD8C4165B, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Aleste */
  {0x4CC11DF9, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Alien Syndrome (J) */
  {0xE421E466, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Chouon Senshi Borgman */
  {0x2BCDB8FA, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Doki Doki Penguin Land - Uchuu-Daibouken */
  {0x56BD2455, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Doki Doki Penguin Land - Uchuu-Daibouken [Proto] */
  {0xC722FB42, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Fantasy Zone II (J) */
  {0x7ABC70E9, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Family Games (Party Games) */
  {0x6586BD1F, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Masters Golf */
  {0x4847BC91, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Masters Golf [Proto] */
  {0xB9FDF6D9, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Haja no Fuuin */
  {0x955A009E, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Hoshi wo Sagashite */
  {0x05EA5353, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Kenseiden (J) */
  {0xD11D32E4, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Kujakuou */
  {0xAA7D6F45, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Lord of Sword */
  {0xBF0411AD, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Maou Golvellius */
  {0x21A21352, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Maou Golvellius [Proto] */
  {0x5B5F9106, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Nekyuu Kousien */
  {0xBEA27D5C, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Opa Opa */
  {0x6605D36A, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Phantasy Star (J) */
  {0xE1FFF1BB, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Shinobi (J) */
  {0x11645549, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Solomon no Kagi - Oujo Rihita no Namida */
  {0x7E0EF8CB, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Super Racing */
  {0xB1DA6A30, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Super Wonder Boy Monster World */
  {0x8132AB2C, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Tensai Bakabon */
  {0xC0CE19B1, 0, 1, SYSTEM_MS_GAMEPAD,  MAPPER_SEGA,   REGION_JAPAN_NTSC}, /* Thunder Blade (J) */
};

/* 1K trash buffer */
static uint8 dummy[0x400];

/* Function prorotypes */
static void mapper_8k_w(int offset, unsigned int data);
static void mapper_16k_w(int offset, unsigned int data);
static void write_mapper_none(unsigned int address, unsigned char data);
static void write_mapper_sega(unsigned int address, unsigned char data);
static void write_mapper_codies(unsigned int address, unsigned char data);
static void write_mapper_korea(unsigned int address, unsigned char data);
static void write_mapper_msx(unsigned int address, unsigned char data);


void sms_cart_init(void)
{
  /* default mapper */
  slot.mapper = MAPPER_SEGA;

  /* default supported peripheral  */
  uint8 device = SYSTEM_MS_GAMEPAD;
  cart.special = 0;

  /* compute CRC */
  uint32 crc = crc32(0, cart.rom, cart.romsize);

  /* detect cartridge mapper */
  int i;
  for (i=0; i<GAME_DATABASE_CNT; i++)
  {
    if (crc == game_list[i].crc)
    {
      cart.special = game_list[i].glasses_3d;
      slot.mapper = game_list[i].mapper;
      device = game_list[i].peripheral;
      i = GAME_DATABASE_CNT;
    }
  }

  /* initialize Z80 write handler */
  switch(slot.mapper)
  {
    case MAPPER_NONE:
      z80_writemem = write_mapper_none;
      break;

    case MAPPER_CODIES:
      z80_writemem = write_mapper_codies;
      break;

    case MAPPER_KOREA:
      z80_writemem = write_mapper_korea;
      break;

    case MAPPER_MSX:
      z80_writemem = write_mapper_msx;
      break;

    default:
      z80_writemem = write_mapper_sega;
      break;
  }

  /* initialize default SRAM (32K max.) */
  sram_init();

  /* restore previous input settings */
  if (old_system[0] != -1)
  {
    input.system[0] = old_system[0];
  }
  if (old_system[1] != -1)
  {
    input.system[1] = old_system[1];
  }

  /* default gun offset */
  input.x_offset = 20; 
  input.y_offset = 0; 

  /* detect if game requires specific peripheral */
  if (device != SYSTEM_MS_GAMEPAD)
  {
    /* save port A setting */
    if (old_system[0] == -1)
    {
      old_system[0] = input.system[0];
    }

    /* force port A configuration */
    input.system[0] = device;

    /* SpaceGun & Gangster Town use different gun offset */
    if ((crc == 0x5359762D) || (crc == 0x5FC74D2A))
    {
      input.x_offset = 16;
    }
  }
}

void sms_cart_reset(void)
{
  int i;

  /* Unmapped memory return $FF */
  memset(dummy, 0xFF, 0x400);

  /* Reset Z80 memory mapping at $0000-$BFFF (first 32k of ROM mirrored) */
  for(i = 0x00; i < 0x30; i++)
  {
    z80_readmap[i] = &cart.rom[(i & 0x1F) << 10];
    z80_writemap[i] = dummy;
  }

  /* Reset Z80 memory mapping at $C000-$FFFF (first 8K of 68k RAM mirrored) */
  for(i = 0x30; i < 0x40; i++)
  {
    z80_readmap[i] = z80_writemap[i] = &work_ram[(i & 0x07) << 10];
  }

  /* Reset cartridge paging registers */
  switch(slot.mapper)
  {
    case MAPPER_NONE:
    case MAPPER_SEGA:
    {
      slot.fcr[0] = 0;
      slot.fcr[1] = 0;
      slot.fcr[2] = 1;
      slot.fcr[3] = 2;
      break;
    }

    default:
    {
      slot.fcr[0] = 0;
      slot.fcr[1] = 0;
      slot.fcr[2] = 1;
      slot.fcr[3] = 0;
      break;
    }
  }

  /* Set default memory map */
  if (slot.mapper != MAPPER_MSX)
  {
    mapper_16k_w(0,slot.fcr[0]);
    mapper_16k_w(1,slot.fcr[1]);
    mapper_16k_w(2,slot.fcr[2]);
    mapper_16k_w(3,slot.fcr[3]);
  }
  else
  {
    mapper_8k_w(0,slot.fcr[0]);
    mapper_8k_w(1,slot.fcr[1]);
    mapper_8k_w(2,slot.fcr[2]);
    mapper_8k_w(3,slot.fcr[3]);
  }
}

 void sms_cart_switch(int enabled)
 {
  int i;

  if (enabled)
  {
    /* Enable cartdige ROM at $0000-$BFFF */
    for(i = 0x00; i < 0x30; i++)
    {
      z80_readmap[i] = &cart.rom[(i & 0x1F) << 10];
      z80_writemap[i] = dummy;
    }
  }
  else
  {
    /* Disable cartridge ROM at $0000-$BFFF */
    for(i = 0x00; i < 0x30; i++)
    {
      z80_readmap[i] = z80_writemap[i] = dummy;
    }
  }
}

int sms_cart_region_detect(void)
{
  /* compute CRC */
  uint32 crc = crc32(0, cart.rom, cart.romsize);

  /* Turma da Mônica em: O Resgate & Wonder Boy III enable FM support on japanese hardware only */
  if (config.ym2413_enabled && ((crc == 0x22CCA9BB) || (crc == 0x679E1676)))
  {
    return REGION_JAPAN_NTSC;
  }

  /* detect game region */
  int i;
  for (i=0; i<GAME_DATABASE_CNT; i++)
  {
    if (crc == game_list[i].crc)
    {
      return game_list[i].region;
    }
  }

  /* default region */
  return REGION_USA;
}

int sms_cart_context_save(uint8 *state)
{
  int bufferptr = 0;
  save_param(slot.fcr, sizeof(slot.fcr));
  return bufferptr;
}

int sms_cart_context_load(uint8 *state)
{
  int bufferptr = 0;
  load_param(slot.fcr, sizeof(slot.fcr));

  /* Set default memory map */
  if (slot.mapper != MAPPER_MSX)
  {
    mapper_16k_w(0,slot.fcr[0]);
    mapper_16k_w(1,slot.fcr[1]);
    mapper_16k_w(2,slot.fcr[2]);
    mapper_16k_w(3,slot.fcr[3]);
  }
  else
  {
    mapper_8k_w(0,slot.fcr[0]);
    mapper_8k_w(1,slot.fcr[1]);
    mapper_8k_w(2,slot.fcr[2]);
    mapper_8k_w(3,slot.fcr[3]);
  }

  return bufferptr;
}


void mapper_8k_w(int offset, unsigned int data)
{
  int i;

  /* cartridge ROM page (8k) */
  uint8 page = data % (cart.romsize >> 13);
  
  /* Save frame control register data */
  slot.fcr[offset] = data;

  /* 4 x 8k banks */
  switch (offset & 3)
  {
    case 0: /* cartridge ROM bank (8k) at $8000-$9FFF */
    {
      for(i = 0x20; i < 0x28; i++)
      {
        z80_readmap[i] = &cart.rom[(page << 13) | ((i & 0x07) << 10)];
      }
      break;
    }
    
    case 1: /* cartridge ROM bank (8k) at $A000-$BFFF */
    {
      for(i = 0x28; i < 0x30; i++)
      {
        z80_readmap[i] = &cart.rom[(page << 13) | ((i & 0x07) << 10)];
      }
      break;
    }
    
    case 2: /* cartridge ROM bank (8k) at $4000-$5FFF */
    {
      for(i = 0x10; i < 0x18; i++)
      {
        z80_readmap[i] = &cart.rom[(page << 13) | ((i & 0x07) << 10)];
      }
      break;
    }
    
    case 3: /* cartridge ROM bank (8k) at $6000-$7FFF */
    {
      for(i = 0x18; i < 0x20; i++)
      {
        z80_readmap[i] = &cart.rom[(page << 13) | ((i & 0x07) << 10)];
      }
      break;
    }
  }
}
    
void mapper_16k_w(int offset, unsigned int data)
{
  int i;

  /* cartridge ROM page (16k) */
  uint8 page = data % (cart.romsize >> 14);
  
  /* page index increment (SEGA mapper) */
  if (slot.fcr[0] & 0x03)
  {
    page = (page + ((4 - (slot.fcr[0] & 0x03)) << 3)) % (cart.romsize >> 14);
  }

  /* save frame control register data */
  slot.fcr[offset] = data;

  switch (offset)
  {
    case 0: /* control register (SEGA mapper) */
    {
      if(data & 0x08)
      {
        /* external RAM (upper or lower 16K) mapped at $8000-$BFFF */
        for(i = 0x20; i <= 0x2F; i++)
        {
          z80_readmap[i] = z80_writemap[i] = &sram.sram[((data & 0x04) << 12) + ((i & 0x0F) << 10)];
        }
      }
      else
      {
        /* cartridge ROM page (16k) */
        page = slot.fcr[3] % (cart.romsize >> 14);
        
        /* page index increment (SEGA mapper) */
        if (data & 0x03)
        {
          page = (page + ((4 - (data & 0x03)) << 3)) % (cart.romsize >> 14);
        }

        /* cartridge ROM mapped at $8000-$BFFF */
        for(i = 0x20; i < 0x30; i++)
        {
          z80_readmap[i] = &cart.rom[(page << 14) | ((i & 0x0F) << 10)];
          z80_writemap[i] = dummy;
        }
      }

      if(data & 0x10)
      {
        /* external RAM (lower 16K) mapped at $C000-$FFFF */
        for(i = 0x30; i < 0x40; i++)
        {
          z80_readmap[i] = z80_writemap[i] = &sram.sram[(i & 0x0F) << 10];
        }
      }
      else
      {
        /* internal RAM (8K mirrorred) mapped at $C000-$FFFF */
        for(i = 0x30; i < 0x40; i++)
        {
          z80_readmap[i] = z80_writemap[i] = &work_ram[(i & 0x07) << 10];
        }
      }
      break;
    }

    case 1: /* cartridge ROM bank (16k) at $0000-$3FFF */
    {
      /* first 1k is not fixed (CODEMASTER mapper) */
      if (slot.mapper == MAPPER_CODIES)
      {
        z80_readmap[0] = &cart.rom[(page << 14)];
      }

      for(i = 0x01; i < 0x10; i++)
      {
        z80_readmap[i] = &cart.rom[(page << 14) | ((i & 0x0F) << 10)];
      }
      break;
    }

    case 2: /* cartridge ROM bank (16k) at $4000-$7FFF */
    {
      for(i = 0x10; i < 0x20; i++)
      {
        z80_readmap[i] = &cart.rom[(page << 14) | ((i & 0x0F) << 10)];
      }

      /* Ernie Elf's Golf external RAM switch */
      if (slot.mapper == MAPPER_CODIES)
      {
        if (data & 0x80)
        {
          /* external RAM (8k) mapped at $A000-$BFFF */
          for(i = 0x28; i < 0x30; i++)
          {
            z80_readmap[i] = z80_writemap[i] = &sram.sram[(i & 0x0F) << 10];
          }
        }
        else
        {
          /* cartridge ROM page (16k) */
          page = slot.fcr[3] % (cart.romsize >> 14);

          /* cartridge ROM mapped at $A000-$BFFF */
          for(i = 0x28; i < 0x30; i++)
          {
            z80_readmap[i] = &cart.rom[(page << 14) | ((i & 0x0F) << 10)];
            z80_writemap[i] = dummy;
          }
        }
      }
      break;
    }

    case 3: /* cartridge ROM bank (16k) at $8000-$BFFF */
    {
      /* check that external RAM (16k) is not mapped at $8000-$BFFF (SEGA mapper) */
      if ((slot.fcr[0] & 0x08)) break;

      /* first 8k */
      for(i = 0x20; i < 0x28; i++)
      {
        z80_readmap[i] = &cart.rom[(page << 14) | ((i & 0x0F) << 10)];
      }

      /* check that external RAM (8k) is not mapped at $A000-$BFFF (CODEMASTER mapper) */
      if ((slot.mapper == MAPPER_CODIES) && (slot.fcr[2] & 0x80)) break;

      /* last 8k */
      for(i = 0x28; i < 0x30; i++)
      {
        z80_readmap[i] = &cart.rom[(page << 14) | ((i & 0x0F) << 10)];
      }
      break;
    }
  }
}

static void write_mapper_none(unsigned int address, unsigned char data)
{
  z80_writemap[address >> 10][address & 0x03FF] = data;
}

static void write_mapper_sega(unsigned int address, unsigned char data)
{
  if(address >= 0xFFFC)
  {
    mapper_16k_w(address & 3, data);
  }

  z80_writemap[address >> 10][address & 0x03FF] = data;
}

static void write_mapper_codies(unsigned int address, unsigned char data)
{
  if (address == 0x0000)
  {
    mapper_16k_w(1,data);
    return;
  }

  if (address == 0x4000)
  {
    mapper_16k_w(2,data);
    return;
  }

  if (address == 0x8000)
  {
    mapper_16k_w(3,data);
    return;
  }

  z80_writemap[address >> 10][address & 0x03FF] = data;
}

static void write_mapper_korea(unsigned int address, unsigned char data)
{
  if (address == 0xA000)
  {
    mapper_16k_w(3,data);
    return;
  }

  z80_writemap[address >> 10][address & 0x03FF] = data;
}

static void write_mapper_msx(unsigned int address, unsigned char data)
{
  if (address <= 0x0003)
  {
    mapper_8k_w(address,data);
    return;
  }

  z80_writemap[address >> 10][address & 0x03FF] = data;
}
