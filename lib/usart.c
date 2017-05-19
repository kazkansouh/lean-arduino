#include <avr/interrupt.h>
#include <stdbool.h>

#if defined USE_PRINTF
  #include <stdio.h>
  #include <stdarg.h>
#endif

#if defined PIN_ERROR
  #include <pins.h>
#endif

#include "usart.h"
#include "queue.h"

#define SEND_BUFFER_SIZE 255
#define RECEIVE_BUFFER_SIZE 50
#define PRINTF_BUFFER_SIZE 128

uint8_t usart_sending[SEND_BUFFER_SIZE];
uint8_t usart_sending_head = 0;
uint8_t usart_sending_tail = 0;
bool usart_is_sending = false;

uint8_t usart_receiving[RECEIVE_BUFFER_SIZE];
uint8_t usart_receiving_head = 0;
uint8_t usart_receiving_tail = 0;

void usart_init(uint16_t ubrr) {
  /* Set baud rate */
  UBRR0H = (uint8_t)(ubrr>>8);
  UBRR0L = (uint8_t)(ubrr & 0xFF);
  /* Enable receiver and transmitter */
  UCSR0B = _BV(RXEN0) | _BV(TXEN0) | _BV(TXCIE0) | _BV(RXCIE0);
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
#if defined PIN_ERROR
      /* Should not happen as called from interrupt */
      writePin(PIN_ERROR, true);
#endif
    }
    /* Put data into buffer, sends the data */
    UDR0 = data;
  } else {
    usart_is_sending = false;
  }
}

/*
  copies data into the send buffer. if length is 0, then it is assumed
  that the data is text and terminated by a null byte.
 */
void usart_write_bytes(const uint8_t* const data, uint16_t length) {
  const uint8_t* x = data;
  bool r = false;
  while((length == 0 && *x != '\0') || (length > x - data)) {
    if (*x == '\n' && !r && length == 0) {
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

#if defined USE_PRINTF
void usart_printf(const char *__fmt, ...) {
  char buffer[PRINTF_BUFFER_SIZE];
  va_list ap;
  int length = 0;
  va_start(ap, __fmt);
  length = vsnprintf(buffer, PRINTF_BUFFER_SIZE, __fmt, ap);
  va_end(ap);

  if (length >= PRINTF_BUFFER_SIZE) {
    /* truncated */
    usart_write_bytes((uint8_t*)"ERROR: TRUNCATED\n", 0);
  } else {
    usart_write_bytes((uint8_t*)buffer, 0);
  }
}
#endif

bool usart_data_pending(void) {
  return usart_receiving_head != usart_receiving_tail;
}

bool usart_get_char(uint8_t* data) {
  return queue_dequeue(usart_receiving,
                       RECEIVE_BUFFER_SIZE,
                       usart_receiving_head,
                       &usart_receiving_tail,
                       data);
}

ISR(USART_TX_vect) {
  usart_write();
}

ISR(USART_RX_vect) {
  uint8_t data = UDR0;
  if (!queue_enqueue(usart_receiving,
                     RECEIVE_BUFFER_SIZE,
                     &usart_receiving_head,
                     usart_receiving_tail,
                     data)) {
#if defined PIN_ERROR
    /* Occours when receive buffer full */
    writePin(PIN_ERROR, true);
#endif
  }
}
