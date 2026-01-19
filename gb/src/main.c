#include <gb/cgb.h>
#include <gb/crash_handler.h>
#include <gb/gb.h>
#include <gbdk/console.h>
#include <gbdk/font.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#ifndef TERM_ACK_GBDK_COMPAT
#define TERM_ACK_GBDK_COMPAT 0
#endif

#if TERM_ACK_GBDK_COMPAT
// Compatibility with GBDK send_byte/receive_byte
// Used for testing
#define TERM_ACK 0x55
#define TERM_SYN_IDLE 0x55
#else
#define TERM_ACK 0x6
#define TERM_SYN_IDLE 0x16
#endif
#define TERM_ROWS 18
#define TERM_COLS 20
#define TERM_MIN_X 1
#define TERM_MIN_Y 2
#define TERM_MAX_X 19
#define TERM_MAX_Y 16
#define TERM_CSI_ARG_LEN 6
#define TERM_CSI_ARG_BUFFER_LEN 12  // nnn;mmm;mmmC
uint8_t term_started = 0;
uint8_t term_x = TERM_MIN_X;
uint8_t term_y = TERM_MIN_Y;
uint8_t term_esc = 0;
uint8_t term_csi = 0;
uint8_t term_csi_args[TERM_CSI_ARG_LEN];
uint8_t term_csi_arg = 0;
uint8_t term_csi_arg_buffer_idx = 0;
unsigned char term_csi_arg_buffer[TERM_CSI_ARG_BUFFER_LEN];

unsigned char term_screen[TERM_ROWS][TERM_COLS];

char line_buffer[20];

#define LINK_BUFFER_SIZE 32
unsigned char link_buffer[LINK_BUFFER_SIZE];
volatile uint8_t link_buffer_head = 0;
volatile uint8_t link_buffer_tail = 0;

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

void link_port_interrupt(void) {
  uint8_t data = SB_REG;

  uint8_t next_head = (link_buffer_head + 1) % LINK_BUFFER_SIZE;
  if (next_head != link_buffer_tail) {
    link_buffer[link_buffer_head] = data;
    link_buffer_head = next_head;
  }

  SB_REG = TERM_ACK;
  SC_REG = SIOF_CLOCK_EXT | SIOF_XFER_START;
}

void init_link_port(void) {
  CRITICAL { add_SIO(link_port_interrupt); }

  SB_REG = TERM_SYN_IDLE;

  // Set to external clock, we're not drivin' this thing
  SC_REG = SIOF_CLOCK_EXT | SIOF_XFER_START;

  set_interrupts(SIO_IFLAG | VBL_IFLAG);
}

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

void term_clear_screen(void) {
  term_x = TERM_MIN_X;
  term_y = TERM_MIN_Y;

  for (uint8_t row = 0; row < TERM_ROWS; row++) {
    for (uint8_t col = 0; col < TERM_COLS; col++) {
      term_screen[row][col] = '\0';
    }
  }
}

void term_advance_line(void) {
  term_x = TERM_MIN_X;
  term_y++;
  if (term_y >= TERM_MAX_Y) {
    term_y = TERM_MIN_Y;
  }
}

void term_consume_csi_arg_buffer(void) {
  unsigned char arg_buf[4] = {0x0, 0x0, 0x0, 0x0};  // 3+1 NULL
  uint8_t arg_buf_idx = 0;
  for (uint8_t i = 0; i < TERM_CSI_ARG_LEN; i++) {
    unsigned char arg_char = term_csi_arg_buffer[i];

    if (arg_char == '\0') {
      // End-of-buffer
      break;
    } else if (arg_char == ';' || arg_buf_idx == 3) {
      term_csi_args[term_csi_arg] = (uint8_t)atoi(arg_buf);
      term_csi_arg++;
      arg_buf[0] = 0x0;
      arg_buf[1] = 0x0;
      arg_buf[2] = 0x0;
    } else if (arg_char >= 0x30 && arg_char <= 0x39) {
      arg_buf[arg_buf_idx] = arg_char;
      arg_buf_idx++;
    }
  }
}

void term_reset_csi(void) {
  for (uint8_t i = 0; i < TERM_CSI_ARG_LEN; i++) {
    term_csi_args[i] = 0x0;
  }
  for (uint8_t i = 0; i < TERM_CSI_ARG_BUFFER_LEN; i++) {
    term_csi_arg_buffer[i] = 0x0;
  }
  term_csi = 0;
}

