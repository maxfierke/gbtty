#include "term.h"

uint8_t term_gfx_mode_inverse = 0;

void term_init(term_t* term) {
  term->x = TERM_MIN_X;
  term->y = TERM_MIN_Y;

  // SAFETY: This will last the whole lifetime of the program, does not need
  // to be free()'d
  term->csi_state = malloc(sizeof(term_csi_state_t));
}

inline void term_clear_cell(term_t* term, uint8_t x, uint8_t y) {
  term->cells[y][x].ch = '\0';
  term->cells[y][x].mode = TERM_SGR_DEFAULT;
}

void term_clear_screen(term_t* term) {
  term->x = TERM_MIN_X;
  term->y = TERM_MIN_Y;

  memset(term->cells, 0, sizeof(term_cell_t) * TERM_ROWS * TERM_COLS);
}

static void term_advance_line(term_t* term) {
  term->x = TERM_MIN_X;
  term->y++;
  if (term->y >= TERM_MAX_Y) {
    term->y = TERM_MIN_Y;
  }
}

inline void term_cursor_up(term_t* term) {
  if (term->y > TERM_MIN_Y) {
    term->y--;
  }
}

inline void term_cursor_down(term_t* term) {
  if (term->y < TERM_MAX_Y) {
    term->y++;
  }
}

inline void term_cursor_forward(term_t* term) {
  if (term->x < TERM_MAX_X) {
    term->x++;
  }
}

inline void term_cursor_backward(term_t* term) {
  if (term->x > TERM_MIN_X) {
    term->x--;
  }
}

static void term_consume_csi_arg_buffer(term_t* term) {
  unsigned char arg_buf[4] = {0x0, 0x0, 0x0, 0x0};  // 3+1 NULL
  uint8_t arg_buf_idx = 0;
  for (uint8_t i = 0; i < TERM_CSI_ARG_BUFFER_LEN; i++) {
    unsigned char arg_char = term->csi_state->arg_buffer[i];

    if (arg_char == '\0') {
      // End-of-buffer
      break;
    } else if (arg_char >= 0x30 && arg_char <= 0x39) {
      arg_buf[arg_buf_idx] = arg_char;
      arg_buf_idx++;
    }

    if (arg_char == ';' || arg_buf_idx == 3) {
      term->csi_state->args[term->csi_state->current_arg] =
          (uint8_t)atoi(arg_buf);
      term->csi_state->current_arg++;
      arg_buf[0] = 0x0;
      arg_buf[1] = 0x0;
      arg_buf[2] = 0x0;
      arg_buf_idx = 0;
    }
  }

  if (arg_buf_idx > 0) {
    term->csi_state->args[term->csi_state->current_arg] =
        (uint8_t)atoi(arg_buf);
    term->csi_state->current_arg++;
  }
}

static void term_reset_csi(term_t* term) {
  memset(term->csi_state, 0, sizeof(term_csi_state_t));
  term->csi = 0;
}

static void term_queue_csi_response(term_t* term) {
  for (uint8_t i = 0; i < TERM_CSI_RESPONSE_LEN; i++) {
    if (!term->csi_state->response[i]) break;
    link_port_write(term->csi_state->response[i]);
  }
}

