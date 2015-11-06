#define GENESIS_SOUND_RATE		44100
#define GENESIS_MAX_WIDTH		512
#define GENESIS_MAX_HEIGHT		512
#define GENESIS_PITCH			(GENESIS_MAX_WIDTH * 2)

#include <android/log.h>
#include <assert.h>
#include <jni.h>
#include <stdlib.h>
#include <time.h>
//#include "prof.h"

#include "logging.h"
#include "retronCommon.h"

extern "C"
{
#include "shared.h"
}

void mallocInit(t_emuAllocators *allocators);

class GenesisEngine : public cEmulatorPlugin {
public:
	GenesisEngine();
	virtual ~GenesisEngine();

	virtual bool initialise(t_pluginInfo *info);
	virtual void destroy();
	virtual void reset();
	virtual t_romInfo *loadRomFile(const char *file, t_systemRegion systemRegion);
	t_romInfo *loadRomBuffer(const void *buf, int size, t_systemRegion systemRegion);
	virtual void unloadRom();
	virtual bool isNvmDirty();
	virtual int saveNvm(const char *file);
	virtual int saveNvmBuffer(void **buffer, int *size);
	virtual bool loadNvm(const char *file);
	virtual bool saveSnapshot(const char *file);
	virtual bool loadSnapshot(const char *file);
	virtual void runFrame(cEmuBitmap *curBitmap, t_emuControllerState ctrlState, short *soundBuffer, int *soundSampleByteCount);
	virtual bool setOption(const char *name, const char *value);
	virtual bool addCheat(const char *cheat);
	virtual bool removeCheat(const char *cheat);
	virtual void resetCheats();

private:

	t_romInfo *loadRomCommon(void *buf, int bufSize, t_systemRegion systemRegion, int systemType);
	void configDefaults();
	void updateSramCRC();
	bool readFile(const char *filename, void **buffer, int *size);
	bool writeFile(const char *filename, void *buffer, int size);

	static const int mSramSize = 0x10000;
	uint32 mSramCRC;
	bool mNvmDirty;
	bool mDisable6Button;
	bool mDisableFM;
	t_romInfo mRomInfo;
	int mLastCtrlConnect;
};

GenesisEngine::GenesisEngine()
{
	mSramCRC = -1;
	mNvmDirty = false;
	mLastCtrlConnect = 0;
	mDisable6Button = false;
	mDisableFM = false;
}

GenesisEngine::~GenesisEngine()
{

}

bool GenesisEngine::initialise(t_pluginInfo *info)
{
#ifdef PROFILE
	monstartup("libcore-genesis.so");
#endif

	configDefaults();

	// allocate global work bitmap
	memset(&bitmap, 0, sizeof (bitmap));
	bitmap.width  = GENESIS_MAX_WIDTH;
	bitmap.height = GENESIS_MAX_HEIGHT;
	bitmap.pitch = GENESIS_PITCH;
	bitmap.depth = 16;
	bitmap.viewport.w = 0;
	bitmap.viewport.h = 0;
	bitmap.viewport.x = 0;
	bitmap.viewport.y = 0;
	bitmap.viewport.changed = 3;
	bitmap.data = (uint8 *)memalign(64, bitmap.pitch * bitmap.height);
	assert(bitmap.data != NULL);

	info->maxWidth = GENESIS_MAX_WIDTH;
	info->maxHeight = GENESIS_MAX_HEIGHT;
	info->bitmapPitch = GENESIS_PITCH;
	info->supportBufferLoading = true;

    LOGI("initialised NEW genesis\n");
	return true;
}

void GenesisEngine::destroy()
{
#ifdef PROFILE
	moncleanup();
#endif

	audio_shutdown();
}

void GenesisEngine::reset()
{
	system_reset();
}

