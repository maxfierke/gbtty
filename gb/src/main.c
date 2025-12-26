#include <gb/cgb.h>
#include <gb/crash_handler.h>
#include <gb/gb.h>
#include <gbdk/console.h>
#include <gbdk/font.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "border.h"
#include "gt_invert.h"

#define BORDER_OFFSET 0xB0
#define GT_INVERT_OFFSET 0xBD

// Keys
uint8_t previousKeys = 0;
uint8_t keys = 0;
#define UPDATE_KEYS()  \
  previousKeys = keys; \
  keys = joypad()
#define KEY_PRESSED(K) (keys & (K))
#define KEY_TICKED(K) ((keys & (K)) && !(previousKeys & (K)))
#define KEY_RELEASED(K) (!(keys & (K)) && (previousKeys & (K)))
#define NO_KEYS_PRESSED() keys == 0

// Globals
uint8_t cgb = 0;
uint8_t themeIndex = 0;
uint8_t triggerMessageClear = 0;

char textBuffer[20];

// Drawing & Themes
font_t ibmFont, minFont, minFontInvert;
#define RGB_GOLD 0x331C
#define RGB_DARK_PURPLE 0x2405
#define RGB_DARK_GREEN 0x0923
#define RGB_DARK_BLUE 0x3860
#define RGB_DARK_ORANGE 0x012E
#define RGB_DARK_RED 0x080E
#define RGB_CERULEAN 0x5CC2
#define RGB_ICE_BLUE 0x5E89
#define RGB_GREY 0x3DEF
#define RGB_MUTED_PINK 0x5573
#define RGB_VIOLET 0x584C
#define RGB_POCKET_WHITE 0x5BDB
#define RGB_POCKET_LIGHT_GREY 0x4B15
#define RGB_POCKET_DARK_GREY 0x3E4F
#define RGB_POCKET_BLACK 0x2D89
#define RGB_EXTREME_RED 0x0CB7
#define RGB_LEAF_GREEN 0x2240
#define RGB_DUSK_GREEN 0x2392
#define RGB_DUSK_TEAL 0x4608
#define RGB_DUSK_PURPLE 0x28A6
#define RGB_SUNRISE_PURPLE 0x2CEF
#define RGB_SUNRISE_ORANGE 0x3ABF
#define RGB_SUNRISE_RED 0x35BD
#define RGB_SUNRISE_YELLOW 0x6BDF

// NORMAL TEXT, HIGHLIGHT TEXT, HIGHLIGHT/TITLE, BG
const palette_color_t palettePurple[] = {RGB_WHITE, RGB_BLACK, RGB_GOLD,
                                         RGB_DARK_PURPLE};
const palette_color_t paletteBlue[] = {RGB_WHITE, RGB_BLACK, RGB_GOLD,
                                       RGB_DARK_BLUE};
const palette_color_t paletteGreen[] = {RGB_WHITE, RGB_BLACK, RGB_GOLD,
                                        RGB_DARK_GREEN};
const palette_color_t paletteOrange[] = {RGB_WHITE, RGB_BLACK, RGB_GOLD,
                                         RGB_DARK_ORANGE};
const palette_color_t paletteRed[] = {RGB_WHITE, RGB_BLACK, RGB_GOLD,
                                      RGB_DARK_RED};
const palette_color_t paletteBlack[] = {RGB_WHITE, RGB_BLACK, RGB_GOLD,
                                        RGB_BLACK};
const palette_color_t paletteSuperBlue[] = {RGB_WHITE, RGB_WHITE, RGB_ICE_BLUE,
                                            RGB_CERULEAN};
const palette_color_t paletteVeryGreen[] = {RGB_WHITE, RGB_GREEN,
                                            RGB_DARK_GREEN, RGB_LEAF_GREEN};
const palette_color_t paletteReallyPink[] = {RGB_WHITE, RGB_BLACK, RGB_VIOLET,
                                             RGB_MUTED_PINK};
const palette_color_t paletteExtremeRed[] = {RGB_WHITE, RGB_RED, RGB_BLACK,
                                             RGB_EXTREME_RED};
const palette_color_t paletteTerminal[] = {RGB_GREEN, RGB_WHITE, RGB_GREY,
                                           RGB_BLACK};
const palette_color_t paletteDotMatrix[] = {
    RGB_POCKET_WHITE, RGB_POCKET_LIGHT_GREY, RGB_POCKET_DARK_GREY,
    RGB_POCKET_BLACK};
const palette_color_t paletteDusk[] = {RGB_WHITE, RGB_DUSK_GREEN, RGB_DUSK_TEAL,
                                       RGB_DUSK_PURPLE};
const palette_color_t paletteSunrise[] = {RGB_SUNRISE_PURPLE,
                                          RGB_SUNRISE_ORANGE, RGB_SUNRISE_RED,
                                          RGB_SUNRISE_YELLOW};
