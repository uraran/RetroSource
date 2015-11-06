#ifndef _RETRON_COMMON_H
#define _RETRON_COMMON_H

typedef struct
{
	double soundRate;
	int soundMaxBytesPerFrame;
	double fps;
	double aspectRatio;
} t_romInfo;

typedef struct
{
	int maxWidth;
	int maxHeight;
	int bitmapPitch;
	bool supportBufferLoading;
} t_pluginInfo;

typedef enum
{
	SYS_REGION_AUTO = 0,
	SYS_REGION_USA = 1,
	SYS_REGION_EUR = 2,
	SYS_REGION_JAP = 3,
} t_systemRegion;

typedef enum
{
	BTN_UP 			= (1 << 0),
	BTN_DOWN 		= (1 << 1),
	BTN_LEFT 		= (1 << 2),
	BTN_RIGHT 		= (1 << 3),
	BTN_START 		= (1 << 4),
	BTN_SELECT 		= (1 << 5),
	BTN_BUTTON_1 	= (1 << 6),
	BTN_BUTTON_2 	= (1 << 7),
	BTN_BUTTON_3 	= (1 << 8),
	BTN_BUTTON_4 	= (1 << 9),
	BTN_BUTTON_5 	= (1 << 10),
	BTN_BUTTON_6 	= (1 << 11),
	BTN_BUTTON_LT	= (1 << 12),
	BTN_BUTTON_RT	= (1 << 13),
} t_emuButtons;

typedef enum
{
	INPUT_SNES_MOUSE 			= 100,
	INPUT_FAMI_KEYBOARD 		= 101,
	INPUT_RAW_PORTS 			= 102,
} t_emuInputSpecialDevice;

typedef struct
{
	t_emuInputSpecialDevice type;
	uint32_t stateLen;
} t_emuInputSpecialInputHdr;

#define MOUSE_BUTTON_LEFT		0x01
#define MOUSE_BUTTON_RIGHT		0x02
typedef struct
{
	t_emuInputSpecialInputHdr hdr;
	uint8_t connected;
	uint8_t buttons;
	uint16_t xoff;
	uint16_t yoff;
} t_emuInputMouseState;

typedef struct
{
	t_emuInputSpecialInputHdr hdr;
	uint8_t data[9];
} t_emuInputFamiKeyboardState;

typedef struct
{
	t_emuInputSpecialInputHdr hdr;
	uint8_t nesD2Strobed;
	uint8_t nesD3Strobed;
	uint8_t famiP1D1Strobed;
	uint8_t famiP2D1Strobed;
	uint8_t famiRawState;
} t_emuInputRawPortState;

typedef struct
{
	int padConnectMask;
	unsigned int padState[5];
	int specialStateNum;
	t_emuInputSpecialInputHdr **specialStates;
} t_emuInputState;

#define PLUGINOPT_OVERSCAN			"opt_overscan"
#define PLUGINOPT_AUDIO_LOWLATENCY	"gameset_audio_lowlatency"

// Gameboy plugin specific
#define PLUGINOPT_GB_SGB_BORDER		"gameset_gb_sgb_borders"
#define PLUGINOPT_GB_HWTYPE			"gameset_gb_hwtype"
#define PLUGINOPT_GB_PALETTE		"gameset_gb_palette"

// SMS plugin specific
#define PLUGINOPT_SMS_ENABLE_FM		"gameset_sms_enable_fm"

// PCE plugin specific
#define PLUGINOPT_PCE_ENABLE_6BUTTON "gameset_pce_enable_6_button"

// NES plugin specific
#define PLUGINOPT_NES_ENABLE_VAUSFILTER "gameset_nes_enable_vausfilter"
#define PLUGINOPT_NES_FDS_SWITCH_SIDE	"gameset_nes_fds_switch_side"
#define PLUGINOPT_NES_MICROPHONE		"gameset_nes_set_microphone"

#define PLUGINOPT_TRUE				"true"
#define PLUGINOPT_FALSE				"false"

// emulator plugins are provided custom memory allocation functions by the engine, which use memory pools to facilitate complete memory clean-up after
// a plugin has been unloaded
typedef struct
{
	void* (*p_malloc)(size_t size);
	void (*p_free)(void *ptr);
	void* (*p_calloc)(size_t nmemb, size_t size);
	void* (*p_realloc)(void *ptr, size_t size);
	void* (*p_memalign)(size_t alignment, size_t size);
} t_emuAllocators;

typedef void (*t_statusUpdateCb)(void *args, int percentage);

class cEmuBitmap;
class cEmulatorPlugin
{
public:

	virtual bool initialise(t_pluginInfo *info) = 0;
	virtual void destroy() = 0;

	virtual void reset() = 0;
	virtual t_romInfo *loadRomFile(const char *file, t_systemRegion systemRegion) = 0;
	virtual t_romInfo *loadRomBuffer(const void *buf, int size, t_systemRegion systemRegion) // default fail-safe implementation provided; only plugin's that implement buffer support need provide their own implementation
	{
		return NULL;
	}
	virtual void unloadRom() = 0;

