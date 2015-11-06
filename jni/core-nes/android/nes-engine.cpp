#define NES_DISP_WIDTH		256
#define NES_DISP_HEIGHT		240
#define NES_SOUND_RATE		32050
#define NES_SOUND_TWEAK		2
#define BIT(b)				(1 << b)
#define NES_HEADER_MAGIC	(0x1A53454E)

extern "C"
{
#include "driver.h"
#include "state.h"
#include "fceu.h"
#include "ppu.h"
#include "fds.h"
#include "input.h"
#include "cart.h"
#include "ines.h"
#include "palette.h"
#include "sound.h"
#include "x6502.h"
#include "cheat.h"
}

#include <jni.h>
#include <android/log.h>
#include <stdlib.h>
#include <stdio.h>

#include "logging.h"
#include "retronCommon.h"

int PPUViewScanline=0;
int PPUViewer=0;

extern FCEUGI *FCEUGameInfo;
extern uint8 *XBuf;
extern CartInfo iNESCart;
extern CartInfo UNIFCart;
extern int FamiMic;
extern uint32 iNESGameCRC32;

static uint16_t palette[256];
void mallocInit(t_emuAllocators *allocators);

class NESEngine : public cEmulatorPlugin {
public:
	NESEngine();
	virtual ~NESEngine();

	virtual bool initialise(t_pluginInfo *info);
	virtual void destroy();
	virtual void reset();
	virtual t_romInfo *loadRomFile(const char *file, t_systemRegion systemRegion);
	virtual void unloadRom();
	virtual bool isNvmDirty();
	virtual int saveNvm(const char *file);
	virtual int saveNvmBuffer(void **buffer, int *size);
	virtual bool loadNvm(const char *file);
	virtual bool saveSnapshot(const char *file);
	virtual bool loadSnapshot(const char *file);
	virtual void runFrame(cEmuBitmap *curBitmap, t_emuInputState ctrlState, short *soundBuffer, int *soundSampleByteCount);
	virtual bool setOption(const char *name, const char *value);
	virtual bool addCheat(const char *cheat);
	virtual bool removeCheat(const char *cheat);
	virtual void resetCheats();

private:

	const static int ADC_HISTORY_SIZE = 3;

	uint32 crc32(uint32 crc, uint8 *buf, int len);
	void updateCRCs();

	static uint32 crc32Tab[256];

	bool mDisplayOverscan;
	bool mVausFilter;
	bool mFdsSwitchPending, mNesMicPending;
	bool mForceShowOverscan;
	uint32 mSaveCRC[4];
	bool mNvmDirty;
	uint8 mJoypad[4];
	uint8 mFKBKeys[9];
	uint8 mHypershot;
	uint32 mArkanoidState[3];
	uint8 mAdcHistory[ADC_HISTORY_SIZE];
	uint32 mPowerPadState;
	t_romInfo mRomInfo;
};

NESEngine::NESEngine()
{
	mDisplayOverscan = false;
	mVausFilter = false;
	mFdsSwitchPending = false;
	mNesMicPending = false;
	mForceShowOverscan = false;
	memset(mJoypad, 0, sizeof(mJoypad));
	memset(mFKBKeys, 0, sizeof(mFKBKeys));
	memset(mArkanoidState, 0, sizeof(mArkanoidState));
	memset(mAdcHistory, 0, sizeof(mAdcHistory));
	mHypershot = 0;
	mPowerPadState = 0;
	mSaveCRC[0] = mSaveCRC[1] = mSaveCRC[2] = mSaveCRC[3] = -1;
	mNvmDirty = false;
}

NESEngine::~NESEngine()
{

}

bool NESEngine::initialise(t_pluginInfo *info)
{
	if(!FCEUI_Initialize())
	{
		LOGE("FCEUI_Initialize failed\n");
		return false;
	}
	FCEUI_SetSoundVolume(256);
	FCEUI_Sound(NES_SOUND_RATE+NES_SOUND_TWEAK);

	info->maxWidth = 256;
	info->maxHeight = 256;
	info->bitmapPitch = 256 * 2;

	return true;
}

void NESEngine::destroy()
{

}

void NESEngine::reset()
{
	ResetNES();
}

