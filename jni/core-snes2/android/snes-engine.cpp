#include <jni.h>
#include <android/log.h>
#include <unistd.h>
#include <zlib.h>
#include <stdlib.h>

#include "snes9x.h"
#include "memmap.h"
#include "cpuexec.h"
#include "srtc.h"
#include "apu.h"
#include "ppu.h"
#include "snapshot.h"
#include "controls.h"
#include "cheats.h"
#include "display.h"

#include "logging.h"
#include "retronCommon.h"

#define SPC7110_CHECK		"SPC7110 CHECK OK"

typedef struct
{
	uint16				buttons;
	uint16				turbos;
	uint16				toggleturbo;
	uint16				togglestick;
	uint8				turbo_ct;
} t_joypad;

extern t_joypad joypad[8];

//extern uint16_t joypad[8];

volatile int g_FrameEndCounter = 0;
static cEmuBitmap *thisBitmap = NULL;

void mallocInit(t_emuAllocators *allocators);

static void S9xAudioCallback();

class SNESEngine : public cEmulatorPlugin {
public:

	SNESEngine();
	virtual ~SNESEngine();

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

	int getSramSize();
	void updateSramCRC();
	bool readFile(const char *filename, void **buffer, int *size);
	bool writeFile(const char *filename, void *buffer, int size);

	uint32 mSramCRC;
	bool mNvmDirty;
	bool mUseAltMouseConfig;
	t_romInfo mRomInfo;
	int mLastCtrlConnect;
	int mMouseX, mMouseY;
};

SNESEngine::SNESEngine()
{
	mSramCRC = -1;
	mNvmDirty = false;
	mUseAltMouseConfig = false;
	mLastCtrlConnect = 0;
}

SNESEngine::~SNESEngine()
{

}

bool SNESEngine::initialise(t_pluginInfo *info)
{
	// Initialise Snes stuff
	memset(&Settings, 0, sizeof(Settings));

	Settings.FrameTimePAL = 20000;
	Settings.FrameTimeNTSC = 16667;
	Settings.SixteenBitSound = TRUE;
	Settings.Stereo = TRUE;
	Settings.SoundPlaybackRate = 32000;
	Settings.SoundInputRate = 32000;
	Settings.SupportHiRes = TRUE;
	Settings.Transparency = TRUE;
	Settings.HDMATimingHack = 100;
	Settings.BlockInvalidVRAMAccessMaster = TRUE;
	Settings.CartAName[0] = 0;
	Settings.CartBName[0] = 0;
	Settings.SoundSync = TRUE;

	CPU.Flags = 0;
	if (!Memory.Init() || !S9xInitAPU())
	{
		Memory.Deinit();
		S9xDeinitAPU();
		LOGE("Failed to init Memory or APU.\n");
		return false;
	}

	S9xInitSound(32, 0);
//	S9xSetSamplesAvailableCallback(S9xAudioCallback);

	GFX.Pitch = IMAGE_WIDTH * sizeof(uint16_t);
	S9xGraphicsInit();
	GFX.Screen = (uint16_t *)calloc(1, GFX.ScreenSize * sizeof(uint16_t));
	assert(GFX.Screen != NULL);

	info->maxWidth = IMAGE_WIDTH;
	info->maxHeight = SNES_HEIGHT_EXTENDED * 2;
	info->bitmapPitch = GFX.Pitch;
	mMouseX = 0;
	mMouseY = 0;

	return true;
}

void SNESEngine::destroy()
{
	if (GFX.Screen) {
		free(GFX.Screen);
		GFX.Screen = NULL;
	}

	S9xDeinitAPU();
	Memory.Deinit();
	S9xGraphicsDeinit();
	S9xUnmapAllControls();

}

void SNESEngine::reset()
{
	S9xReset();
	S9xSoftReset();
	Memory.ClearSRAM();
}