	/*
	 * Emulate a single frame
	 *
	 * bitmap               - bitmap data for this frame will be copied to the buffer pointed to by bitmap.data, and width/height fields will be updated.
	 *                        if bitmap == NULL, skip rendering for this frame
	 * ctrlState            - current controller/mouse state
	 * soundBuffer          - points to sample buffer provided by engine, where plugin should write sample data for the current frame
	 * soundSampleByteCount - plugin will set this value to the number of sample bytes placed in the soundBuffer for the current frame
	 */
	virtual void runFrame(cEmuBitmap *bitmap, t_emuInputState ctrlState, short *soundBuffer, int *soundSampleByteCount) = 0;

	// save/load non-volatile memory such as SRAM etc
	virtual bool isNvmDirty() = 0;
	virtual int saveNvm(const char *file) = 0; // ret < 0 = fail, 0 = no NVM, 1 = success
	virtual int saveNvmBuffer(void **buffer, int *size) = 0;
	virtual bool loadNvm(const char *file) = 0;

	// save/load full emulator state snapshots
	virtual bool saveSnapshot(const char *file) = 0;
	virtual bool loadSnapshot(const char *file) = 0;

	virtual bool setOption(const char *name, const char *value) = 0;

	// cheats
	virtual bool addCheat(const char *cheat) = 0;
	virtual bool removeCheat(const char *cheat) = 0;
	virtual void resetCheats() = 0;
};

class cEmuBitmap
{
public:

	typedef enum
	{
		BITMAP_FORMAT_RGB565 = 0,
		BITMAP_FORMAT_RGBA5551,
		BITMAP_FORMAT_RGBA8888,
		BITMAP_FORMAT_RGBA4444,
	} BitmapFormat;

	cEmuBitmap(int maxBufSize = 0, BitmapFormat format = BITMAP_FORMAT_RGB565, int width = 0, int height = 0, void *manualBuffer = NULL)
	{
		if(manualBuffer != NULL)
		{
			mManagedBuffer = false;
			mData = manualBuffer;
		}
		else
		{
			mManagedBuffer = true;
			if(maxBufSize)
			{
				mData = (void *)malloc(maxBufSize);
				assert(mData != NULL);
			}
			else
				mData = NULL;
		}
		mMaxBufSize = maxBufSize;
		mFormat = format;
		mWidth = width;
		mHeight = height;
	}
	~cEmuBitmap()
	{
		if(mManagedBuffer && mData)
			free(mData);
	}

	// object copying code
	cEmuBitmap(const cEmuBitmap &cSource)
	{
		mMaxBufSize = cSource.mMaxBufSize;
		mWidth = cSource.mWidth;
		mHeight = cSource.mHeight;
		mFormat = cSource.mFormat;
		// copies must use managed buffers!
		mManagedBuffer = true;
		mData = (void *)malloc(mMaxBufSize);
		assert(mData != NULL);
		memcpy(mData, cSource.mData, mMaxBufSize);
	}
	cEmuBitmap& operator= (const cEmuBitmap &cSource)
	{
		if(this == &cSource) // for self-assignment
			return *this;

		mMaxBufSize = cSource.mMaxBufSize;
		mWidth = cSource.mWidth;
		mHeight = cSource.mHeight;
		mFormat = cSource.mFormat;
		if(mManagedBuffer)
		{
			if(mData)
				free(mData);
			mData = (void *)malloc(mMaxBufSize);
			assert(mData != NULL);
		}
		else
		{
			assert(cSource.mMaxBufSize <= mMaxBufSize);
		}
		memcpy(mData, cSource.mData, mMaxBufSize);
		return *this;
	}

	void setBuffer(int maxBufSize, void *manualBuffer)
	{
		mManagedBuffer = false;
		mData = manualBuffer;
		mMaxBufSize = maxBufSize;
	}
	void setDimensions(int width, int height)
	{
		int reqBytes = getBytesPerPixel() * mWidth * mHeight;
		if(mMaxBufSize)
			assert(reqBytes <= mMaxBufSize);
		mWidth = width;
		mHeight = height;
	}
	void setFormat(BitmapFormat format) { mFormat = format; }
	int getWidth() { return mWidth; }
	int getHeight() { return mHeight; }
	BitmapFormat getFormat() { return mFormat; }
	void *getBuffer()
	{
		assert(mData != NULL);
		return mData;
	}
	int getBytesPerPixel()
	{
		switch(mFormat)
		{
		case BITMAP_FORMAT_RGB565:
		case BITMAP_FORMAT_RGBA5551:
		case BITMAP_FORMAT_RGBA4444:
			return 2;
		case BITMAP_FORMAT_RGBA8888:
			return 4;
		}
		return 0;
	}

private:
	void *mData;
	int mMaxBufSize;
	int mWidth;
	int mHeight;
	BitmapFormat mFormat;
	bool mManagedBuffer;
};

#endif // _RETRON_COMMON_H
