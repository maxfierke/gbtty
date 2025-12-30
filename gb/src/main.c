#include <gb/cgb.h>
#include <gb/crash_handler.h>
#include <gb/gb.h>
#include <gbdk/console.h>
#include <gbdk/font.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

uint8_t term_x = 1;
uint8_t term_y = 2;
uint8_t term_started = 0;

char textBuffer[20];

// Drawing & Themes
font_t titleFont, consoleFont, minFontInvert;
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
  consoleFont = font_load(font_ibm);

  if (cgb) {
    font_color(2, 3);
  } else {
    font_color(0, 3);
  }
  titleFont = font_load(font_ibm);

  if (cgb) {
    font_color(1, 2);
  } else {
    font_color(3, 0);
  }
  minFontInvert = font_load(font_min);
}

void printAtWith(char str[], uint8_t x, uint8_t y, font_t font) {
  font_set(font);
  gotoxy(x, y);
  printf(str);
}

void printCharAt(unsigned ch, uint8_t x, uint8_t y, font_t font) {
  font_set(font);
  gotoxy(x, y);
  setchar(ch);
}

font_t pressedFont(uint8_t key) {
  if (KEY_PRESSED(key)) {
    return minFontInvert;
  } else {
    return consoleFont;
  }
}

void clearScreen(void) {
  cls();
  term_x = 1;
  term_y = 2;
}

void printModel(void) {
  if (_is_GBA) {
    printAtWith(" A", 17, 16, consoleFont);
  } else if (cgb) {
    printAtWith(" C", 17, 16, consoleFont);
  } else if (_cpu == MGB_TYPE) {
    printAtWith(" M", 17, 16, consoleFont);
  } else {
    printAtWith(" D", 17, 16, consoleFont);
  }
}

void setPalette(palette_color_t* pal) {
  if (cgb) set_bkg_palette(0, 1, pal);
}

void advanceTermLine(void) {
  term_x = 1;
  term_y++;
  if (term_y >= 16) {
    term_y = 2;
  }
}

void processIncomingBytes(void) {
  if (_io_status == IO_IDLE) {
    unsigned char curChar = (unsigned char)_io_in;

    if (curChar == '\0' || curChar == (unsigned char)0xFF) {
      // Skip NULL and dirty reads
      return;
    }
    if (curChar == (unsigned char)0x7F) {  // Handle backspace
      if (term_x > 1) {
        printCharAt(' ', term_x, term_y, consoleFont);  // Blank out cursor
        term_x--;
      }
      return;
    }

    // Skip unprintable characters
    if (curChar < ' ' || curChar > (unsigned char)0x7F) return;

    if (curChar == '\n') {
      advanceTermLine();
    }

    printCharAt(curChar, term_x, term_y, consoleFont);
    term_x++;
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
  printAtWith("GBTTY", 1, 1, titleFont);

  if (!term_started) {
    printAtWith("START to begin", 3, 12, minFontInvert);
  } else {
    if (term_x >= 19) {
      advanceTermLine();
    }
    printCharAt(' ', term_x, term_y, minFontInvert);
  }

  if (triggerMessageClear) {
    clearScreen();
    triggerMessageClear = 0;
  }

  drawLink();
}

void update(void) {
  if (!term_started) {
    if (KEY_RELEASED(J_START)) {
      term_started = 1;
      printAtWith("               ", 3, 12, titleFont);
    }

    return;
  }

  if (KEY_RELEASED(J_SELECT) && KEY_RELEASED(J_START)) {
    triggerMessageClear = 1;
  }

  processIncomingBytes();
  if (_io_status == IO_IDLE) {
    receive_byte();
  }
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

  DISPLAY_ON;

  while (1) {
    UPDATE_KEYS();

    update();
    draw();

    vsync();
  }
}