t_romInfo *SNESEngine::loadRomFile(const char *file, t_systemRegion systemRegion)
{
	memset(&mRomInfo, 0, sizeof(t_romInfo));

	Settings.ForcePAL = FALSE;
	Settings.ForceNTSC = FALSE;
	switch(systemRegion)
	{
	case SYS_REGION_USA:
	case SYS_REGION_JAP:
		Settings.ForceNTSC = TRUE;
		break;
	case SYS_REGION_EUR:
		Settings.ForcePAL = TRUE;
		break;
	}

	int rv = Memory.LoadROM(file);
	if(!rv)
	{
		LOGE("failed to load rom\n");
		return NULL;
	}

	// set correct initial SRAM state (needed for Megaman X)
	memset(Memory.SRAM, SNESGameFixes.SRAMInitialValue, 0x20000);

	if(Settings.SPC7110 || Settings.SPC7110RTC)
	{
		LOGI("applying SPC7110 SRAM hack");
		memcpy(&Memory.SRAM[getSramSize() - 16], SPC7110_CHECK, 16);
	}
	mRomInfo.fps = (Settings.PAL) ? 21281370.0 / 425568.0 : 21477272.0 / 357366.0;
	mRomInfo.soundRate = 32040;
	mRomInfo.soundMaxBytesPerFrame = ((mRomInfo.soundRate / 50) + 1) * 2 * sizeof(short);
	mRomInfo.aspectRatio = 4.0 / 3.0;

	return &mRomInfo;
}

void SNESEngine::unloadRom()
{

}

bool SNESEngine::readFile(const char *filename, void **buffer, int *size)
{
	FILE *fd = fopen(filename, "rb");
	if(!fd)
		return false;
	fseek(fd, 0, SEEK_END);
	*size = ftell(fd);
	fseek(fd, 0, SEEK_SET);
	*buffer = (void *)malloc(*size);
	assert(*buffer != NULL);
	int rv = fread(*buffer, *size, 1, fd);
	fclose(fd);
	if(rv != 1)
	{
		free(*buffer);
		return false;
	}

	return true;
}

bool SNESEngine::writeFile(const char *filename, void *buffer, int size)
{
	FILE *fd = fopen(filename, "wb+");
	if(!fd)
		return false;
	int rv = fwrite(buffer, size, 1, fd);
	fclose(fd);
	if(rv != 1)
		return false;

	return true;
}

int SNESEngine::getSramSize()
{
	int size = (unsigned) (Memory.SRAMSize ? (1 << (Memory.SRAMSize + 3)) * 128 : 0);
	if (size > 0x20000)
		size = 0x20000;
	return size;
}

bool SNESEngine::isNvmDirty()
{
	if(mNvmDirty)
		return true;

	// As the CRC is quite slow, only process once per 30 frames
	static int frameCounter = 0;
	if(frameCounter++ % 30)
		return false;

	int size = getSramSize();
	if(size)
		if(mSramCRC != crc32(0, Memory.SRAM, size))
		{
			LOGI("SRAM changed\n");
			mNvmDirty = true;
		}

	return mNvmDirty;
}

void SNESEngine::updateSramCRC()
{
	mSramCRC = crc32(0, Memory.SRAM, getSramSize());
}

int SNESEngine::saveNvmBuffer(void **buffer, int *size)
{
	int sramSize = getSramSize();
	if(sramSize)
	{
		*buffer = (void *)malloc(sramSize);
		assert(*buffer != NULL);
		memcpy(*buffer, Memory.SRAM, sramSize);
		*size = sramSize;
		updateSramCRC();
		mNvmDirty = false;
		return 1;
	}
	return 0;
}

int SNESEngine::saveNvm(const char *file)
{
	int size = getSramSize();
	if(size)
	{
		if(writeFile(file, Memory.SRAM, size) == false)
			return -1;
		updateSramCRC();
		mNvmDirty = false;
		return 1;
	}
	return 0;
}

