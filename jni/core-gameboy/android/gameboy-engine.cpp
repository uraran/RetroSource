#define GAMEBOY_SOUND_RATE		32000
#define GAMEBOY_MAX_WIDTH		256
#define GAMEBOY_MAX_HEIGHT		256
#define GAMEBOY_PITCH			(GAMEBOY_MAX_WIDTH * 2)
#define GAMEBOY_FRAME_CYCLES	(70224)

#include <android/log.h>
#include <jni.h>
#include <stdlib.h>
#include <time.h>
#include <zlib.h>
//#include "prof.h"

#include "logging.h"
#include "retronCommon.h"

#include "Util.h"
#include "common/Port.h"
#include "common/Patch.h"
#include "common/SoundDriver.h"
#include "gba/Flash.h"
#include "gba/RTC.h"
#include "gba/Sound.h"
#include "gba/Cheats.h"
#include "gba/GBA.h"
#include "gba/agbprint.h"
#include "gb/gb.h"
#include "gb/gbGlobals.h"
#include "gb/gbCheats.h"
#include "gb/gbSound.h"
#include "System.h"

#include "gbc-palettes.h"

///////////////////////////////////////////////////////////////////////////////
//   Global variables used by the engine
///////////////////////////////////////////////////////////////////////////////

#define KEYM_A            (1<<0)
#define KEYM_B            (1<<1)
#define KEYM_SELECT       (1<<2)
#define KEYM_START        (1<<3)
#define KEYM_RIGHT        (1<<4)
#define KEYM_LEFT         (1<<5)
#define KEYM_UP           (1<<6)
#define KEYM_DOWN         (1<<7)
#define KEYM_R            (1<<8)
#define KEYM_L            (1<<9)

u16 systemColorMap16[0x10000];
u32 systemColorMap32[0x10000];
u16 systemGbPalette[24];
int systemRedShift = 0;
int systemGreenShift = 0;
int systemBlueShift = 0;
int systemColorDepth = 0;
int systemDebug = 0;
int systemVerbose = 0;
int systemFrameSkip = 0;
int systemSaveUpdateCounter = 0;
int systemSpeed = 0;
int emulating = 0;

void dbgSignalDummy(int,int) {}
void (*dbgSignal)(int,int) = dbgSignalDummy;
void dbgOutputDummy(const char *, u32) {}
void (*dbgOutput)(const char *, u32) = dbgOutputDummy;

struct EmulatedSystem emulator =
{
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	false,
	0
};

void mallocInit(t_emuAllocators *allocators);

uint32_t ticksGetTicksMs();
uint64_t ticksGetTicksUs();

class GameboyEngine : public cEmulatorPlugin {
public:
	GameboyEngine();
	virtual ~GameboyEngine();

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

	static uint32_t buttons[4];
	static int srcPitch;
	static int srcWidth;
	static int srcHeight;
	static cEmuBitmap *thisBitmap;
	static uint16_t sampleBuffer[GAMEBOY_SOUND_RATE * 2];
	static int sampleCurrentBytes;

	static void drawScreen();
private:

	void syncPalette();
	void updateSramCRC();

	t_romInfo mRomInfo;
	uint32_t mSramCRC;
	bool mNvmDirty;
	int mLastClockOffset;
	bool mSgbBorders;
	int mTargetEmuType;
	int mSelectedPalette;
};

int GameboyEngine::sampleCurrentBytes = 0;
uint16_t GameboyEngine::sampleBuffer[GAMEBOY_SOUND_RATE * 2];
uint32_t GameboyEngine::buttons[4] = {0};
int GameboyEngine::srcPitch = 0;
int GameboyEngine::srcWidth = 0;
int GameboyEngine::srcHeight = 0;
cEmuBitmap *GameboyEngine::thisBitmap = NULL;

class SoundRetron: public SoundDriver
{
public:
	SoundRetron();
	virtual ~SoundRetron();

	virtual bool init(long sampleRate);
	virtual void pause();
	virtual void reset();
	virtual void resume();
	virtual void write(u16 * finalWave, int length);

private:
};

GameboyEngine::GameboyEngine()
{
	mSramCRC = -1;
	mNvmDirty = false;
	mSgbBorders = false;
	mTargetEmuType = 0;
	mSelectedPalette = 0;
}