t_romInfo *NESEngine::loadRomFile(const char *file, t_systemRegion systemRegion)
{
	int usePAL = 0;

	switch(systemRegion)
	{
	case SYS_REGION_JAP:
	case SYS_REGION_USA:
		usePAL = 0;
		break;
	case SYS_REGION_EUR:
		usePAL = 1;
		break;
	// REGION_AUTO
	default:
		{
			uint8_t header[16];
			FILE *fp = fopen(file, "rb");
			if(fp)
			{
				fread(header, 1, 16, fp);
				fclose(fp);
				if((*(uint32_t *)&header[0] == NES_HEADER_MAGIC) && (header[9] & 0x01))
				{
					LOGI("auto-set NES to PAL based on header flag\n");
					usePAL = 1;
				}
			}
		}
		break;
	}

	memset(&mRomInfo, 0, sizeof(t_romInfo));

	LOGI("loading ROM: %s\n", file);

	FCEUI_SetVidSystem(usePAL);
	FCEUGameInfo = FCEUI_LoadGame(file);
	if(!FCEUGameInfo)
	{
		LOGE("FCEUI_LoadGame failed");
		return NULL;
	}

	// first map defaults
	int defInput[2] = { FCEUGameInfo->input[0], FCEUGameInfo->input[1] };
	LOGI("NES input: %d, %d, %d\n", FCEUGameInfo->input[0], FCEUGameInfo->input[1], FCEUGameInfo->inputfc);
	FCEUI_SetInput(0, SI_GAMEPAD, mJoypad, 0);
	FCEUI_SetInput(1, SI_GAMEPAD, mJoypad, 0);

	// then take care of special FC input devices
	if(FCEUGameInfo->inputfc == SIFC_FKB)
	{
		FCEUI_SetInputFC(SIFC_FKB, mFKBKeys, 0);
	}
	else if(FCEUGameInfo->inputfc == SIFC_HYPERSHOT)
	{
		FCEUI_SetInputFC(SIFC_HYPERSHOT, &mHypershot, 0);
	}
	else if(FCEUGameInfo->inputfc == SIFC_ARKANOID)
	{
		FCEUI_SetInputFC(SIFC_ARKANOID, mArkanoidState, 0);
	}

	// then special NES input devices
	if(defInput[1] == SI_ARKANOID)
	{
		FCEUI_SetInput(1, SI_ARKANOID, mArkanoidState, 0);
	}
	else if(defInput[1] == SI_POWERPADB)
	{
		FCEUI_SetInput(1, SI_POWERPADB, &mPowerPadState, 0);
	}

	mRomInfo.fps = (PAL) ? 838977920.0 / 16777215.0 : 1008307711.0 / 16777215.0;
	mRomInfo.aspectRatio = 4.0 / 3.0;
	mRomInfo.soundRate = NES_SOUND_RATE;
	mRomInfo.soundMaxBytesPerFrame = ((mRomInfo.soundRate / 50) + 1) * 2 * sizeof(short);

	if(PAL)
	{
		LOGI("NES rom video mode: PAL\n");
	}
	else
	{
		LOGI("NES rom video mode: NTSC\n");
	}

	if(FCEUGameInfo->type == GIT_FDS)
	{
		uint8_t forceOverscanMD5[][16] = {
			{ 0xE7, 0x38, 0x8F, 0x78, 0x26, 0x4C, 0x28, 0x2B, 0xB3, 0xE9, 0xA3, 0x47, 0x1E, 0x0D, 0xB3, 0xAC }, // Yuu Maze
		};
		for(int i = 0; i < sizeof(forceOverscanMD5)/16; i++)
			if(!memcmp(&forceOverscanMD5[i][0], FCEUGameInfo->MD5, 16))
			{
				mForceShowOverscan = true;
				break;
			}
	}
	else
	{
		switch(iNESGameCRC32)
		{
		// Arkanoid II
		case 0x20F98F2A:
		case 0x0F141525:
		case 0xFC8DEBEF:
		// Chiisana Obake - Acchi Socchi Kocchi (J)
		case 0x5DEC84F8:
		// Kero Kero Keroppi no Dai Bouken (J)
		case 0xEB465156:
		// Ninjara Hoi! (J)
		case 0xCEE5857B:
		// Gekikame Ninja Den (J)
		case 0x64A02715:
		// Super Black Onyx (J)
		case 0xDFC0CE21:
		// Sword Master (J)
		case 0xDF3776C6:
			mForceShowOverscan = true;
			break;
		}
	}

	return &mRomInfo;
}

void NESEngine::unloadRom()
{
	FCEUI_CloseGame();
	FCEUI_Kill();
}

