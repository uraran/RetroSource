#define PCE_SOUND_RATE		44100
#define PCE_WIDTH 			512
#define PCE_HEIGHT 			242
#define PCE_MAX_PLAYERS		5
#define BIT(b)				(1 << b)

#include "mednafen/mednafen.h"
#include "mednafen/mempatcher.h"
#include "mednafen/git.h"
#include "mednafen/general.h"
#include "mednafen/md5.h"
#include "mednafen/pce_fast/pce.h"
#include <iostream>

#include <jni.h>
#include <android/log.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>

#include "logging.h"
#include "retronCommon.h"

enum JoyPadBits
{
	JOYPAD_BUTTON_I = 0,
	JOYPAD_BUTTON_II,
	JOYPAD_BUTTON_SELECT,
	JOYPAD_BUTTON_RUN,
	JOYPAD_BUTTON_UP,
	JOYPAD_BUTTON_RIGHT,
	JOYPAD_BUTTON_DOWN,
	JOYPAD_BUTTON_LEFT,
	JOYPAD_BUTTON_III,
	JOYPAD_BUTTON_IV,
	JOYPAD_BUTTON_V,
	JOYPAD_BUTTON_VI,
};

void mallocInit(t_emuAllocators *allocators);

class PCEEngine : public cEmulatorPlugin {
public:
	PCEEngine();
	virtual ~PCEEngine();

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

	void setBasename(const char *path);
	bool readFile(const char *filename, void **buffer, int *size);
	bool writeFile(const char *filename, void *buffer, int size);
	void getNVMInfo(void **buf, int *size);
	void updateNVMCRC();
	bool isNVMActive();

	MDFNGI *mGame;
	MDFN_Surface *mSurface;
	static uint32_t mScreenBuf[PCE_WIDTH * PCE_HEIGHT];
	MDFN_PixelFormat mSavedPixFormat;
	double mSavedSoundRate;
	t_romInfo mRomInfo;
	uint8_t mInputBuf[PCE_MAX_PLAYERS][2];
	uint32 mSramCRC;
	bool mNvmDirty;
};

namespace PCE_Fast
{
extern bool IsPopulous;
extern bool IsTsushin;
extern uint8 *TsushinRAM;
extern uint8 SaveRAM[];

extern bool IsBRAMUsed(void);
}

bool mEmulate6ButtonPad = false;
uint32_t PCEEngine::mScreenBuf[PCE_WIDTH * PCE_HEIGHT];
std::string retro_base_directory;
std::string retro_base_name;

PCEEngine::PCEEngine()
{
	memset(mInputBuf, 0, sizeof(mInputBuf));
	memset(mScreenBuf, 0, sizeof(mScreenBuf));
	mEmulate6ButtonPad = false;
	mNvmDirty = false;
}

PCEEngine::~PCEEngine()
{

}

void PCEEngine::setBasename(const char *path)
{
   const char *base = strrchr(path, '/');
   if (!base)
      base = strrchr(path, '\\');

   if (base)
      retro_base_name = base + 1;
   else
      retro_base_name = path;

   std::string baseDir(path, (int)base - (int)path);
   retro_base_directory = baseDir;
   retro_base_name = retro_base_name.substr(0, retro_base_name.find_last_of('.'));
   LOGI("setBasename: %s, %s\n", retro_base_directory.c_str(), retro_base_name.c_str());
}

bool PCEEngine::readFile(const char *filename, void **buffer, int *size)
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

bool PCEEngine::writeFile(const char *filename, void *buffer, int size)
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

bool PCEEngine::initialise(t_pluginInfo *info)
{
	info->maxWidth = PCE_WIDTH;
	info->maxHeight = PCE_HEIGHT;
	info->bitmapPitch = PCE_WIDTH * 2;

	return true;
}

void PCEEngine::destroy()
{

}

void PCEEngine::reset()
{
	if(!mGame)
		return;
	mGame->DoSimpleCommand(MDFN_MSC_RESET);
}

