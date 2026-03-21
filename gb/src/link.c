#include "link.h"

volatile unsigned char link_rx_buffer[LINK_RX_BUFFER_SIZE];
volatile uint8_t link_rx_buffer_head = 0;
volatile uint8_t link_rx_buffer_tail = 0;

volatile unsigned char link_tx_buffer[LINK_TX_BUFFER_SIZE];
volatile uint8_t link_tx_buffer_head = 0;
volatile uint8_t link_tx_buffer_tail = 0;

void link_port_write(unsigned char data) {
  CRITICAL {
    uint8_t next_head = (link_tx_buffer_head + 1) % LINK_TX_BUFFER_SIZE;
    if (next_head != link_tx_buffer_tail) {
      link_tx_buffer[link_tx_buffer_head] = data;
      link_tx_buffer_head = next_head;
    }
  }
}

void link_port_interrupt(void) {
  uint8_t data = SB_REG;

  if (data != TERM_SYN_IDLE) {
    uint8_t next_head = (link_rx_buffer_head + 1) % LINK_RX_BUFFER_SIZE;
    if (next_head != link_rx_buffer_tail) {
      link_rx_buffer[link_rx_buffer_head] = data;
      link_rx_buffer_head = next_head;
    }
  }

  if (link_tx_buffer_head != link_tx_buffer_tail) {
    unsigned char next_char = link_tx_buffer[link_tx_buffer_tail];
    link_tx_buffer_tail = (link_tx_buffer_tail + 1) % LINK_TX_BUFFER_SIZE;
    SB_REG = next_char;
  } else {
    SB_REG = TERM_ACK;
  }
  SC_REG = SIOF_CLOCK_EXT | SIOF_XFER_START;
}

void link_port_init(void) {
  CRITICAL { add_SIO(link_port_interrupt); }

  SB_REG = TERM_ACK;

  // Set to external clock, we're not drivin' this thing
  SC_REG = SIOF_CLOCK_EXT | SIOF_XFER_START;

  set_interrupts(SIO_IFLAG | VBL_IFLAG);
}