t_romInfo *GenesisEngine::loadRomCommon(void *buf, int bufSize, t_systemRegion systemRegion, int systemType)
{
	memset(&mRomInfo, 0, sizeof(t_romInfo));

	switch(systemRegion)
	{
	case SYS_REGION_USA:
		config.region_detect = 1;
		break;
	case SYS_REGION_EUR:
		config.region_detect = 2;
		break;
	case SYS_REGION_JAP:
		config.region_detect = 3;
		break;
	// REGION_AUTO
	default:
		config.region_detect = 0;
		break;
	}

	int size = load_rom(buf, bufSize, systemType);
	if(size <= 0)
	{
		LOGE("failed to load rom\n");
		return NULL;
	}

	mRomInfo.fps = (vdp_pal) ? 53203424.0 / (3420.0 * 313.0) : 53693175.0 / (3420.0 * 262.0);
	mRomInfo.aspectRatio = 4.0 / 3.0;
	mRomInfo.soundRate = GENESIS_SOUND_RATE;
	mRomInfo.soundMaxBytesPerFrame = ((mRomInfo.soundRate / 50) + 1) * 2 * sizeof(short);

	// SMS does not support multi-tap, hot swap
	if(system_hw == SYSTEM_PBC)
	{
		for(int i = 0; i < 2; i++)
			config.input[i].padtype = DEVICE_PAD2B;
		input.system[0] = SYSTEM_MS_GAMEPAD;
		input.system[1] = SYSTEM_MS_GAMEPAD;
	}

	LOGI("system_hw = %d, region_detect = %d\n", system_hw, config.region_detect);
	audio_init(GENESIS_SOUND_RATE);
	system_init();
	system_reset();

	/*
	 * The following games do not function correctly with 6-button emulation, and thus must have it disabled:
	 * 0x182
	 * Arch Rivals
	 * Clue
	 * Decap Attack
	 * Forgotten Worlds
	 * Golden Axe II
	 * IMG International Tour Tennis
	 * King of the Monsters
	 * Ms. Pac-Man
	 * Out of this World
	 * Sunset Riders
	 * ToeJam & Earl
	 * Todd's Adventures in Slime World
	 */
	const static char *FORCE_3BTN_GAMES[] = {
		"T-081056 00",
		"T-89016 -00",
		"MK1027  -00",
		"00004016-0",
		"00001122-00",
		"T-50836 -00",
		"T-48036 -00",
		"T-70106  00",
		"T-95026-00",
		"MK-1020 -0",
		"T-49216 -00",
		"T-83056 -00",
		"MK-1086-50",
		"G-4104 00",
		"T-103026-00",
	};
	mDisable6Button = false;
	for(int i = 0; i < (sizeof(FORCE_3BTN_GAMES) / sizeof(char *)); i++)
		if(strstr(rominfo.product, FORCE_3BTN_GAMES[i]))
		{
			LOGI("disabling 6-button controller for genesis\n");
			mDisable6Button = true;
			break;
		}

	LOGI("sram info: on=%d, custom=%d, detected=%d, start=%X, end=%X\n", sram.on, sram.custom, sram.detected, sram.start, sram.end);
	return &mRomInfo;
}

t_romInfo *GenesisEngine::loadRomFile(const char *file, t_systemRegion systemRegion)
{
	int systemType;
	if(!strcasecmp(&file[strlen(file)-4], ".SMS"))
		systemType = SYSTEM_PBC;
	else
		systemType = SYSTEM_GENESIS;
	LOGI("loadRomFile file: %s, system: %d\n", file, systemType);
	return loadRomCommon((void *)file, 0, systemRegion, systemType);
}

t_romInfo *GenesisEngine::loadRomBuffer(const void *buf, int size, t_systemRegion systemRegion)
{
	return loadRomCommon((void *)buf, size, systemRegion, SYSTEM_GENESIS);
}

void GenesisEngine::unloadRom()
{

}

bool GenesisEngine::readFile(const char *filename, void **buffer, int *size)
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

bool GenesisEngine::writeFile(const char *filename, void *buffer, int size)
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

bool GenesisEngine::isNvmDirty()
{
	if(mNvmDirty)
		return true;

	// As the CRC is quite slow, only process once per 30 frames
	static int frameCounter = 0;
	if(frameCounter++ % 30)
		return false;

	if(sram.on)
		if(mSramCRC != crc32(0, sram.sram, mSramSize))
		{
			LOGI("SRAM changed\n");
			mNvmDirty = true;
		}

	return mNvmDirty;
}

void GenesisEngine::updateSramCRC()
{
	mSramCRC = crc32(0, sram.sram, mSramSize);
}

int GenesisEngine::saveNvm(const char *file)
{
	if(sram.on)
	{
		if(writeFile(file, sram.sram, mSramSize) == false)
			return -1;
		updateSramCRC();
		mNvmDirty = false;
		return 1;
	}
	return 0;
}

bool GenesisEngine::loadNvm(const char *file)
{
	if(sram.on)
	{
		void *tmpBuf;
		int bufSize;
		if(readFile(file, &tmpBuf, &bufSize) == false)
			return false;
/*
		if(bufSize != mSramSize)
		{
			LOGE("SRAM save file size invalid\n");
			free(tmpBuf);
			return false;
		}
		memcpy(sram.sram, tmpBuf, mSramSize);
*/
		memcpy(sram.sram, tmpBuf, bufSize);
		free(tmpBuf);
		updateSramCRC();
		mNvmDirty = false;
	}
	return true;
}

