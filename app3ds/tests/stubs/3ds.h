/*
 * FAUX en-tête libctru, pour vérifier la syntaxe du code 3DS sur le Mac
 * (make test). Il ne déclare que ce que l'appli utilise, avec les mêmes
 * signatures que libctru. La vraie compilation se fait avec devkitPro.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef int32_t s32;
typedef enum { GFX_TOP = 0, GFX_BOTTOM = 1 } gfxScreen_t;
typedef enum { GFX_LEFT = 0, GFX_RIGHT = 1 } gfx3dSide_t;
typedef struct { u16 px, py; } touchPosition;
typedef struct { u8 fontType, height, width, maxWidth; u8 lineFeed; } FINF_s; /* extrait */
enum {
    KEY_A = 1u << 0, KEY_B = 1u << 1, KEY_SELECT = 1u << 2, KEY_START = 1u << 3,
    KEY_DRIGHT = 1u << 4, KEY_DLEFT = 1u << 5, KEY_DUP = 1u << 6, KEY_DDOWN = 1u << 7,
    KEY_R = 1u << 8, KEY_L = 1u << 9, KEY_X = 1u << 10, KEY_Y = 1u << 11,
    KEY_TOUCH = 1u << 20, KEY_CPAD_UP = 1u << 30, KEY_CPAD_DOWN = 1u << 31,
    KEY_UP = KEY_DUP | KEY_CPAD_UP, KEY_DOWN = KEY_DDOWN | KEY_CPAD_DOWN,
};
void gfxInitDefault(void); void gfxExit(void); bool aptMainLoop(void);
typedef uint64_t u64;
u64 osGetTime(void);
void hidScanInput(void); u32 hidKeysDown(void); u32 hidKeysHeld(void); void hidTouchRead(touchPosition *pos);