t_romInfo *PCEEngine::loadRomFile(const char *file, t_systemRegion systemRegion)
{
	memset(&mRomInfo, 0, sizeof(t_romInfo));
	LOGI("loading ROM: %s\n", file);

	MDFNI_InitializeModule();
	MDFNI_Initialize(retro_base_directory.c_str());

	setBasename(file);
	mGame = MDFNI_LoadGame("pce_fast", file);
	if(!mGame)
	{
		LOGE("MDFNI_LoadGame failed\n");
		return NULL;
	}

	mSavedSoundRate = 0.0;
	memset(&mSavedPixFormat, 0, sizeof(mSavedPixFormat));
	MDFN_PixelFormat pix(MDFN_COLORSPACE_RGB, 16, 8, 0, 24);
	mSurface = new MDFN_Surface(mScreenBuf, PCE_WIDTH, PCE_HEIGHT, PCE_WIDTH, pix);
/*
    setting_pce_keepaspect = 0;
    mGame->fb_width = 512;
    mGame->nominal_width = 341;
    mGame->lcm_width = 341;
*/
	for(int i = 0; i < PCE_MAX_PLAYERS; i++)
		mGame->SetInput(i, "gamepad", &mInputBuf[i][0]);

	mRomInfo.fps = 59.82610545348264;
	mRomInfo.aspectRatio = 4.0 / 3.0;
	mRomInfo.soundRate = PCE_SOUND_RATE;
	mRomInfo.soundMaxBytesPerFrame = ((mRomInfo.soundRate / 50) + 1) * 2 * sizeof(short);

	return &mRomInfo;
}

void PCEEngine::unloadRom()
{
	if(!mGame)
		return;

	MDFN_FlushGameCheats(1);
	mGame->CloseGame();
	if (mGame->name)
	{
		free(mGame->name);
		mGame->name=0;
	}
	MDFNMP_Kill();
	mGame = NULL;
}

void PCEEngine::getNVMInfo(void **buf, int *size)
{
	if(IsPopulous)
	{
		*buf = PCE_Fast::ROMSpace + 0x40 * 8192;
		*size = 32768;
	}
	else if(IsTsushin)
	{
		*buf = TsushinRAM;
		*size = 32768;
	}
	else
	{
		*buf = SaveRAM;
		*size = 2048;
	}
}

bool PCEEngine::isNVMActive()
{
	if(IsPopulous || IsTsushin || IsBRAMUsed())
		return true;

	return false;
}

bool PCEEngine::isNvmDirty()
{
	if(!isNVMActive())
		return false;

	if(mNvmDirty)
		return true;

	// As the CRC is quite slow, only process once per 30 frames
	static int frameCounter = 0;
	if(frameCounter++ % 30)
		return false;

	void *nvmBuf;
	int nvmSize;
	getNVMInfo(&nvmBuf, &nvmSize);
//	LOGI("getNVMInfo: %X, %X\n", nvmBuf, nvmSize);
	if(mSramCRC != crc32(0, (const Bytef*)nvmBuf, nvmSize))
	{
		LOGI("SRAM changed\n");
		mNvmDirty = true;
	}
	return mNvmDirty;
}

void PCEEngine::updateNVMCRC()
{
	void *nvmBuf;
	int nvmSize;
	getNVMInfo(&nvmBuf, &nvmSize);
	mSramCRC = crc32(0, (const Bytef*)nvmBuf, nvmSize);
}

int PCEEngine::saveNvm(const char *file)
{
	if(isNVMActive())
	{
		void *nvmBuf;
		int nvmSize;
		getNVMInfo(&nvmBuf, &nvmSize);

		if(writeFile(file, nvmBuf, nvmSize) == false)
			return -1;
		updateNVMCRC();
		mNvmDirty = false;
		return 1;
	}
	return 0;
}

bool PCEEngine::loadNvm(const char *file)
{
	bool ret = false;
	void *tmpBuf = NULL;
	int bufSize;

	if(readFile(file, &tmpBuf, &bufSize) == false)
		return false;

	void *nvmBuf;
	int nvmSize;
	getNVMInfo(&nvmBuf, &nvmSize);

	if(bufSize > nvmSize)
		bufSize = nvmSize;

	memcpy(nvmBuf, tmpBuf, bufSize);
	free(tmpBuf);
	updateNVMCRC();
	mNvmDirty = false;
	return true;
}

int PCEEngine::saveNvmBuffer(void **buffer, int *size)
{
	if(isNVMActive())
	{
		void *nvmBuf;
		int nvmSize;
		getNVMInfo(&nvmBuf, &nvmSize);

		*buffer = (void *)malloc(nvmSize);
		assert(*buffer != NULL);
		memcpy(*buffer, nvmBuf, nvmSize);
		*size = nvmSize;
		updateNVMCRC();
		mNvmDirty = false;
		return 1;
	}
	return 0;
}

bool PCEEngine::saveSnapshot(const char *file)
{
	StateMem st;
	memset(&st, 0, sizeof(st));

	if (!MDFNSS_SaveSM(&st, 0, 0, NULL, NULL, NULL))
	{
		LOGE("MDFNSS_SaveSM error\n");
		return false;
	}
	bool ret = writeFile(file, st.data, st.len);
	free(st.data);
	return ret;
}