static void term_handle_csi_char(term_t* term, unsigned char cur_char) {
  switch (cur_char) {
    case 'c':
      term_consume_csi_arg_buffer(term);

      sprintf(term->csi_state->response, "\x1B[?6c");  // Identify as VT102

      term_queue_csi_response(term);

      term_reset_csi(term);
      break;
    case 'm':  // Graphics mode
      term_consume_csi_arg_buffer(term);

      switch (term->csi_state->args[0]) {
        case 0:
          term_gfx_mode_inverse = 0;
          break;
        case 7:
          term_gfx_mode_inverse = 1;
          break;
        case 27:
          term_gfx_mode_inverse = 0;
          break;
      }

      term_reset_csi(term);
      break;
    case 'n':  // Reporting
      term_consume_csi_arg_buffer(term);

      switch (term->csi_state->args[0]) {
        case 5:  // Status Report
          sprintf(term->csi_state->response, "\x1B[0n");
          break;
        case 6:  // Cursor report
          sprintf(term->csi_state->response, "\x1B[%d;%dR", term->y, term->x);
          break;
        default:
          // Unknown/unsupported
          term_reset_csi(term);
          return;
      }

      term_queue_csi_response(term);

      term_reset_csi(term);
      break;
    case 'A':  // Cursor up (n=1)
      term_consume_csi_arg_buffer(term);

      if (!term->csi_state->args[0]) {
        term->csi_state->args[0] = 1;
      }
      for (uint8_t i = 0; i < term->csi_state->args[0]; i++) {
        term_cursor_up(term);
      }

      term_reset_csi(term);
      break;
    case 'B':  // Cursor down (n=1)
      term_consume_csi_arg_buffer(term);

      if (!term->csi_state->args[0]) {
        term->csi_state->args[0] = 1;
      }
      for (uint8_t i = 0; i < term->csi_state->args[0]; i++) {
        term_cursor_down(term);
      }

      term_reset_csi(term);
      break;
    case 'C':  // Cursor forward (n=1)
      term_consume_csi_arg_buffer(term);

      if (!term->csi_state->args[0]) {
        term->csi_state->args[0] = 1;
      }
      for (uint8_t i = 0; i < term->csi_state->args[0]; i++) {
        term_cursor_forward(term);
      }

      term_reset_csi(term);
      break;
    case 'D':  // Cursor back (n=1)
      term_consume_csi_arg_buffer(term);

      if (!term->csi_state->args[0]) {
        term->csi_state->args[0] = 1;
      }
      for (uint8_t i = 0; i < term->csi_state->args[0]; i++) {
        term_cursor_backward(term);
      }

      term_reset_csi(term);
      break;
    case 'E':  // Cursor next line (n=1)
      term_consume_csi_arg_buffer(term);

      if (!term->csi_state->args[0]) {
        term->csi_state->args[0] = 1;
      }
      for (uint8_t i = 0; i < term->csi_state->args[0]; i++) {
        if (term->y < TERM_MAX_Y) {
          term->y++;
          term->x = TERM_MIN_X;
        }
      }

      term_reset_csi(term);
      break;
    case 'F':  // Cursor prev line (n=1)
      term_consume_csi_arg_buffer(term);

      if (!term->csi_state->args[0]) {
        term->csi_state->args[0] = 1;
      }
      for (uint8_t i = 0; i < term->csi_state->args[0]; i++) {
        if (term->y > TERM_MIN_Y) {
          term->y--;
          term->x = TERM_MIN_X;
        }
      }

      term_reset_csi(term);
      break;
    case 'G':  // Cursor horzontal absolute (n=1)
      term_consume_csi_arg_buffer(term);

      if (!term->csi_state->args[0]) {
        term->csi_state->args[0] = 1;
      }
      if (term->csi_state->args[0] > TERM_MAX_X) {
        term->csi_state->args[0] = TERM_MAX_X;
      }

      term->x = term->csi_state->args[0];

      term_reset_csi(term);
      break;
    case 'H':  // Cursor position (n=1, m=1)
      term_consume_csi_arg_buffer(term);

      if (!term->csi_state->args[0]) {
        term->csi_state->args[0] = 1;
      }
      if (term->csi_state->args[0] > TERM_MAX_Y) {
        term->csi_state->args[0] = TERM_MAX_Y;
      }
      if (term->csi_state->args[1] > TERM_MAX_X) {
        term->csi_state->args[1] = TERM_MAX_X;
      }
      if (!term->csi_state->args[1]) {
        term->csi_state->args[1] = 1;
      }

      term->y = term->csi_state->args[0];
      term->x = term->csi_state->args[1];

      term_reset_csi(term);
      break;
    case 'J':  // Erase in display (n=0)
      term_consume_csi_arg_buffer(term);

      if (term->csi_state->args[0] == 0) {  // Clear to end of screen
        for (uint8_t j = term->y; j < TERM_ROWS; j++) {
          for (uint8_t i = 0; i < TERM_COLS; i++) {
            if (j == term->y && i < term->x) continue;
            term_clear_cell(term, i, j);
          }
        }
      } else if (term->csi_state->args[0] == 1) {
        // Clear from cursor to beginning of screen
        for (uint8_t j = 0; j <= term->y; j++) {
          for (uint8_t i = 0; i < TERM_COLS; i++) {
            if (j == term->y && i > term->x) continue;
            term_clear_cell(term, i, j);
          }
        }
      } else if (term->csi_state->args[0] >= 2) {
        term_clear_screen(term);
      }

      term_reset_csi(term);
      break;
    case 'K':  // Erase in line (n=0)
      term_consume_csi_arg_buffer(term);

      if (term->csi_state->args[0] == 0) {  // Clear to end of line
        for (uint8_t i = term->x; i < TERM_COLS; i++) {
          term_clear_cell(term, i, term->y);
        }
      } else if (term->csi_state->args[0] == 1) {
        // Clear from cursor to beginning of line
        for (uint8_t i = 0; i < term->x; i++) {
          term_clear_cell(term, i, term->y);
        }
      } else if (term->csi_state->args[0] >= 2) {
        // Clear whole line
        for (uint8_t i = 0; i < TERM_COLS; i++) {
          term_clear_cell(term, i, term->y);
        }
      }

      term_reset_csi(term);
      break;
    default:
      if (term->csi_state->current_arg == TERM_CSI_ARG_LEN ||
          term->csi_state->arg_buffer_idx == TERM_CSI_ARG_BUFFER_LEN) {
        // Ignore unknown CSI commands/arguments (TODO: too greedy?)
        term_reset_csi(term);
        break;
      }

      term->csi_state->arg_buffer[term->csi_state->arg_buffer_idx] = cur_char;
      term->csi_state->arg_buffer_idx++;
      break;
  }
}