GameboyEngine::~GameboyEngine()
{

}

bool GameboyEngine::initialise(t_pluginInfo *info)
{
#ifdef PROFILE
	monstartup("libcore-gameboy.so");
#endif

	soundInit();
	soundSetSampleRate(GAMEBOY_SOUND_RATE);
	info->maxHeight = GAMEBOY_MAX_HEIGHT;
	info->maxWidth = GAMEBOY_MAX_WIDTH;
	info->bitmapPitch = GAMEBOY_PITCH;
    LOGI("initialised\n");
	return true;
}

void GameboyEngine::destroy()
{
#ifdef PROFILE
	moncleanup();
#endif
}

void GameboyEngine::reset()
{
	if(emulator.emuReset)
		emulator.emuReset();
}

t_romInfo *GameboyEngine::loadRomFile(const char *file, t_systemRegion systemRegion)
{
	memset(&mRomInfo, 0, sizeof(t_romInfo));

	// init colour tab - fixed to RGB565
	systemColorDepth = 16;
	systemRedShift = 11;
	systemGreenShift = 6;
	systemBlueShift = 0;
	for(int i = 0; i < 0x10000; i++)
	{
		systemColorMap16[i] =
			((i & 0x1f) << systemRedShift) |
			(((i & 0x3e0) >> 5) << systemGreenShift) |
			(((i & 0x7c00) >> 10) << systemBlueShift);
	}

	// GB/GBC mode
	int rv = gbLoadRom(file);
	if(rv <= 0)
	{
		LOGE("failed to load GB rom: %s\n", file);
		return NULL;
	}

	bool romSupportsSGB = ((gbRom[0x146] == 0x03) && (gbRom[0x14B] == 0x33));

	static const char *FORCE_DISABLE_SGB[] =
	{
		"OHASTA Y&R",
	};
	for(int i = 0; i < (sizeof(FORCE_DISABLE_SGB)/sizeof(char *)); i++)
		if(!strncmp((const char *)&gbRom[0x134], FORCE_DISABLE_SGB[i], 11))
		{
			LOGI("this game is SGB blacklisted\n");
			romSupportsSGB = false;
			break;
		}

	if((mTargetEmuType == 0 || mTargetEmuType == 2) && mSgbBorders && romSupportsSGB)
	{
		gbBorderOn = 1;
		srcWidth = 256;
		srcHeight = 224;
		gbBorderLineSkip = 256;
		gbBorderColumnSkip = 48;
		gbBorderRowSkip = 40;
		gbEmulatorType = 2;

	}
	else
	{
		gbBorderOn = 0;
		srcWidth = 160;
		srcHeight = 144;
		gbBorderLineSkip = 160;
		gbBorderColumnSkip = 0;
		gbBorderRowSkip = 0;
		gbEmulatorType = mTargetEmuType;
	}
	srcPitch = srcWidth * sizeof(short) + 4;

	emulator = GBSystem;
	gbGetHardwareType();

	// used for the handling of the gb Boot Rom
//	if (gbHardware & 5)
//	gbCPUInit(gbBiosFileName, useBios);

	syncPalette();

	gbSoundReset();
	gbSoundSetDeclicking(true);
	gbReset();

	mRomInfo.fps = 4194304.0 / (double)GAMEBOY_FRAME_CYCLES;
	mRomInfo.aspectRatio = (double)srcWidth / (double)srcHeight;
	mRomInfo.soundRate = GAMEBOY_SOUND_RATE;
	mRomInfo.soundMaxBytesPerFrame = sizeof(sampleBuffer);

	mLastClockOffset = 0;
	memset(buttons, 0, sizeof(buttons));
	emulating = 1;
	return &mRomInfo;
}

void GameboyEngine::unloadRom()
{
	soundShutdown();
	emulator.emuCleanUp();
}

bool GameboyEngine::isNvmDirty()
{
	if(mNvmDirty)
		return true;

	static int frameCounter = 0;
	if(frameCounter++ % 30)
		return false;

	if(gbRamSize)
		if(mSramCRC != crc32(0, gbRam, gbRamSizeMask + 1))
		{
			LOGI("SRAM changed\n");
			mNvmDirty = true;
		}

	return mNvmDirty;
}

