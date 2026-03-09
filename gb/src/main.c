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
#include "palettes.h"
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
