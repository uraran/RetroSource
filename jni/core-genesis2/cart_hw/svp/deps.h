typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

// memory map setup
#define cpu68k_map_set(map, start, end, ptr, is_handler) \
	(void)ptr

// debug print
#define elprintf(source, ...)