void GameboyEngine::updateSramCRC()
{
	mSramCRC = crc32(0, gbRam, gbRamSizeMask + 1);
}

int GameboyEngine::saveNvm(const char *file)
{
	if(gbRamSize)
	{
		if(emulator.emuWriteBattery(file) == false)
			return -1;
		updateSramCRC();
		mNvmDirty = false;
		return 1;
	}
	return 0;
}

bool GameboyEngine::loadNvm(const char *file)
{
	if(gbRamSize)
	{
		if(emulator.emuReadBattery(file) == false)
			return false;
		updateSramCRC();
		mNvmDirty = false;
		return true;
	}
	return false;
}

int GameboyEngine::saveNvmBuffer(void **buffer, int *size)
{
	return -1;
}

bool GameboyEngine::saveSnapshot(const char *file)
{
	return emulator.emuWriteState(file);
}

bool GameboyEngine::loadSnapshot(const char *file)
{
	bool ret = emulator.emuReadState(file);

	// for Gameboy, we sync the palette after loading state based on the current settings
	if(gbCgbMode == 0 && gbSgbMode == 0)
	{
		syncPalette();
		memcpy(gbPalette, systemGbPalette, 12*sizeof(u16));
	}

	return ret;
}

void GameboyEngine::runFrame(cEmuBitmap *curBitmap, t_emuInputState ctrlState, short *soundBuffer, int *soundSampleByteCount)
{
	if(curBitmap == NULL)
	{
		thisBitmap = NULL;
	}
	else
	{
		thisBitmap = curBitmap;
		thisBitmap->setDimensions(srcWidth, srcHeight);
	}

	for(int i = 0; i < 4; i++)
	{
		buttons[i] = 0;
		if(!(ctrlState.padConnectMask & (1 << i)))
			continue;

		if(ctrlState.padState[i] & BTN_SELECT)
			buttons[i] |= KEYM_SELECT;
		if(ctrlState.padState[i] & BTN_START)
			buttons[i] |= KEYM_START;
		if(ctrlState.padState[i] & BTN_UP)
			buttons[i] |= KEYM_UP;
		if(ctrlState.padState[i] & BTN_DOWN)
			buttons[i] |= KEYM_DOWN;
		if(ctrlState.padState[i] & BTN_LEFT)
			buttons[i] |= KEYM_LEFT;
		if(ctrlState.padState[i] & BTN_RIGHT)
			buttons[i] |= KEYM_RIGHT;
		if(ctrlState.padState[i] & BTN_BUTTON_1)
			buttons[i] |= KEYM_A;
		if(ctrlState.padState[i] & BTN_BUTTON_2)
			buttons[i] |= KEYM_B;
		if(ctrlState.padState[i] & BTN_BUTTON_5)
			buttons[i] |= KEYM_L;
		if(ctrlState.padState[i] & BTN_BUTTON_6)
			buttons[i] |= KEYM_R;
	}

//	LOGI("frame start\n");

	sampleCurrentBytes = 0;
	int ticksPerFrame = (gbSpeed) ? GAMEBOY_FRAME_CYCLES / 2 : GAMEBOY_FRAME_CYCLES / 4;
	mLastClockOffset = emulator.emuMain(ticksPerFrame - mLastClockOffset);
	gbSoundFlush();

	if(soundBuffer)
	{
		memcpy(soundBuffer, sampleBuffer, sampleCurrentBytes);
		*soundSampleByteCount = sampleCurrentBytes;
	}
//	LOGI("frame end, clk off = %d, sample bytes = %d\n", mLastClockOffset, sampleCurrentBytes);

#if 1
	// sound output verification
	{
		static int sampleCounter = 0;
		static int frameCounter = 0;

		sampleCounter += sampleCurrentBytes / 4;
		frameCounter++;
		if(frameCounter == 60*60)
		{
			LOGI("gameboy sound stats: %d\n", sampleCounter / 60);
			sampleCounter = 0;
			frameCounter = 0;
		}
	}
#endif
}