bool PCEEngine::loadSnapshot(const char *file)
{
	uint8_t *snapshot;
	int snapshotSize;

	if(readFile(file, (void **)&snapshot, &snapshotSize) == false)
		return false;

	StateMem st;
	memset(&st, 0, sizeof(st));
	st.data = snapshot;
	st.len = snapshotSize;
	bool ret = MDFNSS_LoadSM(&st, 0, 0);
	free(snapshot);
	return ret;
}

void PCEEngine::runFrame(cEmuBitmap *curBitmap, t_emuInputState ctrlState, short *soundBuffer, int *soundSampleByteCount)
{
	uint16_t stateP1 = 0, stateP2 = 0;
	memset(mInputBuf, 0, sizeof(mInputBuf));

    if(ctrlState.padState[0] & BTN_SELECT)
    	stateP1 |= BIT(JOYPAD_BUTTON_SELECT);
    if(ctrlState.padState[0] & BTN_START)
    	stateP1 |= BIT(JOYPAD_BUTTON_RUN);
    if(ctrlState.padState[0] & BTN_UP)
    	stateP1 |= BIT(JOYPAD_BUTTON_UP);
    if(ctrlState.padState[0] & BTN_DOWN)
    	stateP1 |= BIT(JOYPAD_BUTTON_DOWN);
    if(ctrlState.padState[0] & BTN_LEFT)
    	stateP1 |= BIT(JOYPAD_BUTTON_LEFT);
    if(ctrlState.padState[0] & BTN_RIGHT)
    	stateP1 |= BIT(JOYPAD_BUTTON_RIGHT);
    if(ctrlState.padState[0] & BTN_BUTTON_1)
    	stateP1 |= BIT(JOYPAD_BUTTON_I);
    if(ctrlState.padState[0] & BTN_BUTTON_2)
    	stateP1 |= BIT(JOYPAD_BUTTON_II);
    if(ctrlState.padState[0] & BTN_BUTTON_3)
    	stateP1 |= BIT(JOYPAD_BUTTON_III);
    if(ctrlState.padState[0] & BTN_BUTTON_4)
    	stateP1 |= BIT(JOYPAD_BUTTON_IV);
    if(ctrlState.padState[0] & BTN_BUTTON_5)
    	stateP1 |= BIT(JOYPAD_BUTTON_V);
    if(ctrlState.padState[0] & BTN_BUTTON_6)
    	stateP1 |= BIT(JOYPAD_BUTTON_VI);
    mInputBuf[0][0] = (stateP1 >> 0) & 0xff;
    mInputBuf[0][1] = (stateP1 >> 8) & 0xff;

    if(ctrlState.padState[1] & BTN_SELECT)
    	stateP2 |= BIT(JOYPAD_BUTTON_SELECT);
    if(ctrlState.padState[1] & BTN_START)
    	stateP2 |= BIT(JOYPAD_BUTTON_RUN);
    if(ctrlState.padState[1] & BTN_UP)
    	stateP2 |= BIT(JOYPAD_BUTTON_UP);
    if(ctrlState.padState[1] & BTN_DOWN)
    	stateP2 |= BIT(JOYPAD_BUTTON_DOWN);
    if(ctrlState.padState[1] & BTN_LEFT)
    	stateP2 |= BIT(JOYPAD_BUTTON_LEFT);
    if(ctrlState.padState[1] & BTN_RIGHT)
    	stateP2 |= BIT(JOYPAD_BUTTON_RIGHT);
    if(ctrlState.padState[1] & BTN_BUTTON_1)
    	stateP2 |= BIT(JOYPAD_BUTTON_I);
    if(ctrlState.padState[1] & BTN_BUTTON_2)
    	stateP2 |= BIT(JOYPAD_BUTTON_II);
    if(ctrlState.padState[1] & BTN_BUTTON_3)
    	stateP2 |= BIT(JOYPAD_BUTTON_III);
    if(ctrlState.padState[1] & BTN_BUTTON_4)
    	stateP2 |= BIT(JOYPAD_BUTTON_IV);
    if(ctrlState.padState[1] & BTN_BUTTON_5)
    	stateP2 |= BIT(JOYPAD_BUTTON_V);
    if(ctrlState.padState[1] & BTN_BUTTON_6)
    	stateP2 |= BIT(JOYPAD_BUTTON_VI);
    mInputBuf[1][0] = (stateP2 >> 0) & 0xff;
    mInputBuf[1][1] = (stateP2 >> 8) & 0xff;

	static int16_t soundBuf[0x10000];
	static MDFN_Rect rects[PCE_HEIGHT];
	rects[0].w = ~0;

	EmulateSpecStruct spec = {0};
	spec.surface = mSurface;
	spec.SoundRate = PCE_SOUND_RATE;
	spec.SoundBuf = soundBuf;
	spec.LineWidths = rects;
	spec.SoundBufMaxSize = sizeof(soundBuf) / 2;
	spec.SoundVolume = 1.0;
	spec.soundmultiplier = 1.0;
	spec.SoundBufSize = 0;
	spec.VideoFormatChanged = false;
	spec.SoundFormatChanged = false;

	if (memcmp(&mSavedPixFormat, &spec.surface->format, sizeof(MDFN_PixelFormat)))
	{
		LOGI("VideoFormatChanged");
		spec.VideoFormatChanged = TRUE;
		mSavedPixFormat = spec.surface->format;
	}

	if (spec.SoundRate != mSavedSoundRate)
	{
		LOGI("SoundFormatChanged: %f -> %f\n", mSavedSoundRate, spec.SoundRate);
		spec.SoundFormatChanged = true;
		mSavedSoundRate = spec.SoundRate;
	}

	mGame->Emulate(&spec);

    if(curBitmap)
    {
#define BUILD_PIXEL_RGB565(R,G,B) (((int) ((R)&0x1f) << 11) | ((int) ((G)&0x3f) << 5) | (int) ((B)&0x1f))
    	const uint32_t *src32 = mSurface->pixels;
    	uint16_t *dst16 = (uint16_t *)curBitmap->getBuffer();
		unsigned width  = spec.DisplayRect.w;
		unsigned height = spec.DisplayRect.h;

//		LOGI("frame dim: %d, %d\n", width, height);
		curBitmap->setDimensions(width, height);
		for(int y = 0; y < height; y++)
			for(int x = 0; x < width; x++)
			{
				uint32_t pixel = src32[(y * PCE_WIDTH) + x];
				int r = (pixel >> 16) & 0xff;
				int g = (pixel >> 8) & 0xff;
				int b = (pixel >> 0) & 0xff;
				dst16[(y * PCE_WIDTH) + x] = BUILD_PIXEL_RGB565(r >> 3, g >> 2, b >> 3);
			}

    }

    if(soundBuffer)
    {
    	if((spec.SoundBufSize * 2 * sizeof(short)) > mRomInfo.soundMaxBytesPerFrame)
    	{
    		LOGE("too many sound samples: %d > %d\n", spec.SoundBufSize * 2 * sizeof(short), mRomInfo.soundMaxBytesPerFrame);
    		*soundSampleByteCount = 0;
    		return;
    	}
    	memcpy(soundBuffer, spec.SoundBuf, spec.SoundBufSize * 2 * sizeof(short));
    	*soundSampleByteCount = spec.SoundBufSize * 2 * sizeof(short);
    }

#if 0
	// sound output verification
	{
		static int sampleCounter = 0;
		static int frameCounter = 0;

		sampleCounter += spec.SoundBufSize;
		frameCounter++;
		if(frameCounter == 60)
		{
			LOGI("PCE sound stats: %d\n", sampleCounter);
			sampleCounter = 0;
			frameCounter = 0;
		}
	}
#endif
}