void term_handle_link_byte(term_t* term, unsigned char cur_char) {
  if (term->csi) {
    term_handle_csi_char(term, cur_char);
    return;
  }

  switch (cur_char) {
    case '\0':
    case 0xFF:
      // Skip NULL and dirty reads
      return;
    case 0x1B:  // ESC
      if (!term->esc) {
        // Enable ESC
        term->esc = 1;
      }
      return;
    case 0x5B:  // CSI (7-bit)
      if (term->esc && !term->csi) {
        // Enable CSI
        term->esc = 0;
        term->csi = 1;
        return;
      }
      break;
    case 0x08:  // BS
    case 0x7F:  // DEL

      if (term->x > 1) {  // Current line
        // Blank out current char
        term_clear_cell(term, term->x, term->y);
        term->x--;
      } else {  // Reverse wrap
        // Blank out current char
        term_clear_cell(term, term->x, term->y);
        term->x = TERM_MAX_X;
        term->y--;
      }
      return;
    case '\a':
      // TODO: Ding a bell here.
      return;
    case '\v':
    case '\n':
    case '\r':
      term_advance_line(term);
      return;
    case '\t':
      // Tabs = spaces here. We're working with 20x18!
      cur_char = ' ';
      break;
  }

  // Skip unprintable characters
  if (cur_char < ' ' || cur_char == 0x7F || cur_char == 0xFF) {
    return;
  }

  term->cells[term->y][term->x].ch = cur_char;
  term->cells[term->y][term->x].mode = term_gfx_mode_inverse;
  term->x++;

  if (term->x > TERM_MAX_X) {
    term_advance_line(term);
  }
}