void term_handle_csi_char(unsigned char cur_char) {
  switch (cur_char) {
    case 'A':  // Cursor up (n=1)
      term_consume_csi_arg_buffer();
      if (!term_csi_args[0]) {
        term_csi_args[0] = 1;
      }
      for (uint8_t i = 0; i < term_csi_args[0]; i++) {
        if (term_y > TERM_MIN_Y) {
          term_y--;
        }
      }
      term_reset_csi();
      break;
    case 'B':  // Cursor down (n=1)
      term_consume_csi_arg_buffer();
      if (!term_csi_args[0]) {
        term_csi_args[0] = 1;
      }
      for (uint8_t i = 0; i < term_csi_args[0]; i++) {
        if (term_y < TERM_MAX_Y) {
          term_y++;
        }
      }
      term_reset_csi();
      break;
    case 'C':  // Cursor forward (n=1)
      term_consume_csi_arg_buffer();
      if (!term_csi_args[0]) {
        term_csi_args[0] = 1;
      }
      for (uint8_t i = 0; i < term_csi_args[0]; i++) {
        if (term_x < TERM_MAX_X) {
          term_x++;
        }
      }
      term_reset_csi();
      break;
    case 'D':  // Cursor back (n=1)
      term_consume_csi_arg_buffer();
      if (!term_csi_args[0]) {
        term_csi_args[0] = 1;
      }
      for (uint8_t i = 0; i < term_csi_args[0]; i++) {
        if (term_x > TERM_MIN_X + 1) {
          term_x--;
        }
      }
      term_reset_csi();
      break;
    case 'E':  // Cursor next line (n=1)
      term_consume_csi_arg_buffer();
      if (!term_csi_args[0]) {
        term_csi_args[0] = 1;
      }
      for (uint8_t i = 0; i < term_csi_args[0]; i++) {
        if (term_y < TERM_MAX_Y + 1) {
          term_y++;
          term_x = TERM_MIN_X;
        }
      }
      break;
    case 'F':  // Cursor prev line (n=1)
      term_consume_csi_arg_buffer();
      if (!term_csi_args[0]) {
        term_csi_args[0] = 1;
      }
      for (uint8_t i = 0; i < term_csi_args[0]; i++) {
        if (term_y > TERM_MIN_Y + 1) {
          term_y--;
          term_x = TERM_MIN_X;
        }
      }
      term_reset_csi();
      break;
    case 'G':  // Cursor horzontal absolute (n=1)
      term_consume_csi_arg_buffer();
      if (!term_csi_args[0]) {
        term_csi_args[0] = 1;
      }
      if (term_csi_args[0] > TERM_MAX_X) {
        term_csi_args[0] = TERM_MAX_X;
      }

      term_x = term_csi_args[0];

      term_reset_csi();
      break;
    case 'H':  // Cursor position (n=1, m=1)
      term_consume_csi_arg_buffer();
      if (!term_csi_args[0]) {
        term_csi_args[0] = 1;
      }
      if (term_csi_args[0] > TERM_MAX_X) {
        term_csi_args[0] = TERM_MAX_X;
      }
      if (!term_csi_args[1]) {
        term_csi_args[1] = 1;
      }

      if (term_csi_args[1] > TERM_MAX_Y) {
        term_csi_args[1] = TERM_MAX_Y;
      }

      term_x = term_csi_args[0];
      term_y = term_csi_args[1];

      term_reset_csi();
      break;
    case 'J':  // Erase in display (n=0)
      term_consume_csi_arg_buffer();
      if (term_csi_args[0] == 0) {  // Clear to end of screen
        for (uint8_t j = term_y; j < TERM_ROWS; j++) {
          for (uint8_t i = 0; i < TERM_COLS; i++) {
            if (j == term_y && i < term_x) continue;
            term_screen[j][i] = ' ';
          }
        }
      } else if (term_csi_args[0] == 1) {
        // Clear from cursor to beginning of screen
        for (uint8_t j = 0; j <= term_y; j++) {
          for (uint8_t i = 0; i < TERM_COLS; i++) {
            if (j == term_y && i > term_x) continue;
            term_screen[j][i] = ' ';
          }
        }
      } else if (term_csi_args[0] >= 2) {
        term_clear_screen();
      }

      term_reset_csi();
      break;
    case 'K':  // Erase in line (n=0)
      term_consume_csi_arg_buffer();
      if (term_csi_args[0] == 0) {  // Clear to end of screen
        for (uint8_t j = term_y; j < TERM_ROWS; j++) {
          for (uint8_t i = 0; i < TERM_COLS; i++) {
            if (j == term_y && i < term_x) continue;
            term_screen[j][i] = ' ';
          }
        }
      } else if (term_csi_args[0] == 1) {
        // Clear from cursor to beginning of screen
        for (uint8_t j = 0; j <= term_y; j++) {
          for (uint8_t i = 0; i < TERM_COLS; i++) {
            if (j == term_y && i > term_x) continue;
            term_screen[j][i] = ' ';
          }
        }
      } else if (term_csi_args[0] >= 2) {
        term_clear_screen();
      }

      term_reset_csi();
      break;
    default:
      if (term_csi_arg == TERM_CSI_ARG_LEN ||
          term_csi_arg_buffer_idx == TERM_CSI_ARG_BUFFER_LEN) {
        // Ignore unknown CSI commands/arguments (TODO: too greedy?)
        term_reset_csi();
        break;
      }

      term_csi_arg_buffer[term_csi_arg_buffer_idx] = cur_char;
      term_csi_arg_buffer_idx++;
      break;
  }
}