bool SNESEngine::loadNvm(const char *file)
{
	int size = getSramSize();
	if(size)
	{
		void *tmpBuf;
		int bufSize;
		if(readFile(file, &tmpBuf, &bufSize) == true)
		{
			if(bufSize > size)
				bufSize = size;

			memcpy(Memory.SRAM, tmpBuf, bufSize);
			free(tmpBuf);
		}
		updateSramCRC();
		mNvmDirty = false;

		if(Settings.SPC7110 || Settings.SPC7110RTC)
		{
			LOGI("applying SPC7110 SRAM hack");
			memcpy(&Memory.SRAM[getSramSize() - 16], SPC7110_CHECK, 16);
		}
	}
	return true;
}

bool SNESEngine::saveSnapshot(const char *file)
{
	if(S9xFreezeGame(file) == TRUE)
		return true;
	else
		return false;
}

bool SNESEngine::loadSnapshot(const char *file)
{
	int rv = S9xUnfreezeGame(file);
	if(rv != TRUE)
		return false;

	return true;
}

void SNESEngine::runFrame(cEmuBitmap *curBitmap, t_emuInputState ctrlState, short *soundBuffer, int *soundSampleByteCount)
{
	// sometimes S9xMainLoop() can run for longer than a single frame, in which case we need to re-sync the main emulation loop
	if(g_FrameEndCounter > 0)
	{
		*soundSampleByteCount = 0;
		g_FrameEndCounter--;
		return;
	}

	if(curBitmap == NULL)
	{
		thisBitmap = NULL;
		IPPU.RenderThisFrame = FALSE;
	}
	else
	{
		thisBitmap = curBitmap;
		IPPU.RenderThisFrame = TRUE;
	}

	t_emuInputMouseState mouseState;
	memset(&mouseState, 0, sizeof(t_emuInputMouseState));
	for(int i = 0; i < ctrlState.specialStateNum; i++)
		if(ctrlState.specialStates[i]->type == INPUT_SNES_MOUSE && ctrlState.specialStates[i]->stateLen == sizeof(t_emuInputMouseState))
		{
			memcpy(&mouseState, ctrlState.specialStates[i], sizeof(t_emuInputMouseState));
			break;
		}

	int thisConnectState = ctrlState.padConnectMask | ((mouseState.connected & 1) << 16);
	if(mLastCtrlConnect != thisConnectState)
	{
		S9xUnmapAllControls();

		if(mouseState.connected)
		{
			if(mUseAltMouseConfig)
			{
				S9xSetController(1, CTL_MOUSE, 0, 0, 0, 0);
				S9xSetController(0, CTL_JOYPAD, 0, 0, 0, 0);
			}
			else
			{
				S9xSetController(0, CTL_MOUSE, 0, 0, 0, 0);
				S9xSetController(1, CTL_JOYPAD, 1, 0, 0, 0);
			}
		}
		else if(ctrlState.padConnectMask & ~3)
		{
			S9xSetController(0, CTL_JOYPAD, 0, 0, 0, 0);
			S9xSetController(1, CTL_MP5, 1, 2, 3, 4);
		}
		else
		{
			S9xSetController(0, CTL_JOYPAD, 0, 0, 0, 0);
			S9xSetController(1, CTL_JOYPAD, 1, 0, 0, 0);
		}
		mLastCtrlConnect = thisConnectState;
	}

	if(mouseState.connected)
	{
		mMouseX += mouseState.xoff;
		mMouseY += mouseState.yoff;
		s9xcommand_t cmd;
		memset(&cmd, 0, sizeof(cmd));
		cmd.type = S9xPointer;
		cmd.pointer.aim_mouse0 = 1;
//		LOGI("sending mouse co-ords: %d, %d\n", mMouseX, mMouseY);
		S9xApplyCommand(cmd, mMouseX, -mMouseY);
		memset(&cmd, 0, sizeof(cmd));
		cmd.type = S9xButtonMouse;
		cmd.button.mouse.left = 1;
		S9xApplyCommand(cmd, mouseState.buttons & MOUSE_BUTTON_LEFT, 0);
		memset(&cmd, 0, sizeof(cmd));
		cmd.type = S9xButtonMouse;
		cmd.button.mouse.right = 1;
		S9xApplyCommand(cmd, mouseState.buttons & MOUSE_BUTTON_RIGHT, 0);
	}
	else
	{
		mMouseX = 0;
		mMouseY = 0;
	}

	memset(joypad, 0, sizeof(joypad));
	for(int i = 0; i < 5; i++)
	{
	    if(ctrlState.padState[i] & BTN_SELECT)
	    	joypad[i].buttons |= SNES_SELECT_MASK;
	    if(ctrlState.padState[i] & BTN_START)
	    	joypad[i].buttons |= SNES_START_MASK;
	    if(ctrlState.padState[i] & BTN_UP)
	    	joypad[i].buttons |= SNES_UP_MASK;
	    if(ctrlState.padState[i] & BTN_DOWN)
	    	joypad[i].buttons |= SNES_DOWN_MASK;
	    if(ctrlState.padState[i] & BTN_LEFT)
	    	joypad[i].buttons |= SNES_LEFT_MASK;
	    if(ctrlState.padState[i] & BTN_RIGHT)
	    	joypad[i].buttons |= SNES_RIGHT_MASK;
	    if(ctrlState.padState[i] & BTN_BUTTON_1)
	    	joypad[i].buttons |= SNES_A_MASK;
	    if(ctrlState.padState[i] & BTN_BUTTON_2)
	    	joypad[i].buttons |= SNES_B_MASK;
	    if(ctrlState.padState[i] & BTN_BUTTON_3)
	    	joypad[i].buttons |= SNES_X_MASK;
	    if(ctrlState.padState[i] & BTN_BUTTON_4)
	    	joypad[i].buttons |= SNES_Y_MASK;
	    if(ctrlState.padState[i] & BTN_BUTTON_LT)
	    	joypad[i].buttons |= SNES_TL_MASK;
	    if(ctrlState.padState[i] & BTN_BUTTON_RT)
	    	joypad[i].buttons |= SNES_TR_MASK;
	}

	do
	{
		S9xMainLoop();
	} while(!g_FrameEndCounter);
	S9xSyncSound();
	g_FrameEndCounter--;

	if(soundBuffer)
	{
		S9xFinalizeSamples();
		int sampleCount = S9xGetSampleCount();
//		LOGI("sampleCount = %d\n", sampleCount);
		if(S9xMixSamples((uint8_t *)soundBuffer, sampleCount) == 0)
		{
			LOGE("S9xMixSamples failed\n");
		}
		*soundSampleByteCount = sampleCount * sizeof(short);
	}

#if 0
	// sound output verification
	{
		static int sampleCounter = 0;
		static int frameCounter = 0;

		sampleCounter += sampleCount / 2;
		frameCounter++;
		if(frameCounter == 60)
		{
			LOGI("snes sound stats: %d\n", sampleCounter);
			sampleCounter = 0;
			frameCounter = 0;
		}
	}
#endif

}