bool NESEngine::isNvmDirty()
{
	if(mNvmDirty)
		return true;

	// As the CRC is quite slow, only process once per 30 frames
	static int frameCounter = 0;
	if(frameCounter++ % 30)
		return false;

	for(int i = 0; i < 4; i++)
		if(iNESCart.SaveGame[i])
			if(mSaveCRC[i] != crc32(0, iNESCart.SaveGame[i], iNESCart.SaveGameLen[i]))
			{
				LOGI("SaveGame %d changed\n", i);
				mNvmDirty = true;
			}

	return mNvmDirty;
}

void NESEngine::updateCRCs()
{
	for(int i = 0; i < 4; i++)
		if(iNESCart.SaveGame[i])
			mSaveCRC[i] = crc32(0, iNESCart.SaveGame[i], iNESCart.SaveGameLen[i]);
}

int NESEngine::saveNvm(const char *file)
{
	int rv = FCEU_SaveGameSave2(file, &iNESCart);
	if(rv <= 0)
		return rv;
	updateCRCs();
	mNvmDirty = false;
	return 1;
}

bool NESEngine::loadNvm(const char *file)
{
	if(FCEU_LoadGameSave2(file, &iNESCart) < 0)
		return false;
	updateCRCs();
	mNvmDirty = false;
	return true;
}

int NESEngine::saveNvmBuffer(void **buffer, int *size)
{
	int rv = FCEU_SaveGameSaveBuffer2(buffer, size, &iNESCart);
	if(rv <= 0)
		return rv;
	updateCRCs();
	mNvmDirty = false;
	return 1;
}

bool NESEngine::saveSnapshot(const char *file)
{
	if(!FCEUI_SaveState((char *)file))
	{
		LOGI("failed to save nes snapshot: %s\n", file);
		return false;
	}
	return true;
}

bool NESEngine::loadSnapshot(const char *file)
{
	if(!FCEUI_LoadState((char *)file))
	{
		LOGI("failed to load nes snapshot: %s\n", file);
		return false;
	}
	return true;
}

