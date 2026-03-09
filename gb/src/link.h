#ifndef LINK_H
#define LINK_H

#include <gb/gb.h>
#include <stdint.h>

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

#define LINK_RX_BUFFER_SIZE 128
extern unsigned char link_rx_buffer[LINK_RX_BUFFER_SIZE];
extern volatile uint8_t link_rx_buffer_head;
extern volatile uint8_t link_rx_buffer_tail;

#define LINK_TX_BUFFER_SIZE 32
extern unsigned char link_tx_buffer[LINK_TX_BUFFER_SIZE];
extern volatile uint8_t link_tx_buffer_head;
extern volatile uint8_t link_tx_buffer_tail;

void link_port_init(void);
void link_port_write(unsigned char data);
void link_port_interrupt(void);

#endif
