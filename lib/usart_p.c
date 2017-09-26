#if defined USE_PRINTF

#include <stdio.h>
#include <stdarg.h>
#include <avr/pgmspace.h>

#include "usart.h"

#define PRINTF_BUFFER_SIZE 128

void usart_printf_P(const char *__fmt, ...) {
  char buffer[PRINTF_BUFFER_SIZE];
  char fmt[PRINTF_BUFFER_SIZE];
  va_list ap;
  int length = 0;
  uint16_t i;
  for (i = 0; i < PRINTF_BUFFER_SIZE - 1; i++) {
    fmt[i] = pgm_read_byte(__fmt + i);
    if (fmt[i] == 0x00) {
      break;
    }
  }
  fmt[i] = 0x00;

  va_start(ap, __fmt);
  length = vsnprintf(buffer, PRINTF_BUFFER_SIZE, fmt, ap);
  va_end(ap);

  if (length >= PRINTF_BUFFER_SIZE) {
    /* truncated */
    usart_write_bytes((uint8_t*)"ERROR: TRUNCATED\n", 0);
  } else {
    usart_write_bytes((uint8_t*)buffer, 0);
  }
}
#endif
