#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

#include "usart.h"
#include "queue.h"

#define SEND_BUFFER_SIZE 255
#define PRINTF_BUFFER_SIZE 128

uint8_t usart_sending[SEND_BUFFER_SIZE];
uint8_t usart_sending_head = 0;
uint8_t usart_sending_tail = 0;
bool usart_is_sending = false;

void usart_init(uint16_t ubrr) {
  /* Set baud rate */
  UBRR0H = (uint8_t)(ubrr>>8);
  UBRR0L = (uint8_t)(ubrr & 0xFF);
  /* Enable receiver and transmitter */
  UCSR0B = /*(1<<RXEN0)|*/(1<<TXEN0) | (1<<TXCIE0);
  /* Set frame format: 8data, 1stop bit, no parity */
  UCSR0C = (3<<UCSZ00);
}

inline
void usart_write() {
  uint8_t data;
  usart_is_sending = true;
  if (queue_dequeue(usart_sending,
                    SEND_BUFFER_SIZE,
                    usart_sending_head,
                    &usart_sending_tail,
                    &data)) {
    /* Wait for empty transmit buffer */
    while ( !( UCSR0A & (1<<UDRE0)) ) {
      PORTB |= _BV(PORTB1); /* Should not happen as from interrupt */
    }
    /* Put data into buffer, sends the data */
    UDR0 = data;
  } else {
    usart_is_sending = false;
  }
}

void usart_write_bytes(const uint8_t* const data , bool istext) {
  const uint8_t* x = data;
  bool r = false;
  while(*x != '\0') {
    if (*x == '\n' && !r && istext) {
      r = queue_enqueue(usart_sending,
                        SEND_BUFFER_SIZE,
                        &usart_sending_head,
                        usart_sending_tail,
                        '\r');
    } else {
      if (queue_enqueue(usart_sending,
                        SEND_BUFFER_SIZE,
                        &usart_sending_head,
                        usart_sending_tail,
                        *x)) {
        x++;
        r = false;
      }
    }
    if (!usart_is_sending) {
      usart_write();
    }
  }
}

void usart_printf(const char *__fmt, ...) {
  char buffer[PRINTF_BUFFER_SIZE];
  va_list ap;
  int length = 0;
  va_start(ap, __fmt);
  length = vsnprintf(buffer, PRINTF_BUFFER_SIZE, __fmt, ap);
  va_end(ap);

  if (length >= PRINTF_BUFFER_SIZE) {
    /* truncated */
    usart_write_bytes((uint8_t*)"ERROR: TRUNCATED\n", true);
  } else {
    usart_write_bytes((uint8_t*)buffer, true);
  }
}

ISR(USART_TX_vect) {
  usart_write();
}
