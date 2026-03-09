#ifndef TERM_H
#define TERM_H 1

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TERM_ROWS 18
#define TERM_COLS 20
#define TERM_MIN_X 1
#define TERM_MIN_Y 1
#define TERM_MAX_X 18
#define TERM_MAX_Y 15
#define TERM_CSI_ARG_LEN 6
#define TERM_CSI_ARG_BUFFER_LEN 12  // nnn;mmm;mmmC
#define TERM_CSI_RESPONSE_LEN 10

typedef struct term_char_attributes {
  uint8_t mode;  // TODO: Make this a bit field. Right now it's just inverse.
} term_char_attributes_t;

typedef struct {
  uint8_t enabled;
  uint8_t current_arg;
  uint8_t args[TERM_CSI_ARG_LEN];
  uint8_t arg_buffer_idx;
  unsigned char arg_buffer[TERM_CSI_ARG_BUFFER_LEN];
  unsigned char response[TERM_CSI_RESPONSE_LEN];
} term_csi_state_t;

typedef struct {
  uint8_t started;
  uint8_t x;
  uint8_t y;
  uint8_t esc;
  uint8_t csi;
  term_csi_state_t* csi_state;
  unsigned char screen[TERM_ROWS][TERM_COLS];
  term_char_attributes_t screen_attrs[TERM_ROWS][TERM_COLS];
} term_state_t;

extern uint8_t term_gfx_mode_inverse;

void term_init(term_state_t* term);
void term_clear_screen(term_state_t* term);
inline void term_cursor_up(term_state_t* term);
inline void term_cursor_down(term_state_t* term);
inline void term_cursor_forward(term_state_t* term);
inline void term_cursor_backward(term_state_t* term);
void term_handle_link_byte(term_state_t* term, unsigned char cur_char);

// TODO: Remove this dependency
extern void link_port_write(unsigned char data);

#endif