void GameboyEngine::drawScreen()
{
	if(thisBitmap)
	{
		thisBitmap->setDimensions(srcWidth, srcHeight);
		
		uint8_t *dst = (uint8_t *)thisBitmap->getBuffer();
		for(int i = 0; i < srcHeight; i++)
			memcpy(&dst[i * GAMEBOY_PITCH], &pix[(i+1) * srcPitch], srcWidth * sizeof(short));
	}
}

bool getBoolFromString(const char *str)
{
	if(!strcasecmp(str, PLUGINOPT_TRUE))
		return true;
	else
		return false;
}

bool GameboyEngine::setOption(const char *name, const char *value)
{
	if(!strcasecmp(name, PLUGINOPT_GB_SGB_BORDER))
	{
		mSgbBorders = getBoolFromString(value);
		return true;
	}
	else if(!strcasecmp(name, PLUGINOPT_GB_HWTYPE))
	{
		int type = strtol(value, NULL, 10);
		switch(type)
		{
		case 1: // GB
			mTargetEmuType = 3;
			break;
		case 2:	// CGB
			mTargetEmuType = 1;
			break;
		case 3: // SGB
			mTargetEmuType = 2;
			break;
		default: // auto
			mTargetEmuType = 0;
		}
	}
	else if(!strcasecmp(name, PLUGINOPT_GB_PALETTE))
	{
		mSelectedPalette = strtol(value, NULL, 10);
		if(gbRom != NULL)
			syncPalette();
	}

	return false;
}

bool GameboyEngine::addCheat(const char *cheat)
{
	char *scheat = strdup(cheat), *code;
	bool ret = false;
	char *format = strtok((char *)scheat, ";");
	if(!format)
	{
		LOGE("cheat parse error 1\n");
		goto addCheat_end;
	}
	code = strtok(NULL, ";");
	if(!code)
	{
		LOGE("cheat parse error 2\n");
		goto addCheat_end;
	}
	if(!strcasecmp(format, "GameShark"))
	{
		ret = gbAddGsCheat(code, "");
		LOGI("add GS code %s: %d\n", code, ret);
	}
	else if(!strcasecmp(format, "Game Genie"))
	{
		ret = gbAddGgCheat(code, "");
		LOGI("add GENIE code %s: %d\n", code, ret);
	}

addCheat_end:
	free(scheat);
	return ret;
}

bool GameboyEngine::removeCheat(const char *cheat)
{
	return false;
}

void GameboyEngine::resetCheats()
{
	gbCheatRemoveAll();
}

extern "C" __attribute__((visibility("default")))
cEmulatorPlugin *createPlugin(t_emuAllocators *allocators)
{
	mallocInit(allocators);
	return new GameboyEngine;
}

struct timespec mStartTicks = { 0 };
void ticksCheckInit()
{
	static int inited = 0;
	if(inited == 0)
	{
		clock_gettime(CLOCK_MONOTONIC, &mStartTicks);
		inited = 1;
	}
}

// get tick count in mSec
uint32_t ticksGetTicksMs()
{
	struct timespec now;
	ticksCheckInit();
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (now.tv_sec - mStartTicks.tv_sec) * 1000 +
			(now.tv_nsec - mStartTicks.tv_nsec) / 1000000;
}

uint64_t ticksGetTicksUs()
{
	struct timespec now;
	ticksCheckInit();
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (now.tv_sec - mStartTicks.tv_sec) * 1000000 +
			(now.tv_nsec - mStartTicks.tv_nsec) / 1000;
}


///////////////////////////////////////////////////////////////////////////////
//   VBA interface code
///////////////////////////////////////////////////////////////////////////////

void log(const char *,...)
{

}

void systemFrame() 
{ 

}

void systemDrawScreen()
{
	GameboyEngine::drawScreen();
}

bool systemPauseOnFrame()
{
	return false;
}

bool systemReadJoypads()
{
	return true;
}

u32 systemReadJoypad(int which)
{
	if(which < 0 || which > 3)
		which = 0;
	return GameboyEngine::buttons[which];
}

u32 systemGetClock()
{
	return ticksGetTicksMs();
}

SoundDriver * systemSoundInit()
{
	soundShutdown();
	return new SoundRetron();
}