void NESEngine::runFrame(cEmuBitmap *curBitmap, t_emuInputState ctrlState, short *soundBuffer, int *soundSampleByteCount)
{
	static int latchedPadState = 0, fdsCycleCounter = 0;
	unsigned y, x, width, height, xoff, yoff;
	uint8_t *gfx;
	int32 *sound = 0;
	int32 ssize;

	// FDS disk swapping logic
	if(FCEUGameInfo->type == GIT_FDS && mFdsSwitchPending && fdsCycleCounter == 0)
	{
		LOGI("got FDS swap side cmd\n");
		mFdsSwitchPending = false;
		fdsCycleCounter = 120;
		FCEU_FDSInsert(0);
	}
	else if(fdsCycleCounter == 110)
	{
		FCEU_FDSSelect();
	}
	else if(fdsCycleCounter == 1)
	{
		FCEU_FDSInsert(0);
	}
	if(fdsCycleCounter > 0) fdsCycleCounter--;
	latchedPadState = ctrlState.padState[0];

	memset(&mFKBKeys, 0, sizeof(mFKBKeys));
	for(int i = 0; i < ctrlState.specialStateNum; i++)
	{
		if(ctrlState.specialStates[i]->type == INPUT_FAMI_KEYBOARD && ctrlState.specialStates[i]->stateLen == sizeof(t_emuInputFamiKeyboardState))
		{
			t_emuInputFamiKeyboardState *state = (t_emuInputFamiKeyboardState *)ctrlState.specialStates[i];
			memcpy(mFKBKeys, state->data, sizeof(state->data));
			break;
		}
		else if(ctrlState.specialStates[i]->type == INPUT_RAW_PORTS && ctrlState.specialStates[i]->stateLen == sizeof(t_emuInputRawPortState))
		{
			t_emuInputRawPortState *state = (t_emuInputRawPortState *)ctrlState.specialStates[i];
			if(FCEUGameInfo->inputfc == SIFC_HYPERSHOT)
			{
				mHypershot = 0;
				if(!(state->famiRawState & 0x02))
					mHypershot |= 0x01;
				if(!(state->famiRawState & 0x04))
					mHypershot |= 0x02;
				if(!(state->famiRawState & 0x08))
					mHypershot |= 0x04;
				if(!(state->famiRawState & 0x10))
					mHypershot |= 0x08;
			}
			else if(FCEUGameInfo->inputfc == SIFC_ARKANOID || FCEUGameInfo->input[1] == SI_ARKANOID)
			{
				uint8_t thisADC = state->famiP2D1Strobed;

				if(mVausFilter)
				{
					// shift FIFO
					for(int i = 0; i < (ADC_HISTORY_SIZE - 1); i++)
						mAdcHistory[i] = mAdcHistory[i+1];
					mAdcHistory[ADC_HISTORY_SIZE-1] = thisADC;

					// account for wrapping
					if(thisADC < 0x10 || thisADC > 0xf0)
						memset(mAdcHistory, thisADC, sizeof(mAdcHistory));

					// calculate average
					uint32_t adcAverage = 0;
					for(int i = 0; i < ADC_HISTORY_SIZE; i++)
						adcAverage += mAdcHistory[i];
					adcAverage /= ADC_HISTORY_SIZE;

					mArkanoidState[0] = adcAverage;
				}
				else
					mArkanoidState[0] = thisADC;

				mArkanoidState[2] = state->famiP1D1Strobed;
//				LOGI("arkanoid state:%02X/%02X %02X\n", mArkanoidState[0], thisADC, mArkanoidState[2]);
			}
			else if(FCEUGameInfo->input[1] == SI_POWERPADB)
			{
				mPowerPadState = 0;
				if(state->nesD2Strobed & 0x40)
					mPowerPadState |= (1 << 0);
				if(state->nesD2Strobed & 0x80)
					mPowerPadState |= (1 << 1);
				if(state->nesD3Strobed & 0x40)
					mPowerPadState |= (1 << 2);
				if(state->nesD3Strobed & 0x80)
					mPowerPadState |= (1 << 3);
				if(state->nesD2Strobed & 0x20)
					mPowerPadState |= (1 << 4);
				if(state->nesD2Strobed & 0x08)
					mPowerPadState |= (1 << 5);
				if(state->nesD2Strobed & 0x01)
					mPowerPadState |= (1 << 6);
				if(state->nesD3Strobed & 0x10)
					mPowerPadState |= (1 << 7);
				if(state->nesD2Strobed & 0x10)
					mPowerPadState |= (1 << 8);
				if(state->nesD2Strobed & 0x04)
					mPowerPadState |= (1 << 9);
				if(state->nesD2Strobed & 0x02)
					mPowerPadState |= (1 << 10);
				if(state->nesD3Strobed & 0x20)
					mPowerPadState |= (1 << 11);
//				LOGI("mPowerPadState = %04X\n", mPowerPadState);
			}
		}
	}

	if(mNesMicPending)
	{
		FamiMic = 1;
		mNesMicPending = false;
	}
	else
		FamiMic = 0;

	int padCnt = 0;
	if(ctrlState.padConnectMask & ~3)
		padCnt = 4;
	else
		padCnt = 2;
	memset(mJoypad, 0, sizeof(mJoypad));

	for(int i = 0; i < padCnt; i++)
	{
	    if(ctrlState.padState[i] & BTN_SELECT)
	    	mJoypad[i] |= BIT(2);
	    if(ctrlState.padState[i] & BTN_START)
	    	mJoypad[i] |= BIT(3);
	    if(ctrlState.padState[i] & BTN_UP)
	    	mJoypad[i] |= BIT(4);
	    if(ctrlState.padState[i] & BTN_DOWN)
	    	mJoypad[i] |= BIT(5);
	    if(ctrlState.padState[i] & BTN_LEFT)
	    	mJoypad[i] |= BIT(6);
	    if(ctrlState.padState[i] & BTN_RIGHT)
	    	mJoypad[i] |= BIT(7);
	    if(ctrlState.padState[i] & BTN_BUTTON_1)
	    	mJoypad[i] |= BIT(0);
	    if(ctrlState.padState[i] & BTN_BUTTON_2)
	    	mJoypad[i] |= BIT(1);
	}

	ssize = 0;
	FCEUI_Emulate(&gfx, &sound, &ssize, 0);

	if(mDisplayOverscan || mForceShowOverscan)
	{
		// overscan shown
		width = 256;
		height = 240;
		xoff = 0;
		yoff = 0;
	}
	else
	{
		// overscan hidden
		width = 256 - 16;
		height = 240 - 16;
		xoff = 8;
		yoff = 8;
	}

    if(curBitmap)
    {
		uint16_t *dispOut = (uint16_t *)curBitmap->getBuffer();
		curBitmap->setDimensions(width, height);

		gfx = XBuf + xoff + 256 * yoff;
		for(int y = 0; y < NES_DISP_HEIGHT; y++)
			for(int x = 0; x < NES_DISP_WIDTH; x++, gfx++)
				dispOut[y * NES_DISP_WIDTH + x] = palette[*gfx];
    }

    if(soundBuffer)
    {
		// convert mono to stereo frames
		for(int i = 0; i < ssize; i++)
		{
			soundBuffer[(i*2)+0] = sound[i];
			soundBuffer[(i*2)+1] = sound[i];
		}
		*soundSampleByteCount = ssize * 2 * 2;
    }

#if 0
	// sound output verification
	{
		static int sampleCounter = 0;
		static int frameCounter = 0;

		sampleCounter += ssize;
		frameCounter++;
		if(frameCounter == 60)
		{
			LOGI("nes sound stats: %d\n", sampleCounter);
			sampleCounter = 0;
			frameCounter = 0;
		}
	}
#endif
}

