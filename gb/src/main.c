#include <gb/cgb.h>
#include <gb/crash_handler.h>
#include <gb/gb.h>
#include <gbdk/console.h>
#include <gbdk/font.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "link.h"
#include "term.h"

// Keys
uint8_t previous_keys = 0;
uint8_t keys = 0;
#define UPDATE_KEYS()   \
  previous_keys = keys; \
  keys = joypad()
#define KEY_PRESSED(K) (keys & (K))
#define KEY_TICKED(K) ((keys & (K)) && !(previous_keys & (K)))
#define KEY_RELEASED(K) (!(keys & (K)) && (previous_keys & (K)))
#define NO_KEYS_PRESSED() keys == 0

// Globals
uint8_t theme_index = 0;
uint8_t trigger_clear_screen = 0;

char line_buffer[20];

// Drawing & Themes
font_t title_font, term_console_font, invert_min_font;
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

void setup_fonts(void) {
  font_init();
  font_color(0, 3);
  term_console_font = font_load(font_ibm);

  if (DEVICE_SUPPORTS_COLOR) {
    font_color(2, 3);
  } else {
    font_color(0, 3);
  }
  title_font = font_load(font_ibm);

  if (DEVICE_SUPPORTS_COLOR) {
    font_color(1, 2);
  } else {
    font_color(3, 0);
  }
  invert_min_font = font_load(font_min);
}

void print_str_at(char str[], uint8_t x, uint8_t y, font_t font) {
  font_set(font);
  gotoxy(x, y);
  printf(str);
}

void print_char_at(char ch, uint8_t x, uint8_t y, font_t font) {
  font_set(font);
  gotoxy(x, y);
  setchar(ch);
}

void draw_term_state(term_state_t* term) {
  if (!term->esc && !term->csi) {
    print_str_at("   ", 1, 16, term_console_font);
  } else if (term->esc && !term->csi) {
    print_str_at("ESC", 1, 16, invert_min_font);
  } else if (term->csi && !term->csi_state->args[0]) {
    print_str_at("CSI", 1, 16, invert_min_font);
  } else if (term->csi) {
    sprintf(line_buffer, "^[[%s", (unsigned char*)term->csi_state->arg_buffer);
    print_str_at(line_buffer, 1, 16, invert_min_font);
  }

  sprintf(
      line_buffer, "%hx %hx %hx %hx",
      (uint8_t)link_rx_buffer[(link_rx_buffer_head - 3) % LINK_RX_BUFFER_SIZE],
      (uint8_t)link_rx_buffer[(link_rx_buffer_head - 2) % LINK_RX_BUFFER_SIZE],
      (uint8_t)link_rx_buffer[(link_rx_buffer_head - 1) % LINK_RX_BUFFER_SIZE],
      (uint8_t)link_rx_buffer[link_rx_buffer_head]);
  print_str_at(line_buffer, 8, 16, invert_min_font);
}

// Main Loop
void draw(term_state_t* term) {
  if (!term->started) {
    print_str_at("GBTTY", 8, 1, title_font);
    print_str_at("START to begin", 3, 12, invert_min_font);
  } else {
    for (uint8_t row = TERM_MIN_Y; row <= TERM_MAX_Y; row++) {
      for (uint8_t col = TERM_MIN_X; col <= TERM_MAX_X; col++) {
        if ((row == term->y && col == term->x) ||
            term->screen_attrs[row][col].mode) {
          print_char_at(term->screen[row][col], col, row, invert_min_font);
        } else {
          print_char_at(term->screen[row][col], col, row, term_console_font);
        }
      }
    }
  }

  draw_term_state(term);
}

void update(term_state_t* term) {
  if (!term->started) {
    if (KEY_RELEASED(J_START)) {
      term->started = 1;
      link_port_init();
    }

    return;
  }

  if (KEY_PRESSED(J_SELECT) && KEY_PRESSED(J_START)) {
    term_clear_screen(term);
  }

  if (KEY_PRESSED(J_UP)) {
    term_cursor_up(term);
  } else if (KEY_PRESSED(J_DOWN)) {
    term_cursor_down(term);
  }

  if (KEY_PRESSED(J_LEFT)) {
    term_cursor_backward(term);
  } else if (KEY_PRESSED(J_RIGHT)) {
    term_cursor_forward(term);
  }

  if (term->started) {
    while (link_rx_buffer_head != link_rx_buffer_tail) {
      unsigned char next_char = link_rx_buffer[link_rx_buffer_tail];
      link_rx_buffer_tail = (link_rx_buffer_tail + 1) % LINK_RX_BUFFER_SIZE;
      term_handle_link_byte(term, next_char);
    }
  }
}

void main(void) {
  DISPLAY_OFF;

  ENABLE_RAM;
  SHOW_BKG;
  SHOW_SPRITES;
  SPRITES_8x8;

  if (DEVICE_SUPPORTS_COLOR) {
    set_bkg_palette(0, 1, palettes[theme_index]);
  }

  setup_fonts();

  term_state_t term = {0};
  term_init(&term);

  DISPLAY_ON;

  while (1) {
    UPDATE_KEYS();

    update(&term);
    draw(&term);

    vsync();
  }
}
