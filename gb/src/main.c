#include <gb/cgb.h>
#include <gb/crash_handler.h>
#include <gb/gb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "link.h"
#include "palettes.h"
#include "portfolio.h"
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

#define SPRITE_DMG_CURSOR 0

#define PORTFOLIO_FIRST_TILE 0

// Globals
uint8_t cursor_tile[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
char line_buffer[20];

void setup_fonts(void) {
  set_1bpp_colors(1, 2);
  set_bkg_1bpp_data(PORTFOLIO_FIRST_TILE, portfolio_TILE_COUNT,
                    portfolio_tiles);
}

void print_char_at(char ch, uint8_t x, uint8_t y, term_sgr_mode_t mode) {
  set_bkg_tile_xy(x, y, PORTFOLIO_FIRST_TILE + (uint8_t)ch);

  if (DEVICE_SUPPORTS_COLOR) {
    if (mode == TERM_SGR_INVERSE) {
      set_bkg_attribute_xy(x, y, 1);
    } else {
      set_bkg_attribute_xy(x, y, 0);
    }
  } else {
    // TODO: Determine how to support invert on DMG/GBP
  }
}

void print_str_at(char str[], uint8_t x, uint8_t y, term_sgr_mode_t mode) {
  uint8_t i = 0;

  while (str[i] != '\0') {
    if (x + i > TERM_MAX_X) break;
    print_char_at(str[i], x + i, y, mode);
    i++;
  }
}

void draw_term_state(term_state_t* term) {
  if (!term->esc && !term->csi) {
    print_str_at("   ", 1, 16, TERM_SGR_DEFAULT);
  } else if (term->esc && !term->csi) {
    print_str_at("ESC", 1, 16, TERM_SGR_INVERSE);
  } else if (term->csi && !term->csi_state->args[0]) {
    print_str_at("CSI", 1, 16, TERM_SGR_INVERSE);
  } else if (term->csi) {
    sprintf(line_buffer, "^[[%s", (unsigned char*)term->csi_state->arg_buffer);
    print_str_at(line_buffer, 1, 16, TERM_SGR_INVERSE);
  }

  uint8_t buffer_head = link_rx_buffer_head;

  sprintf(
      line_buffer, "%hx %hx %hx %hx",
      (uint8_t)link_rx_buffer[(uint8_t)(buffer_head - 4) % LINK_RX_BUFFER_SIZE],
      (uint8_t)link_rx_buffer[(uint8_t)(buffer_head - 3) % LINK_RX_BUFFER_SIZE],
      (uint8_t)link_rx_buffer[(uint8_t)(buffer_head - 2) % LINK_RX_BUFFER_SIZE],
      (uint8_t)
          link_rx_buffer[(uint8_t)(buffer_head - 1) % LINK_RX_BUFFER_SIZE]);
  print_str_at(line_buffer, 8, 16, TERM_SGR_INVERSE);
}

// Main Loop
void draw(term_state_t* term) {
  if (!term->started) {
    print_str_at("GBTTY", 8, 1, TERM_SGR_DEFAULT);
    print_str_at("START to begin", 3, 12, TERM_SGR_INVERSE);
  } else {
    for (uint8_t row = TERM_MIN_Y; row <= TERM_MAX_Y; row++) {
      for (uint8_t col = TERM_MIN_X; col <= TERM_MAX_X; col++) {
        if ((row == term->y && col == term->x) || term->cells[row][col].mode) {
          print_char_at(term->cells[row][col].ch, col, row, TERM_SGR_INVERSE);
        } else {
          print_char_at(term->cells[row][col].ch, col, row, TERM_SGR_DEFAULT);
        }
      }
    }

    if (!DEVICE_SUPPORTS_COLOR) {
      set_sprite_1bpp_data(0, 1, cursor_tile);
      move_sprite(SPRITE_DMG_CURSOR, (term->x * 8) + 8, (term->y * 8) + 16);
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
    set_bkg_palette(0, 1, palettePurple);
    set_bkg_palette(1, 1, paletteInvertPurple);
  } else {
    BGP_REG = DMG_PALETTE(DMG_BLACK, DMG_WHITE, DMG_BLACK, DMG_DARK_GRAY);
    OBP0_REG = DMG_PALETTE(DMG_WHITE, DMG_BLACK, DMG_WHITE, DMG_DARK_GRAY);
    hide_sprite(SPRITE_DMG_CURSOR);
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