bool getBoolFromString(const char *str)
{
	if(!strcasecmp(str, "true"))
		return true;
	else
		return false;
}

bool NESEngine::setOption(const char *name, const char *value)
{
	if(!strcasecmp(name, PLUGINOPT_OVERSCAN))
	{
		mDisplayOverscan = getBoolFromString(value);
		return true;
	}
	else if(!strcasecmp(name, PLUGINOPT_NES_ENABLE_VAUSFILTER))
	{
		mVausFilter = getBoolFromString(value);
		return true;
	}
	else if(!strcasecmp(name, PLUGINOPT_NES_FDS_SWITCH_SIDE))
	{
		mFdsSwitchPending = true;
		return true;
	}
	else if(!strcasecmp(name, PLUGINOPT_NES_MICROPHONE))
	{
		mNesMicPending = true;
		return true;
	}

	return false;
}

bool NESEngine::addCheat(const char *cheat)
{
	static const uint8_t parShiftTab[] = { 15, 3, 13, 14, 1, 6, 9, 5, 0, 12, 7, 2, 8, 10, 11, 4, 19, 21, 23, 22, 20, 17, 16, 18, 29, 31, 24, 26, 25, 30, 27, 28 };
	char *scheat = strdup(cheat), *s_format, *s_addr, *s_val;
	uint32_t addr;
	uint8_t val, cmp;
	bool ret = false;

	s_format = strtok((char *)scheat, ";");
	if(!s_format)
	{
		LOGE("cheat parse error 1\n");
		goto addCheat_end;
	}
	if(!strcasecmp(s_format, "raw"))
	{
		s_addr = strtok(NULL, ":");
		if(!s_addr)
		{
			LOGE("cheat parse error 2\n");
			goto addCheat_end;
		}
		s_val = strtok(NULL, ":");
		if(!s_val)
		{
			LOGE("cheat parse error 3\n");
			goto addCheat_end;
		}
		addr = strtol(s_addr, NULL, 16);
		val = strtol(s_val, NULL, 16);
		LOGI("raw code: %04X:%02X\n", addr, val);
		FCEUI_AddCheat("", addr, val, -1, (addr < 0x0100) ? 0 : 1);
	}
	else if(!strcasecmp(s_format, "par"))
	{
		s_addr = strtok(NULL, ":");
		if(!s_addr)
		{
			LOGE("cheat parse error 2\n");
			goto addCheat_end;
		}
		addr = strtoll(s_addr, NULL, 16);
//		LOGI("PAR enc: %08X\n", addr);

		uint32_t raw = 0;
		addr ^= 0xFCBDD275;
		for(int i = 31; i >= 0; i--)
		{
			if(addr & 0x80000000)
			{
				raw += 1 << parShiftTab[i];
				addr ^= 0xB8309722;
			}
			addr <<= 1;
		}
		raw |= 0x8000;
//		LOGI("dec: %08X\n", raw);

		val = raw >> 24;
		cmp = raw >> 16;
		addr = raw & 0xffff;
		LOGI("par code: %04X:%02X:%02X\n", addr, cmp, val);
		FCEUI_AddCheat("", addr, val, cmp, 1);
	}
	else
	{
		LOGE("unsupported format: %s\n", s_format);
		goto addCheat_end;
	}

	ret = true;

addCheat_end:
	free(scheat);
	return ret;
}

bool NESEngine::removeCheat(const char *cheat)
{
	return false;
}

void NESEngine::resetCheats()
{
	FCEU_FlushGameCheats(NULL, 1);
}

extern "C" void UpdatePPUView(int refreshchr) { }

extern "C" const char * GetKeyboard(void)
{
   return "";
}