bool getBoolFromString(const char *str)
{
	if(!strcasecmp(str, PLUGINOPT_TRUE))
		return true;
	else
		return false;
}

bool PCEEngine::setOption(const char *name, const char *value)
{
	if(!strcasecmp(name, PLUGINOPT_PCE_ENABLE_6BUTTON))
	{
		mEmulate6ButtonPad = getBoolFromString(value);
	}
	return false;
}

bool PCEEngine::addCheat(const char *cheat)
{
	char *scheat = strdup(cheat), *s_format, *s_addr, *s_val;
	uint32_t addr;
	uint8_t val;
	bool ret = false;

	s_format = strtok((char *)scheat, ";");
	if(!s_format)
	{
		LOGE("cheat parse error 1\n");
		goto addCheat_end;
	}
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
	if(strcasecmp(s_format, "raw"))
	{
		LOGE("unsupported format: %s\n", s_format);
		goto addCheat_end;
	}
	addr = strtol(s_addr, NULL, 16);
	val = strtol(s_val, NULL, 16);
	LOGI("code: %s, %04X:%02X\n", s_format, addr, val);

	MDFNI_AddCheat(strdup(""), addr, val, 0, 'R', 1, false);

	ret = true;

addCheat_end:
	free(scheat);
	return ret;
}

bool PCEEngine::removeCheat(const char *cheat)
{
	return false;
}

void PCEEngine::resetCheats()
{
	MDFN_FlushGameCheats(1);
}

extern "C" __attribute__((visibility("default")))
cEmulatorPlugin *createPlugin(t_emuAllocators *allocators)
{
	mallocInit(allocators);
	return new PCEEngine;
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
