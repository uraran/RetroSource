#define GBA_SOUND_RATE		32000

#include <android/log.h>
#include <jni.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
//#include "prof.h"

#include "logging.h"
#include "retronCommon.h"

#include "system.h"
#include "port.h"
#include "types.h"
#include "gba.h"
#include "gba-memory.h"
#include "sound.h"
#include "globals.h"
#include "cheats.h"

#define GBA_WIDTH		(240)
#define GBA_HEIGHT		(160)
#define GBA_PITCH		(512)

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

// Motion blur filter
void interframeInit();
void interframeCleanup();
void MotionBlurIB(u8 *srcPtr, u32 srcPitch, int width, int height);
void SmartIB(u8 *srcPtr, u32 srcPitch, int width, int height);

class GbaEngine : public cEmulatorPlugin {
public:
	GbaEngine();
	virtual ~GbaEngine();

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
	virtual void runFrame(cEmuBitmap *curBitmap, t_emuInputState ctrlState, short *soundBuffer, int *soundSampleByteCount);
	virtual bool setOption(const char *name, const char *value);
	virtual bool addCheat(const char *cheat);
	virtual bool removeCheat(const char *cheat);
	virtual void resetCheats();

	static uint32_t buttons;
	static volatile bool frameEndFlag;
	static cEmuBitmap *thisBitmap;
	static uint16_t sampleBuffer[GBA_SOUND_RATE * 2];
	static int sampleCurrentBytes;

	static void drawScreen();
private:
	
	t_romInfo *loadRomCommon();
	void configureInterframe();
	bool readFile(const char *filename, void **buffer, int *size);
	bool writeFile(const char *filename, void *buffer, int size);	

	t_romInfo mRomInfo;
	int mSnapshotSize;
	static bool mNeedInterframeFilter;
};

void mallocInit(t_emuAllocators *allocators);
void loadImagePreferences();

int GbaEngine::sampleCurrentBytes = 0;
uint16_t GbaEngine::sampleBuffer[GBA_SOUND_RATE * 2];
uint32_t GbaEngine::buttons = 0;
volatile bool GbaEngine::frameEndFlag = false;
bool GbaEngine::mNeedInterframeFilter = false;
cEmuBitmap *GbaEngine::thisBitmap = NULL;
uint8_t libretro_save_buf[0x20000 + 0x2000];	/* Workaround for broken-by-design GBA save semantics. */

GbaEngine::GbaEngine()
{
	mNeedInterframeFilter = false;
	mSnapshotSize = 0;
}

GbaEngine::~GbaEngine()
{

}

bool GbaEngine::initialise(t_pluginInfo *info)
{
#ifdef PROFILE
	monstartup("libcore-gba.so");
#endif

	info->maxWidth = GBA_WIDTH;
	info->maxHeight = 256;// GBA_HEIGHT;
	info->bitmapPitch = GBA_WIDTH * 2;
	info->supportBufferLoading = true;
    LOGI("initialised\n");
	return true;
}

void GbaEngine::destroy()
{
	if(mNeedInterframeFilter)
		interframeCleanup();
#ifdef PROFILE
	moncleanup();
#endif
}

void GbaEngine::reset()
{
	CPUReset();
}

t_romInfo *GbaEngine::loadRomCommon()
{
	memset(&mRomInfo, 0, sizeof(t_romInfo));

	cpuSaveType = 0;
	flashSize = 0x10000;
	enableRtc = false;
	mirroringEnable = false;
	
	loadImagePreferences();
	
	if(flashSize == 0x10000 || flashSize == 0x20000)
		flashSetSize(flashSize);
	
	if(enableRtc)
		rtcEnable(enableRtc);
	
	doMirroring(mirroringEnable);
	
	soundSetSampleRate(GBA_SOUND_RATE);
	CPUInit(0, false);
	CPUReset();
	soundReset();

	uint8_t *tmpBuf = (uint8_t*)malloc(2000000);
	mSnapshotSize = CPUWriteState(tmpBuf, 2000000);
	free(tmpBuf);
	LOGI("snapshot size = 0x%X\n", mSnapshotSize);
	
	configureInterframe();

	mRomInfo.fps = 16777216.0 / 280896.0;
	mRomInfo.aspectRatio = (double)GBA_WIDTH / (double)GBA_HEIGHT;
	mRomInfo.soundRate = GBA_SOUND_RATE;
	mRomInfo.soundMaxBytesPerFrame = sizeof(sampleBuffer);

	return &mRomInfo;
}