extern "C" int FCEUD_SendData(void *data, uint32 len)
{
   return 1;
}

extern "C" bool FCEUD_ShouldDrawInputAids (void)
{
   return 1;
}

extern "C" void FCEUD_NetworkClose(void)
{ }

extern "C" void FCEUD_GetPalette(uint8 i,uint8 *r, uint8 *g, uint8 *b) { }

extern "C" void FCEUD_VideoChanged (void)
{ }

extern "C" FILE *FCEUD_UTF8fopen(const char *n, const char *m)
{
   return fopen(n, m);
}
#ifdef DEBUG
extern "C" void FCEU_printf(char *format, ...)
{
	char temp[2048];
	va_list ap;
	va_start(ap,format);
	vsnprintf(temp,sizeof(temp),format,ap);
	LOGI(temp);
	va_end(ap);
}

extern "C" void FCEU_PrintError(char *format, ...) 
{
	char temp[2048];
	va_list ap;
	va_start(ap, format);
	vsprintf(temp, format, ap);
	LOGE(temp);
	va_end(ap);
}
#endif
#define BUILD_PIXEL_RGB565(R,G,B) (((int) ((R)&0x1f) << 11) | ((int) ((G)&0x3f) << 5) | (int) ((B)&0x1f))
extern "C" void FCEUD_SetPalette(uint8 index, uint8 r, uint8 g, uint8 b)
{
	palette[index] = BUILD_PIXEL_RGB565(r >> 3, g >> 2, b >> 3);
}

extern "C" __attribute__((visibility("default")))
cEmulatorPlugin *createPlugin(t_emuAllocators *allocators)
{
	mallocInit(allocators);
	return new NESEngine;
}

uint32 NESEngine::crc32Tab[] = { /* CRC polynomial 0xedb88320 */
	0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
	0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
	0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
	0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
	0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
	0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
	0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
	0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
	0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
	0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
	0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
	0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
	0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
	0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
	0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
	0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
	0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
	0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
	0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
	0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
	0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
	0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
	0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
	0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
	0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
	0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
	0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
	0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
	0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
	0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
	0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
	0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
	0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
	0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
	0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
	0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
	0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
	0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
	0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
	0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
	0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
	0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
	0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
	0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
	0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
	0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
	0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
	0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
	0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
	0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
	0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
	0x2d02ef8dL
};

#define DO1(buf) crc = crc32Tab[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
#define DO2(buf)  DO1(buf); DO1(buf);
#define DO4(buf)  DO2(buf); DO2(buf);
#define DO8(buf)  DO4(buf); DO4(buf);

uint32 NESEngine::crc32(uint32 crc, uint8 *buf, int len)
{
	crc = crc ^ 0xffffffffL;
	while (len >= 8)
	{
		DO8(buf);
		len -= 8;
	}
	if (len) do {
		DO1(buf);
	} while (--len);
	return crc ^ 0xffffffffL;
}

///////////////////////////////////////////////////////////////////////////////
// malloc replacement
///////////////////////////////////////////////////////////////////////////////

static void* (*p_malloc)(size_t size) = NULL;
static void (*p_free)(void *ptr) = NULL;
static void* (*p_calloc)(size_t nmemb, size_t size) = NULL;
static void* (*p_realloc)(void *ptr, size_t size) = NULL;
static void* (*p_memalign)(size_t alignment, size_t size) = NULL;

void mallocInit(t_emuAllocators *allocators)
{
	p_malloc = allocators->p_malloc;
	p_free = allocators->p_free;
	p_calloc = allocators->p_calloc;
	p_realloc = allocators->p_realloc;
	p_memalign = allocators->p_memalign;
}

extern "C" void* malloc(size_t size)
{
	return p_malloc(size);
}

extern "C" void  free(void *ptr)
{
	p_free(ptr);
}

extern "C" void* calloc(size_t nmemb, size_t size)
{
	return p_calloc(nmemb, size);
}

extern "C" void* realloc(void *ptr, size_t size)
{
	return p_realloc(ptr, size);
}

extern "C" void* memalign(size_t alignment, size_t size)
{
	return p_memalign(alignment, size);
}

extern "C" char *strdup(const char *inStr)
{
    char *outStr;
    if (NULL == inStr)
    {
        outStr = NULL;
    }
    else
    {
        outStr = (char *)calloc((strlen(inStr) + 1), sizeof(char));
        if (NULL != outStr)
        {
            strcpy(outStr, inStr);
        }
    }
    return outStr;
}
