#ifndef PALETTES_H
#define PALETTES_H

#include <gb/cgb.h>
#include <stdint.h>

#define RGB_GOLD 0x331C
#define RGB_DARK_PURPLE 0x2405

// NORMAL TEXT, BG, NOT USED, NOT USED
const palette_color_t palettePurple[] = {RGB_GOLD, RGB_DARK_PURPLE};
const palette_color_t paletteInvertPurple[] = {RGB_DARK_PURPLE, RGB_GOLD,
                                               RGB_WHITE, RGB_BLACK};

#endif
