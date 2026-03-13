#ifndef PALETTES_H
#define PALETTES_H

#include <gb/cgb.h>
#include <stdint.h>

#define RGB_GOLD 0x331C
#define RGB_DARK_PURPLE 0x2405

// NOT USED, NORMAL TEXT, BG, NOT USED
const palette_color_t palettePurple[] = {RGB_WHITE, RGB_GOLD, RGB_DARK_PURPLE,
                                         RGB_BLACK};
const palette_color_t paletteInvertPurple[] = {RGB_WHITE, RGB_DARK_PURPLE,
                                               RGB_GOLD, RGB_BLACK};

#endif