const palette_color_t paletteWhiteHC[] = {RGB_BLACK, RGB_WHITE, RGB_BLACK,
                                          RGB_WHITE};
const palette_color_t paletteBlackHC[] = {RGB_WHITE, RGB_BLACK, RGB_WHITE,
                                          RGB_BLACK};

const char* themeNames[] = {
    "     Purple", "       Blue", "      Green", "     Orange",
    "        Red", "      Black", " Super Blue", " Very Green",
    "Really Pink", "Extreme Red", "   Terminal", " Dot Matrix",
    "       Dusk", "    Sunrise", "   White HC", "   Black HC"};
palette_color_t* palettes[] = {
    palettePurple,     paletteBlue,       paletteGreen,     paletteOrange,
    paletteRed,        paletteBlack,      paletteSuperBlue, paletteVeryGreen,
    paletteReallyPink, paletteExtremeRed, paletteTerminal,  paletteDotMatrix,
    paletteDusk,       paletteSunrise,    paletteWhiteHC,   paletteBlackHC};
const uint8_t themeCount = sizeof palettes / sizeof palettes[0];

palette_color_t* bgColorTestPalettes[24];
palette_color_t* spriteColorTestPalettes[16];
const uint8_t paletteSteps[] = {1, 4, 7, 12, 16, 20, 27, 31};

void setupFonts(void) {
  font_init();
  font_color(0, 3);
  minFont = font_load(font_min);

  if (cgb) {
    font_color(2, 3);
  } else {
    font_color(0, 3);
  }
  ibmFont = font_load(font_ibm);

  if (cgb) {
    font_color(1, 2);
  } else {
    font_color(3, 0);
  }
  minFontInvert = font_load(font_min);

  set_bkg_data(GT_INVERT_OFFSET, gt_invert_TILE_COUNT, gt_invert_tiles);
}

void drawBorder(void) {
  set_bkg_data(BORDER_OFFSET, border_TILE_COUNT, border_tiles);
  set_bkg_based_tiles(0, 0, 20u, 18u, border_map, BORDER_OFFSET);
}

void printAtWith(char str[], uint8_t x, uint8_t y, font_t font) {
  font_set(font);
  gotoxy(x, y);
  printf(str);
}

font_t pressedFont(uint8_t key) {
  if (KEY_PRESSED(key)) {
    return minFontInvert;
  } else {
    return minFont;
  }
}

void clearMessageArea(void) {
  printAtWith("                  ", 1, 13, minFont);
  printAtWith("                  ", 1, 14, minFont);
}

void printModel(void) {
  if (_is_GBA) {
    printAtWith(" A", 17, 16, minFont);
  } else if (cgb) {
    printAtWith(" C", 17, 16, minFont);
  } else if (_cpu == MGB_TYPE) {
    printAtWith(" M", 17, 16, minFont);
  } else {
    printAtWith(" D", 17, 16, minFont);
  }
}

void setPalette(palette_color_t* pal) {
  if (cgb) set_bkg_palette(0, 1, pal);
}

void whiteScreen(void) {
  font_set(ibmFont);
  setPalette(paletteWhiteHC);
  for (uint8_t i = 0; i < 18; i++) {
    gotoxy(0, i);
    printf("                    ");
  }
}

void testLinkClient(void) {
  if (SB_REG != 0xFF) {  // we've received data, send it right back
    SC_REG = 0x80;       // transfer on, internal clock
  }
}

void drawLink(void) {
  if (SB_REG == 0xFF || !SB_REG) {
    printModel();
  } else {
    sprintf(textBuffer, "%hx", (uint8_t)SB_REG);
    printAtWith(textBuffer, 17, 16, minFontInvert);
  }
}

// Main Loop
void draw(void) {
  printAtWith("GBTTY", 1, 1, ibmFont);

  if (triggerMessageClear) {
    clearMessageArea();
    triggerMessageClear = 0;
  } else {
    printAtWith("Waiting for data...", 1, 13, ibmFont);
  }

  drawLink();
}

void update(void) {
  if (KEY_PRESSED(J_SELECT) && KEY_PRESSED(J_START)) {
    triggerMessageClear = 1;
  }

  testLinkClient();
}

void main(void) {
  DISPLAY_OFF;

  ENABLE_RAM;
  SHOW_BKG;
  SHOW_SPRITES;
  SPRITES_8x8;

  if (_cpu == CGB_TYPE) {
    cgb = 1;
  }

  setPalette(palettes[themeIndex]);

  setupFonts();
  drawBorder();

  SB_REG = 0xFF;
  SC_REG = 0x80;

  DISPLAY_ON;

  while (1) {
    UPDATE_KEYS();

    update();
    draw();

    wait_vbl_done();
  }
}