t_romInfo *GbaEngine::loadRomFile(const char *file, t_systemRegion systemRegion)
{
	int rv = CPULoadRom(file);
	if(!rv)
	{
		LOGI("failed to load gba rom: %s\n", file);
		return NULL;
	}

	return loadRomCommon();
}

t_romInfo *GbaEngine::loadRomBuffer(const void *buf, int size, t_systemRegion systemRegion)
{
	int rv = CPULoadRomBuffer((void *)buf, size);
	if(!rv)
	{
		LOGI("failed to load gba rom buffer\n");
		return NULL;
	}

	return loadRomCommon();
}

void GbaEngine::unloadRom()
{
	CPUCleanUp();
}		

bool GbaEngine::readFile(const char *filename, void **buffer, int *size)
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

bool GbaEngine::writeFile(const char *filename, void *buffer, int size)
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

bool GbaEngine::isNvmDirty()
{
	return saveDirty;
}

/*
 * NVM file format: 136kb data blob
 * 0x00000-0x1ffff = 128kb flash AND sram data (sram only uses first 64kb)
 * 0x20000-0x21FFF = 8kb eeprom data
 */
int GbaEngine::saveNvm(const char *file)
{
	if(writeFile(file, libretro_save_buf, sizeof(libretro_save_buf)) == false)
		return -1;
	saveDirty = false;
	return 1;
}

bool GbaEngine::loadNvm(const char *file)
{
	void *tmpBuf;
	int bufSize;
	if(readFile(file, &tmpBuf, &bufSize) == false)
		return false;
/*
	if(bufSize != sizeof(libretro_save_buf))
	{
		LOGE("NVM save file size invalid\n");
		free(tmpBuf);
		return false;
	}
*/
	memcpy(libretro_save_buf, tmpBuf, sizeof(libretro_save_buf));
	free(tmpBuf);
	saveDirty = false;
	return true;
}

int GbaEngine::saveNvmBuffer(void **buffer, int *size)
{
	*buffer = (void *)malloc(sizeof(libretro_save_buf));
	assert(*buffer != NULL);
	memcpy(*buffer, libretro_save_buf, sizeof(libretro_save_buf));
	*size = sizeof(libretro_save_buf);
	saveDirty = false;
	return 1;
}

bool GbaEngine::saveSnapshot(const char *file)
{
	uint8_t *snapshot = (uint8_t *)malloc(mSnapshotSize);
	assert(snapshot != NULL);

	int fileSize = CPUWriteState(snapshot, mSnapshotSize);
	assert(fileSize == mSnapshotSize);

	bool ret = writeFile(file, snapshot, fileSize);
	free(snapshot);
	return ret;
}

bool GbaEngine::loadSnapshot(const char *file)
{
	uint8_t *snapshot;
	int snapshotSize;

	if(readFile(file, (void **)&snapshot, &snapshotSize) == false)
		return false;

	bool ret = CPUReadState(snapshot, snapshotSize);
	free(snapshot);
	return ret;
}

void GbaEngine::runFrame(cEmuBitmap *curBitmap, t_emuInputState ctrlState, short *soundBuffer, int *soundSampleByteCount)
{
	if(curBitmap == NULL)
	{
		renderEnabled = false;
		thisBitmap = NULL;
	}
	else
	{
		thisBitmap = curBitmap;
		thisBitmap->setDimensions(GBA_WIDTH, GBA_HEIGHT);
		renderEnabled = true;
	}
	
	joy = 0;
    if(ctrlState.padState[0] & BTN_SELECT)
    	joy |= KEYM_SELECT;
    if(ctrlState.padState[0] & BTN_START)
    	joy |= KEYM_START;
    if(ctrlState.padState[0] & BTN_UP)
    	joy |= KEYM_UP;
    if(ctrlState.padState[0] & BTN_DOWN)
    	joy |= KEYM_DOWN;
    if(ctrlState.padState[0] & BTN_LEFT)
    	joy |= KEYM_LEFT;
    if(ctrlState.padState[0] & BTN_RIGHT)
    	joy |= KEYM_RIGHT;
    if(ctrlState.padState[0] & BTN_BUTTON_1)
    	joy |= KEYM_A;
    if(ctrlState.padState[0] & BTN_BUTTON_2)
    	joy |= KEYM_B;
    if(ctrlState.padState[0] & BTN_BUTTON_LT)
    	joy |= KEYM_L;
    if(ctrlState.padState[0] & BTN_BUTTON_RT)
    	joy |= KEYM_R;	
	
    frameEndFlag = false;
	sampleCurrentBytes = 0;
//	LOGI("frame start\n");
	while(1)
	{
		CPULoop();
		if(frameEndFlag == true)
			break;
//		LOGI("emuMain returned without rendering a frame!\n");
	}
//	LOGI("frame end\n");
	sound_flush();
	if(soundBuffer)
	{
		memcpy(soundBuffer, sampleBuffer, sampleCurrentBytes);
		*soundSampleByteCount = sampleCurrentBytes;
	}
	
#if 0
	// sound output verification
	{
		static int sampleCounter = 0;
		static int frameCounter = 0;

		sampleCounter += sampleCurrentBytes / 4;
		frameCounter++;
		if(frameCounter == 60)
		{
			LOGI("gameboy sound stats: %d\n", sampleCounter);
			sampleCounter = 0;
			frameCounter = 0;
		}
	}
#endif
}	