bool SNESEngine::setOption(const char *name, const char *value)
{
	// NOTE: most games on SNES run at 256x224 which already accounts for TV overscan and represents the full image viewable onscreen. The SNES also supports 256x240 which allows for
	// graphics to be drawn in the overscan area, but I'm not aware of any games that actually use it. Therefore at this time, we dont need to worry about overscan processing with SNES
	return false;
}

bool SNESEngine::addCheat(const char *cheat)
{
	char *scheat = strdup(cheat), *s_format, *s_address, *s_value;
	uint32_t address, value;
	bool ret = false;

	s_format = strtok((char *)scheat, ";");
	if(!s_format)
	{
		LOGE("cheat parse error 1\n");
		goto addCheat_end;
	}
	s_address = strtok(NULL, ":");
	if(!s_address)
	{
		LOGE("cheat parse error 2\n");
		goto addCheat_end;
	}
	s_value = strtok(NULL, ":");
	if(!s_value)
	{
		LOGE("cheat parse error 3\n");
		goto addCheat_end;
	}
	if(strcasecmp(s_format, "raw"))
	{
		LOGE("unsupported format: %s\n", s_format);
		goto addCheat_end;
	}
	address = strtol(s_address, NULL, 16);
	value = strtol(s_value, NULL, 16);
	LOGI("cheat: %X:%X\n", address, value);
	S9xAddCheat(true, true, address, value);
	Settings.ApplyCheats = TRUE;
	ret = true;

addCheat_end:
	free(scheat);
	return ret;
}