int GenesisEngine::saveNvmBuffer(void **buffer, int *size)
{
	if(sram.on)
	{
		*buffer = (void *)malloc(mSramSize);
		assert(*buffer != NULL);
		memcpy(*buffer, sram.sram, mSramSize);
		*size = mSramSize;
		updateSramCRC();
		mNvmDirty = false;
		return 1;
	}
	return 0;
}

bool GenesisEngine::saveSnapshot(const char *file)
{
	uint8_t *snapshot = (uint8_t *)malloc(STATE_SIZE);
	assert(snapshot != NULL);

	int fileSize = state_save(snapshot);
	LOGI("state_save ret = %d\n", fileSize);

	bool ret = writeFile(file, snapshot, fileSize);
	free(snapshot);
	return ret;
}

bool GenesisEngine::loadSnapshot(const char *file)
{
	uint8_t *snapshot;
	int snapshotSize;

	if(readFile(file, (void **)&snapshot, &snapshotSize) == false)
		return false;

	int rv = state_load(snapshot);
	free(snapshot);
	if(rv <= 0)
		return false;

	return true;
}

void GenesisEngine::runFrame(cEmuBitmap *curBitmap, t_emuControllerState ctrlState, short *soundBuffer, int *soundSampleByteCount)
{
	if(mLastCtrlConnect != ctrlState.padConnectMask && system_hw != SYSTEM_PBC)
	{
		if(ctrlState.padConnectMask & ~3)
		{
			for(int i = 0; i < 4; i++)
				config.input[i].padtype = (mDisable6Button == true) ? DEVICE_PAD3B : DEVICE_PAD6B;
			input.system[0] = SYSTEM_TEAMPLAYER;
			input.system[1] = NO_SYSTEM;
		}
		else
		{
			for(int i = 0; i < 2; i++)
				config.input[i].padtype = (mDisable6Button == true) ? DEVICE_PAD3B : DEVICE_PAD6B;
			input.system[0] = SYSTEM_MD_GAMEPAD;
			input.system[1] = SYSTEM_MD_GAMEPAD;
		}
		io_init();
		mLastCtrlConnect = ctrlState.padConnectMask;
	}

	for(int i = 0; i < MAX_DEVICES; i++)
	{
		input.pad[i] = 0;
		input.analog[i][0] = 0;
		input.analog[i][1] = 0;
	}

	if((ctrlState.padConnectMask & ~3) && (system_hw != SYSTEM_PBC))
	{
		for(int i = 0; i < 4; i++)
		{
		    if(ctrlState.padState[i] & BTN_SELECT)
		    	input.pad[i] |= INPUT_MODE;
		    if(ctrlState.padState[i] & BTN_START)
		    	input.pad[i] |= INPUT_START;
		    if(ctrlState.padState[i] & BTN_UP)
		    	input.pad[i] |= INPUT_UP;
		    if(ctrlState.padState[i] & BTN_DOWN)
		    	input.pad[i] |= INPUT_DOWN;
		    if(ctrlState.padState[i] & BTN_LEFT)
		    	input.pad[i] |= INPUT_LEFT;
		    if(ctrlState.padState[i] & BTN_RIGHT)
		    	input.pad[i] |= INPUT_RIGHT;
			if(ctrlState.padState[i] & BTN_BUTTON_1)
				input.pad[i] |= INPUT_A;
			if(ctrlState.padState[i] & BTN_BUTTON_2)
				input.pad[i] |= INPUT_B;
			if(ctrlState.padState[i] & BTN_BUTTON_3)
				input.pad[i] |= INPUT_C;
			if(mDisable6Button == false)
			{
				if(ctrlState.padState[i] & BTN_BUTTON_4)
					input.pad[i] |= INPUT_X;
				if(ctrlState.padState[i] & BTN_BUTTON_5)
					input.pad[i] |= INPUT_Y;
				if(ctrlState.padState[i] & BTN_BUTTON_6)
					input.pad[i] |= INPUT_Z;
			}
		}
	}
	else
	{
		for(int i = 0; i < 2; i++)
		{
			if(ctrlState.padState[i] & BTN_SELECT)
				input.pad[i*4] |= INPUT_MODE;
			if(ctrlState.padState[i] & BTN_START)
				input.pad[i*4] |= INPUT_START;
			if(ctrlState.padState[i] & BTN_UP)
				input.pad[i*4] |= INPUT_UP;
			if(ctrlState.padState[i] & BTN_DOWN)
				input.pad[i*4] |= INPUT_DOWN;
			if(ctrlState.padState[i] & BTN_LEFT)
				input.pad[i*4] |= INPUT_LEFT;
			if(ctrlState.padState[i] & BTN_RIGHT)
				input.pad[i*4] |= INPUT_RIGHT;
			if(system_hw != SYSTEM_PBC)
			{
				if(ctrlState.padState[i] & BTN_BUTTON_1)
					input.pad[i*4] |= INPUT_A;
				if(ctrlState.padState[i] & BTN_BUTTON_2)
					input.pad[i*4] |= INPUT_B;
				if(ctrlState.padState[i] & BTN_BUTTON_3)
					input.pad[i*4] |= INPUT_C;
				if(mDisable6Button == false)
				{
					if(ctrlState.padState[i] & BTN_BUTTON_4)
						input.pad[i*4] |= INPUT_X;
					if(ctrlState.padState[i] & BTN_BUTTON_5)
						input.pad[i*4] |= INPUT_Y;
					if(ctrlState.padState[i] & BTN_BUTTON_6)
						input.pad[i*4] |= INPUT_Z;
				}
			}
			else
			{
				if(ctrlState.padState[i] & BTN_BUTTON_1)
					input.pad[i*4] |= INPUT_BUTTON1;
				if(ctrlState.padState[i] & BTN_BUTTON_2)
					input.pad[i*4] |= INPUT_BUTTON2;
			}
		}
	}

	bool skipFrame = (curBitmap == NULL) ? true : false;

	RAMCheatUpdate();
	system_frame(skipFrame);

	if(curBitmap)
	{
		int width = bitmap.viewport.w + (2 * bitmap.viewport.x);
		int height = bitmap.viewport.h + (2 * bitmap.viewport.y);
		curBitmap->setDimensions(width, height);

    	uint8 *dst = (uint8 *)curBitmap->getBuffer();
    	for(int y = 0; y < height; y++)
    		memcpy(&dst[y * GENESIS_PITCH], &bitmap.data[y * GENESIS_PITCH], bitmap.viewport.w * 2);
	}

	if(soundBuffer)
		*soundSampleByteCount = audio_update(soundBuffer) * 4;
//	LOGI("soundSampleByteCount = %d\n", *soundSampleByteCount);

#if 0
	// sound output verification
	{
		static int sampleCounter = 0;
		static int frameCounter = 0;

		sampleCounter += *soundSampleByteCount / 4;
		frameCounter++;
		if(frameCounter == 60)
		{
			LOGI("genesis sound stats: %d\n", sampleCounter);
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

bool GenesisEngine::setOption(const char *name, const char *value)
{
	if(!strcasecmp(name, PLUGINOPT_OVERSCAN))
	{
		config.overscan = (getBoolFromString(value)) ? 1 : 0;
	    bitmap.viewport.changed = 3;
/*
		if ((system_hw == SYSTEM_GG) && !config.gg_extra)
			bitmap.viewport.x = (config.overscan & 2) ? 14 : -48;
		else
			bitmap.viewport.x = (config.overscan & 2) * 7;
*/
		return true;
	}
	else if(!strcasecmp(name, PLUGINOPT_SMS_DISABLE_FM))
	{
		mDisableFM = getBoolFromString(value);
		config.ym2413_enabled = (mDisableFM) ? 0 : 1;
	}
	return false;
}

bool GenesisEngine::addCheat(const char *cheat)
{
	char *scheat = strdup(cheat), *format, *code;
	bool ret = false;

	format = strtok((char *)scheat, ";");
	if(!format)
	{
		LOGE("cheat parse error 1\n");
		goto addChear_end;
	}
	code = strtok(NULL, ";");
	if(!code)
	{
		LOGE("cheat parse error 2\n");
		goto addChear_end;
	}
	if(system_hw != SYSTEM_PBC)
	{
		if(strcasecmp(format, "Game Genie"))
		{
			LOGE("unsupported format: %s\n", format);
			goto addChear_end;
		}
	}
	else
	{
		if(strcasecmp(format, "Action Replay/GameShark"))
		{
			LOGE("unsupported format: %s\n", format);
			goto addChear_end;
		}
	}
	ret = (add_cheat(code) != 0) ? true : false;

addChear_end:
	free(scheat);
	return ret;
}

bool GenesisEngine::removeCheat(const char *cheat)
{
	return false;
}

void GenesisEngine::resetCheats()
{
	clear_cheats();
}

void GenesisEngine::configDefaults()
{
	memset(&config, 0, sizeof(config));

	// sound options
	config.psg_preamp     = 150;
	config.fm_preamp      = 100;
	config.hq_fm          = 1;
	config.psgBoostNoise  = 1;
	config.filter         = 0;
	config.lp_range       = 50;
	config.low_freq       = 880;
	config.high_freq      = 5000;
	config.lg             = 1.0;
	config.mg             = 1.0;
	config.hg             = 1.0;
	config.dac_bits 	 = 14;
	config.ym2413_enabled = 1;

	// system options
//	config.system         = 0; /* AUTO */
	config.region_detect  = 0; /* AUTO */
//	config.vdp_mode       = 0; /* AUTO */
//	config.master_clock   = 0; /* AUTO */
	config.force_dtack    = 0;
	config.addr_error     = 1;
//	config.bios           = 0;
	config.lock_on        = 0;
	config.hot_swap       = 0;

	// video options
	config.xshift   = 0;
	config.yshift   = 0;
	config.xscale   = 0;
	config.yscale   = 0;
	config.aspect   = 0;
	config.overscan = 0; // 3 == FULL
//	config.gg_extra = 0; // 1 = show extended Game Gear screen (256x192)
	#if defined(USE_NTSC)
	config.ntsc     = 1;
	#endif
//	config.vsync    = 1; // AUTO

	config.render   = 0;
	config.bilinear = 0;

	// controllers options
	config.gun_cursor[0]  = 1;
	config.gun_cursor[1]  = 1;
	config.invert_mouse   = 0;

	// menu options
	config.autoload     = 0;
	config.autocheat    = 0;
	config.s_auto       = 0;
	config.s_default    = 1;
	config.s_device     = 0;
//	config.l_device     = 0;
	config.bg_overlay   = 0;
	config.screen_w     = 658;
	config.bgm_volume   = 100.0;
	config.sfx_volume   = 100.0;

	// hot swap requires at least a first initialization
	config.hot_swap &= 1;
}

extern "C" void osd_input_update(void)
{
	// The input processing is done in runFrame()
}

extern "C" int load_archive(char *filename, unsigned char *buffer, int maxsize, char *extension)
{
	#define CHUNKSIZE   (0x10000)
	int size = 0;
	char in[CHUNKSIZE];

	FILE *fd = fopen(filename, "rb");
	if (!fd)
	{
		LOGI("failed to open file: %s\n", filename);
		return 0;
	}

	// Read first chunk
	fread(in, CHUNKSIZE, 1, fd);

	int left;
	// Get file size
	fseek(fd, 0, SEEK_END);
	size = ftell(fd);
	fseek(fd, 0, SEEK_SET);

	// size limit
	if(size > maxsize)
	{
		fclose(fd);
		LOGI("ERROR - File is too large.\n");
		return 0;
	}

	// filename extension
	if (extension)
	{
		memcpy(extension, &filename[strlen(filename) - 3], 3);
		extension[3] = 0;
	}

	// Read into buffer
	left = size;
	while (left > CHUNKSIZE)
	{
		fread(buffer, CHUNKSIZE, 1, fd);
		buffer += CHUNKSIZE;
		left -= CHUNKSIZE;
	}

	// Read remaining bytes
	fread(buffer, left, 1, fd);

	// Close file
	fclose(fd);

	// Return loaded ROM size
	return size;
}

char GG_ROM[256];
char AR_ROM[256];
char SK_ROM[256];
char SK_UPMEM[256];
char GG_BIOS[256];
char MS_BIOS_EU[256];
char MS_BIOS_JP[256];
char MS_BIOS_US[256];
char CD_BIOS_EU[256];
char CD_BIOS_US[256];
char CD_BIOS_JP[256];
char DEFAULT_PATH[1024];
char CD_BRAM_JP[256];
char CD_BRAM_US[256];
char CD_BRAM_EU[256];
char CART_BRAM[256];

extern "C" __attribute__((visibility("default")))
cEmulatorPlugin *createPlugin(t_emuAllocators *allocators)
{
	mallocInit(allocators);
	return new GenesisEngine;
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

extern "C" void raise(int p)
{
	LOGE("raise!\n");
	*(uint32_t *)0 = 0;
}