void GbaEngine::drawScreen()
{
	if(frameEndFlag)
	{
		LOGE("rendering called multiple times this iteration!\n");
		return;
	}

	if(thisBitmap)
	{
		thisBitmap->setDimensions(GBA_WIDTH, GBA_HEIGHT);
		
		if(mNeedInterframeFilter)
		{
			MotionBlurIB((uint8_t *)pix, GBA_PITCH, GBA_WIDTH, GBA_HEIGHT);
		}

		uint8_t *dst = (uint8_t *)thisBitmap->getBuffer();
		for(int i = 0; i < GBA_HEIGHT; i++)
			memcpy(&dst[i * GBA_WIDTH * sizeof(short)], &pix[i * (GBA_PITCH / 2)], GBA_WIDTH * sizeof(short));
	}
	frameEndFlag = true;
}

bool GbaEngine::setOption(const char *name, const char *value)
{
	return false;
}

bool GbaEngine::addCheat(const char *cheat)
{
	char *scheat = strdup(cheat), *s_format, *s_addr, *s_val;
	uint32_t addr;
	uint32_t val;
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

	addr = strtoll(s_addr, NULL, 16);
	val = strtoll(s_val, NULL, 16);
	LOGI("code: %s, %08X:%08X\n", s_format, addr, val);

	char combinedCode[32];
	if(!strcasecmp(s_format, "CB"))
	{
		sprintf(combinedCode, "%08X %04X", addr, val);
		ret = cheatsAddCBACode(combinedCode, "");
	}
	else if(!strcasecmp(s_format, "AR12"))
	{
		sprintf(combinedCode, "%08X%08X", addr, val);
		ret = cheatsAddGSACode(combinedCode, "", false, false);
	}
	else if(!strcasecmp(s_format, "AR34"))
	{
		sprintf(combinedCode, "%08X%08X", addr, val);
		ret = cheatsAddGSACode(combinedCode, "", true, false);
	}
	else if(!strcasecmp(s_format, "PAR"))
	{
		sprintf(combinedCode, "%08X%08X", addr, val);
		ret = cheatsAddGSACode(combinedCode, "", false, true);
	}
	else
	{
		LOGE("unsupported format: %s\n", s_format);
		ret = false;
	}

	if(ret == true)
		cheatsEnabled = true;

	LOGI("cheat add ret: %d\n", ret);
addCheat_end:
	free(scheat);
	return ret;
}

bool GbaEngine::removeCheat(const char *cheat)
{
	return false;
}

void GbaEngine::resetCheats()
{
	cheatsDeleteAll(true);
	cheatsEnabled = false;
}