bool SNESEngine::removeCheat(const char *cheat)
{
	return false;
}

void SNESEngine::resetCheats()
{
	S9xDeleteCheats();
}

bool S9xPollButton (uint32 id, bool *pressed) { return FALSE; }
bool S9xPollPointer (uint32 id, int16 *x, int16 *y) { return FALSE; }
bool S9xPollAxis (uint32 id, int16 *value) { return FALSE; }
void S9xHandlePortCommand (s9xcommand_t cmd, int16 data1, int16 data2) {}
void S9xSyncSpeed (void) {}
bool8 S9xInitUpdate (void) { return TRUE; }
void S9xAutoSaveSRAM (void) {}
void S9xSetPalette (void) {}
bool8 S9xContinueUpdate (int, int) {}

bool8 S9xOpenSoundDevice (void) { return TRUE; }

bool8 S9xDeinitUpdate(int width, int height)
{
	if(thisBitmap)
	{
		thisBitmap->setDimensions(width, height);
		uint8_t *dst = (uint8_t *)thisBitmap->getBuffer();
		uint8_t *src = (uint8_t *)GFX.Screen;
		for(int i = 0; i < height; i++)
			memcpy(&dst[i * GFX.Pitch], &src[i * GFX.Pitch], width * sizeof(uint16_t));
	}
	return TRUE;
}

static void S9xAudioCallback()
{
	// Do nothing
}

const char* S9xGetFilename(const char* in, s9x_getdirtype dirtype) { return in; }
const char* S9xGetDirectory(s9x_getdirtype dirtype) { return NULL; }
const char* S9xChooseFilename(bool8 a) { return NULL; }
bool8 S9xOpenSnapshotFile (const char *filename, bool8 read_only, STREAM *file)
{
	if ((*file = OPEN_STREAM(filename, read_only ? "rb" : "wb")))
		return (TRUE);

	return (FALSE);
}

void S9xCloseSnapshotFile (STREAM file)
{
	CLOSE_STREAM(file);
}

/*
void S9xMessage(int a, int b, const char* msg)
{
   LOGI(msg);
}
*/
void _splitpath (const char * path, char * drive, char * dir, char * fname, char * ext)
{
	char *slash, *dot;

	slash = strrchr((char*)path, SLASH_CHAR);
	dot   = strrchr((char*)path, '.');

	if (dot && slash && dot < slash)
		dot = NULL;

	if (!slash)
	{
		*dir = 0;

		strcpy(fname, path);

		if (dot)
		{
			fname[dot - path] = 0;
			strcpy(ext, dot + 1);
		}
		else
			*ext = 0;
	}
	else
	{
		strcpy(dir, path);
		dir[slash - path] = 0;

		strcpy(fname, slash + 1);

		if (dot)
		{
			fname[dot - slash - 1] = 0;
			strcpy(ext, dot + 1);
		}
		else
			*ext = 0;
	}
}

void _makepath (char *path, const char * a, const char *dir, const char *fname, const char *ext)
{
   if (dir && *dir)
   {
      strcpy(path, dir);
      strcat(path, SLASH_STR);
   }
   else
      *path = 0;

   strcat(path, fname);

   if (ext && *ext)
   {
      strcat(path, ".");
      strcat(path, ext);
   }
}

extern "C" __attribute__((visibility("default")))
cEmulatorPlugin *createPlugin(t_emuAllocators *allocators)
{
	mallocInit(allocators);
	return new SNESEngine;
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