void term_handle_link_byte(unsigned char cur_char) {
  switch (cur_char) {
    case '\0':
    case 0xFF:
      // Skip NULL and dirty reads
      return;
    case 0x1B:  // ESC
      if (!term_esc) {
        // Enable ESC
        term_esc = 1;
      }
      return;
    case 0x5B:  // CSI (7-bit)
    case 0x9B:  // CSI (8-bit)
      if (term_esc && !term_csi) {
        // Enable CSI
        term_esc = 0;
        term_csi = 1;
        return;
      }
    case 0x08:  // BS
    case 0x7F:  // DEL

      if (term_x > 1) {  // Current line
        // Blank out cursor
        term_screen[term_y][term_x] = ' ';
        term_x--;
      } else {  // Reverse wrap
        // Blank out cursor
        term_screen[term_y][term_x] = ' ';
        term_x = TERM_MAX_X - 1;
        term_y--;
      }
      return;
    case '\a':
      // TODO: Ding a bell here.
      return;
    case '\n':
    case 0x0D:
      term_advance_line();
      return;
    case '\t':
      // Tabs = spaces here. We're working with 20x18!
      cur_char = ' ';
      break;
    default:
      if (term_csi) {
        term_handle_csi_char(cur_char);
        return;
      }

      // Skip unprintable characters
      if (cur_char < ' ' || cur_char > (unsigned char)0x7F) {
        return;
      }

      break;
  }

  term_screen[term_y][term_x] = cur_char;
  term_x++;

  if (term_x >= TERM_MAX_X) {
    term_advance_line();
  }
}

void draw_term_state(void) {
  if (!term_esc && !term_csi) {
    print_str_at("   ", 1, 16, term_console_font);
  } else if (term_esc && !term_csi) {
    print_str_at("ESC", 1, 16, invert_min_font);
  } else if (term_csi && !term_csi_args[0]) {
    print_str_at("CSI", 1, 16, invert_min_font);
  } else if (term_csi) {
    sprintf(line_buffer, "^[[%s", (unsigned char*)term_csi_arg_buffer);
    print_str_at(line_buffer, 1, 16, invert_min_font);
  }

  sprintf(line_buffer, "%hx %hx %hx %hx",
          (uint8_t)link_buffer[(link_buffer_head - 3) % LINK_BUFFER_SIZE],
          (uint8_t)link_buffer[(link_buffer_head - 2) % LINK_BUFFER_SIZE],
          (uint8_t)link_buffer[(link_buffer_head - 1) % LINK_BUFFER_SIZE],
          (uint8_t)link_buffer[link_buffer_head]);
  print_str_at(line_buffer, 8, 16, invert_min_font);
}

// Main Loop
void draw(void) {
  if (!term_started) {
    print_str_at("GBTTY", 8, 1, title_font);
    print_str_at("START to begin", 3, 12, invert_min_font);
  } else {
    for (uint8_t row = TERM_MIN_Y; row < TERM_MAX_Y; row++) {
      for (uint8_t col = TERM_MIN_X; col < TERM_MAX_X; col++) {
        if (row == term_y && col == term_x) {
          print_char_at(term_screen[row][col], col, row, invert_min_font);
        } else {
          print_char_at(term_screen[row][col], col, row, term_console_font);
        }
      }
    }
  }

  draw_term_state();
}

void update(void) {
  if (!term_started) {
    if (KEY_RELEASED(J_START)) {
      term_started = 1;
      init_link_port();
    }

    return;
  }

  if (KEY_PRESSED(J_SELECT) && KEY_PRESSED(J_START)) {
    term_clear_screen();
  }

  if (term_started) {
    while (link_buffer_head != link_buffer_tail) {
      unsigned char next_char = link_buffer[link_buffer_tail];
      link_buffer_tail = (link_buffer_tail + 1) % LINK_BUFFER_SIZE;
      term_handle_link_byte(next_char);
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

  DISPLAY_ON;

  while (1) {
    UPDATE_KEYS();

    update();
    draw();

    vsync();
  }
}