extern "C" __attribute__((visibility("default")))
cEmulatorPlugin *createPlugin(t_emuAllocators *allocators)
{
	mallocInit(allocators);
	return new GbaEngine;
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

void systemOnWriteDataToSoundBuffer(int16_t *finalWave, int length)
{
//	LOGI("systemOnWriteDataToSoundBuffer - %d, %d\n", GbaEngine::sampleCurrentBytes, length * sizeof(short));
	uint8_t *dst = (uint8_t *)GbaEngine::sampleBuffer;
	memcpy(&dst[GbaEngine::sampleCurrentBytes], finalWave, length * sizeof(short));
	GbaEngine::sampleCurrentBytes += length * sizeof(short);
}

void systemDrawScreen()
{
	GbaEngine::drawScreen();
}
/*
void systemMessage(const char* str, ...)
{
   LOGI(str);
}
*/
void GbaEngine::configureInterframe()
{
	const static char *needInterframeGames[] = {
		"B9AJ",		// Kunio Kun Nekketsu Collection 1 (Japan)
		"B9BJ",		// Kunio Kun Nekketsu Collection 2 (Japan)
		"B9CJ",		// Kunio Kun Nekketsu Collection 3 (Japan)
		"BJCJ",		// Moero!! Jaleco Collection (Japan)
		"AYCE",		// Phantasy Star Collection (USA)
		"AYCP",		// Phantasy Star Collection (Europe)
		NULL,
	};
	mNeedInterframeFilter = false;

	char gameID[5];
	gameID[0] = rom[0xac];
	gameID[1] = rom[0xad];
	gameID[2] = rom[0xae];
	gameID[3] = rom[0xaf];
	gameID[4] = 0;

	for(int i = 0; needInterframeGames[i] != NULL; i++)
	{
		if(!strcmp(needInterframeGames[i], gameID))
		{
			LOGI("enabling interframe filter\n");
			mNeedInterframeFilter = true;
			return;
		}
	}
}

typedef struct  {
	char romid[5];
	int flashSize;
	int saveType;
	int rtcEnabled;
	int mirroringEnabled;
	int useBios;
} ini_t;

static const ini_t gbaover[256] = {
	//romtitle,							    	romid	flash	save	rtc	mirror	bios
	{/*"2 Games in 1 - Dragon Ball Z - The Legacy of Goku I & II (USA)",*/	"BLFE",	0,	1,	0,	0,	0},
	{/*"2 Games in 1 - Dragon Ball Z - Buu's Fury + Dragon Ball GT - Transformation (USA)",*/ "BUFE", 0, 1, 0, 0, 0},
	{/*"Boktai - The Sun Is in Your Hand (Europe)(En,Fr,De,Es,It)",*/		"U3IP",	0,	0,	1,	0,	0},
	{/*"Boktai - The Sun Is in Your Hand (USA)",*/				"U3IE",	0,	0,	1,	0,	0},
	{/*"Boktai 2 - Solar Boy Django (USA)",	*/				"U32E",	0,	0,	1,	0,	0},
	{/*"Boktai 2 - Solar Boy Django (Europe)(En,Fr,De,Es,It)",*/		"U32P",	0,	0,	1,	0,	0},
	{/*"Bokura no Taiyou - Taiyou Action RPG (Japan)",*/			"U3IJ",	0,	0,	1,	0,	0},
	{/*"Card e-Reader+ (Japan)",*/						"PSAJ",	131072,	0,	0,	0,	0},
	{/*"Classic NES Series - Bomberman (USA, Europe)",*/			"FBME",	0,	1,	0,	1,	0},
	{/*"Classic NES Series - Castlevania (USA, Europe)",*/			"FADE",	0,	1,	0,	1,	0},
	{/*"Classic NES Series - Donkey Kong (USA, Europe)",*/			"FDKE",	0,	1,	0,	1,	0},
	{/*"Classic NES Series - Dr. Mario (USA, Europe)",	*/		"FDME",	0,	1,	0,	1,	0},
	{/*"Classic NES Series - Excitebike (USA, Europe)",	*/		"FEBE",	0,	1,	0,	1,	0},
	{/*"Classic NES Series - Legend of Zelda (USA, Europe)",*/			"FZLE",	0,	1,	0,	1,	0},
	{/*"Classic NES Series - Ice Climber (USA, Europe)",	*/		"FICE",	0,	1,	0,	1,	0},
	{/*"Classic NES Series - Metroid (USA, Europe)",	*/			"FMRE",	0,	1,	0,	1,	0},
	{/*"Classic NES Series - Pac-Man (USA, Europe)",	*/			"FP7E",	0,	1,	0,	1,	0},
	{/*"Classic NES Series - Super Mario Bros. (USA, Europe)",*/		"FSME",	0,	1,	0,	1,	0},
	{/*"Classic NES Series - Xevious (USA, Europe)",		*/		"FXVE",	0,	1,	0,	1,	0},
	{/*"Classic NES Series - Zelda II - The Adventure of Link (USA, Europe)",*/	"FLBE",	0,	1,	0,	1,	0},
	{/*"Digi Communication 2 - Datou! Black Gemagema Dan (Japan)",	*/	"BDKJ",	0,	1,	0,	0,	0},
	{/*"e-Reader (USA)",						*/	"PSAE",	131072,	0,	0,	0,	0},
	{/*"Dragon Ball GT - Transformation (USA)",	*/			"BT4E",	0,	1,	0,	0,	0},
	{/*"Dragon Ball Z - Buu's Fury (USA)",		*/			"BG3E",	0,	1,	0,	0,	0},
	{/*"Dragon Ball Z - Taiketsu (Europe)(En,Fr,De,Es,It)",		*/	"BDBP",	0,	1,	0,	0,	0},
	{/*"Dragon Ball Z - Taiketsu (USA)",				*/	"BDBE",	0,	1,	0,	0,	0},
	{/*"Dragon Ball Z - The Legacy of Goku II International (Japan)",	*/	"ALFJ",	0,	1,	0,	0,	0},
	{/*"Dragon Ball Z - The Legacy of Goku II (Europe)(En,Fr,De,Es,It)",	*/"ALFP", 0,	1,	0,	0,	0},
	{/*"Dragon Ball Z - The Legacy of Goku II (USA)",		*/		"ALFE",	0,	1,	0,	0,	0},
	{/*"Dragon Ball Z - The Legacy Of Goku (Europe)(En,Fr,De,Es,It)",	*/	"ALGP",	0,	1,	0,	0,	0},
	{/*"Dragon Ball Z - The Legacy of Goku (USA)",		*/		"ALGE",	131072,	1,	0,	0,	0},
	{/*"F-Zero - Climax (Japan)",				*/		"BFTJ",	131072,	0,	0,	0,	0},
	{/*"Famicom Mini Vol. 01 - Super Mario Bros. (Japan)",	*/		"FMBJ",	0,	1,	0,	1,	0},
	{/*"Famicom Mini Vol. 12 - Clu Clu Land (Japan)",	*/			"FCLJ",	0,	1,	0,	1,	0},
	{/*"Famicom Mini Vol. 13 - Balloon Fight (Japan)",	*/		"FBFJ",	0,	1,	0,	1,	0},
	{/*"Famicom Mini Vol. 14 - Wrecking Crew (Japan)",	*/		"FWCJ",	0,	1,	0,	1,	0},
	{/*"Famicom Mini Vol. 15 - Dr. Mario (Japan)",		*/		"FDMJ",	0,	1,	0,	1,	0},
	{/*"Famicom Mini Vol. 16 - Dig Dug (Japan)",	*/			"FTBJ",	0,	1,	0,	1,	0},
	{/*"Famicom Mini Vol. 17 - Takahashi Meijin no Boukenjima (Japan)",*/	"FTBJ",	0,	1,	0,	1,	0},
	{/*"Famicom Mini Vol. 18 - Makaimura (Japan)",	*/			"FMKJ",	0,	1,	0,	1,	0},
	{/*"Famicom Mini Vol. 19 - Twin Bee (Japan)",	*/			"FTWJ",	0,	1,	0,	1,	0},
	{/*"Famicom Mini Vol. 20 - Ganbare Goemon! Karakuri Douchuu (Japan)",*/	"FGGJ",	0,	1,	0,	1,	0},
	{/*"Famicom Mini Vol. 21 - Super Mario Bros. 2 (Japan)",*/			"FM2J",	0,	1,	0,	1,	0},
	{/*"Famicom Mini Vol. 22 - Nazo no Murasame Jou (Japan)",*/			"FNMJ",	0,	1,	0,	1,	0},
	{/*"Famicom Mini Vol. 23 - Metroid (Japan)",*/				"FMRJ",	0,	1,	0,	1,	0},
	{/*"Famicom Mini Vol. 24 - Hikari Shinwa - Palthena no Kagami (Japan)",*/	"FPTJ",	0,	1,	0,	1,	0},
	{/*"Famicom Mini Vol. 25 - The Legend of Zelda 2 - Link no Bouken (Japan)",*/"FLBJ",0,	1,	0,	1,	0},
	{/*"Famicom Mini Vol. 26 - Famicom Mukashi Banashi - Shin Onigashima - Zen Kou Hen (Japan)",*/"FFMJ",0,1,0,	1,	0},
	{/*"Famicom Mini Vol. 27 - Famicom Tantei Club - Kieta Koukeisha - Zen Kou Hen (Japan)",*/"FTKJ",0,1,0,	1,	0},
	{/*"Famicom Mini Vol. 28 - Famicom Tantei Club Part II - Ushiro ni Tatsu Shoujo - Zen Kou Hen (Japan)",*/"FTUJ",0,1,0,1,0},
	{/*"Famicom Mini Vol. 29 - Akumajou Dracula (Japan)",*/			"FADJ",	0,	1,	0,	1,	0},
	{/*"Famicom Mini Vol. 30 - SD Gundam World - Gachapon Senshi Scramble Wars (Japan)",*/"FSDJ",0,1,	0,	1,	0},
	{/*"Game Boy Wars Advance 1+2 (Japan)",*/					"BGWJ",	131072,	0,	0,	0,	0},
	{/*"Golden Sun - The Lost Age (USA)",*/					"AGFE",	65536,	0,	0,	1,	0},
	{/*"Golden Sun (USA)",*/							"AGSE",	65536,	0,	0,	1,	0},
	{/*"Koro Koro Puzzle - Happy Panechu! (Japan)",*/				"KHPJ",	0,	4,	0,	0,	0},
	{/*"Mario vs. Donkey Kong (Europe)",*/					"BM5P",	0,	3,	0,	0,	0},
	{/*"Pocket Monsters - Emerald (Japan)",*/					"BPEJ",	131072,	0,	1,	0,	0},
	{/*"Pocket Monsters - Fire Red (Japan)",*/					"BPRJ",	131072,	0,	0,	0,	0},
	{/*"Pocket Monsters - Leaf Green (Japan)",*/				"BPGJ",	131072,	0,	0,	0,	0},
	{/*"Pocket Monsters - Ruby (Japan)",*/					"AXVJ",	131072,	0,	1,	0,	0},
	{/*"Pocket Monsters - Sapphire (Japan)",*/					"AXPJ",	131072,	0,	1,	0,	0},
	{/*"Pokemon Mystery Dungeon - Red Rescue Team (USA, Australia)",*/		"B24E",	131072,	0,	0,	0,	0},
	{/*"Pokemon Mystery Dungeon - Red Rescue Team (En,Fr,De,Es,It)",*/		"B24P",	131072,	0,	0,	0,	0},
	{/*"Pokemon - Blattgruene Edition (Germany)",*/				"BPGD",	131072,	0,	0,	0,	0},
	{/*"Pokemon - Edicion Rubi (Spain)",*/					"AXVS",	131072,	0,	1,	0,	0},
	{/*"Pokemon - Edicion Esmeralda (Spain)",*/					"BPES",	131072,	0,	1,	0,	0},
	{/*"Pokemon - Edicion Rojo Fuego (Spain)",*/				"BPRS",	131072,	1,	0,	0,	0},
	{/*"Pokemon - Edicion Verde Hoja (Spain)",*/				"BPGS",	131072,	1,	0,	0,	0},
	{/*"Pokemon - Eidicion Zafiro (Spain)",*/					"AXPS",	131072,	0,	1,	0,	0},
	{/*"Pokemon - Emerald Version (USA, Europe)",*/				"BPEE",	131072,	0,	1,	0,	0},
	{/*"Pokemon - Feuerrote Edition (Germany)",*/				"BPRD",	131072,	0,	0,	0,	0},
	{/*"Pokemon - Fire Red Version (USA, Europe)",*/				"BPRE",	131072,	0,	0,	0,	0},
	{/*"Pokemon - Leaf Green Version (USA, Europe)",*/				"BPGE",	131072,	0,	0,	0,	0},
	{/*"Pokemon - Rubin Edition (Germany)",*/					"AXVD",	131072,	0,	1,	0,	0},
	{/*"Pokemon - Ruby Version (USA, Europe)",*/				"AXVE",	131072,	0,	1,	0,	0},
	{/*"Pokemon - Sapphire Version (USA, Europe)",*/				"AXPE",	131072,	0,	1,	0,	0},
	{/*"Pokemon - Saphir Edition (Germany)",*/					"AXPD",	131072,	0,	1,	0,	0},
	{/*"Pokemon - Smaragd Edition (Germany)",*/					"BPED",	131072,	0,	1,	0,	0},
	{/*"Pokemon - Version Emeraude (France)",*/					"BPEF",	131072,	0,	1,	0,	0},
	{/*"Pokemon - Version Rouge Feu (France)",*/				"BPRF",	131072,	0,	0,	0,	0},
	{/*"Pokemon - Version Rubis (France)",*/					"AXVF",	131072,	0,	1,	0,	0},
	{/*"Pokemon - Version Saphir (France)",*/					"AXPF",	131072,	0,	1,	0,	0},
	{/*"Pokemon - Version Vert Feuille (France)",*/				"BPGF",	131072,	0,	0,	0,	0},
	{/*"Pokemon - Versione Rubino (Italy)",*/					"AXVI",	131072,	0,	1,	0,	0},
	{/*"Pokemon - Versione Rosso Fuoco (Italy)",*/				"BPRI",	131072,	0,	0,	0,	0},
	{/*"Pokemon - Versione Smeraldo (Italy)",*/					"BPEI",	131072,	0,	1,	0,	0},
	{/*"Pokemon - Versione Verde Foglia (Italy)",*/				"BPGI",	131072,	0,	0,	0,	0},
	{/*"Pokemon - Versione Zaffiro (Italy)",*/					"AXPI",	131072,	0,	1,	0,	0},
	{/*"Rockman EXE 4.5 - Real Operation (Japan)",*/				"BR4J",	0,	0,	1,	0,	0},
	{/*"Rocky (Europe)(En,Fr,De,Es,It)",*/					"AROP",	0,	1,	0,	0,	0},
	{/*"Sennen Kazoku (Japan)",*/						"BKAJ",	131072,	0,	1,	0,	0},
	{/*"Shin Bokura no Taiyou - Gyakushuu no Sabata (Japan)",*/			"U33J",	0,	1,	1,	0,	0},
	{/*"Super Mario Advance 4 (Japan)",*/					"AX4J",	131072,	0,	0,	0,	0},
	{/*"Super Mario Advance 4 - Super Mario Bros. 3 (Europe)(En,Fr,De,Es,It)",*/"AX4P",	131072,	0,	0,	0,	0},
	{/*"Super Mario Advance 4 - Super Mario Bros 3 - Super Mario Advance 4 v1.1 (USA)",*/"AX4E",131072,0,0,0,0},
	{/*"Top Gun - Combat Zones (USA)(En,Fr,De,Es,It)",*/			"A2YE",	0,	5,	0,	0,	0},
	{/*"Yoshi no Banyuuinryoku (Japan)",*/					"KYGJ",	0,	4,	0,	0,	0},
	{/*"Yoshi - Topsy-Turvy (USA)",*/						"KYGE",	0,	1,	0,	0,	0},
	{/*"Yu-Gi-Oh! GX - Duel Academy (USA)",	*/				"BYGE",	0,	2,	0,	0,	1},
	{/*"Yu-Gi-Oh! - Ultimate Masters - 2006 (Europe)(En,Jp,Fr,De,Es,It)",*/	"BY6P",	0,	2,	0,	0,	0},
	{/*"Zoku Bokura no Taiyou - Taiyou Shounen Django (Japan)",*/		"U32J",	0,	0,	1,	0,	0}
};

void loadImagePreferences()
{
	char buffer[5];
	buffer[0] = rom[0xac];
	buffer[1] = rom[0xad];
	buffer[2] = rom[0xae];
	buffer[3] = rom[0xaf];
	buffer[4] = 0;
	LOGI("GameID in ROM is: %s\n", buffer);

	bool found = false;
	int found_no = 0;

	for(int i = 0; i < 256; i++)
	{
		if(!strcmp(gbaover[i].romid, buffer))
		{
			found = true;
			found_no = i;
         	break;
		}
	}

	if(found)
	{
		LOGI("Found ROM in vba-over list.\n");

		enableRtc = gbaover[found_no].rtcEnabled;

		if(gbaover[found_no].flashSize != 0)
			flashSize = gbaover[found_no].flashSize;
		else
			flashSize = 65536;

		cpuSaveType = gbaover[found_no].saveType;

		mirroringEnable = gbaover[found_no].mirroringEnabled;
	}

	LOGI("RTC = %d.\n", enableRtc);
	LOGI("flashSize = %d.\n", flashSize);
	LOGI("cpuSaveType = %d.\n", cpuSaveType);
	LOGI("mirroringEnable = %d.\n", mirroringEnable);
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