void systemUpdateMotionSensor()
{

}

int  systemGetSensorX()
{
	return 0;
}

int  systemGetSensorY()
{
	return 0;
}

bool systemCanChangeSoundQuality()
{
	return false;
}

void system10Frames(int)
{

}

void DbgMsg(const char *msg, ...)
{

}

static const uint16_t originalPal[24] = { 0x1B8E, 0x02C0, 0x0DA0, 0x1140,  0x1B8E, 0x02C0, 0x0DA0, 0x1140 };
static const uint16_t gbaspPal[24] = { 0x7BDE, 0x5778, 0x5640, 0x0000, 0x7BDE, 0x529C, 0x2990, 0x0000 };
static const unsigned short *paletteTable[] =
{
		NULL,	// placeholder for "auto"
		originalPal,
		gbaspPal,
		p518,	// GBC - Blue
		p012,	// GBC - Brown
		p50D,	// GBC - Dark Blue
		p319,	// GBC - Dark Brown
		p31C,	// GBC - Dark Green
		p016,	// GBC - Grayscale
		p005,	// GBC - Green
		p013,	// GBC - Inverted
		p007,	// GBC - Orange
		p017,	// GBC - Pastel Mix
		p510,	// GBC - Red
		p51A,	// GBC - Yellow
};

void GameboyEngine::syncPalette()
{
	int i;

	// default palette
	for( i = 0; i < 24; )
	{
		systemGbPalette[i++] = (0x1f) | (0x1f << 5) | (0x1f << 10);
		systemGbPalette[i++] = (0x15) | (0x15 << 5) | (0x15 << 10);
		systemGbPalette[i++] = (0x0c) | (0x0c << 5) | (0x0c << 10);
		systemGbPalette[i++] = 0;
	}

	// check for any GBC palette overrides for GB games
	if(mSelectedPalette == 0)
	{
		char romTitle[17];
		strncpy(romTitle, (char const *)&gbRom[0x134], 16);
		romTitle[16] = '\0';
		const uint16_t *p = findGbcTitlePal(romTitle);
		if(p)
		{
			LOGI("found GB palette override\n");
			memcpy(systemGbPalette, p, 12*sizeof(u16));
		}
	}
	else if((mSelectedPalette >= 1) && (mSelectedPalette < (sizeof(paletteTable) / sizeof(short *))))
	{
		memcpy(systemGbPalette, paletteTable[mSelectedPalette], 12*sizeof(u16));
	}
}

void systemGbPrint(u8 *,int,int,int,int,int) { }
void systemScreenCapture(int) { }
//void systemMessage(int, const char *, ...) { }
void systemSetTitle(const char *) { }
void systemOnSoundShutdown() { }
void systemScreenMessage(const char *) { }
void systemShowSpeed(int) { }
void systemGbBorderOn() { }

///////////////////////////////////////////////////////////////////////////////
//   VBA sound class
///////////////////////////////////////////////////////////////////////////////

SoundRetron::SoundRetron()
{

}

SoundRetron::~SoundRetron()
{

}

bool SoundRetron::init(long sampleRate)
{
	LOGI("SoundRetron::init - %d\n", sampleRate);
	return true;
}

void SoundRetron::pause()
{

}

void SoundRetron::reset()
{

}

void SoundRetron::resume()
{

}

void SoundRetron::write(u16 * finalWave, int length)
{
//	LOGI("SoundRetron::write - %d, %d\n", GameboyEngine::sampleCurrentBytes, length);
//	memcpy(&GameboyEngine::sampleBuffer[GameboyEngine::sampleCurrentBytes / 2], finalWave, length);
//	GameboyEngine::sampleCurrentBytes += length;
}

void systemOnWriteDataToSoundBuffer(const u16 * finalWave, int length)
{
//	LOGI("systemOnWriteDataToSoundBuffer - %d, %d\n", GameboyEngine::sampleCurrentBytes, length * sizeof(short));
	uint8_t *dst = (uint8_t *)GameboyEngine::sampleBuffer;
	memcpy(&dst[GameboyEngine::sampleCurrentBytes], finalWave, length * sizeof(short));
	GameboyEngine::sampleCurrentBytes += length * sizeof(short);
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
