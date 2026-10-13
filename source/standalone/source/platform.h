#ifndef FH2_PRESENCE_PLATFORM_H
#define FH2_PRESENCE_PLATFORM_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#define PATH_CAPACITY 32768
#define GAME_FILE_SIZE 58204160u
#define GAME_IMAGE_SIZE 0x3a18000u
#define GAME_TIMESTAMP 0x565501edu
#define ARRAY_COUNT(a) (sizeof(a)/sizeof((a)[0]))
#endif
