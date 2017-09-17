#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#include "usart.h"

#define DELAY_MS 500
#define MYUBRR F_CPU/16/BAUD-1

/*
  Simple program to demonstrate using objcopy to load binary data into
  the program memory section.

  Change the test.bin file to what is required (up to flash size
  limitations), and the below code will print it out 1 byte at a time.
 */

/* these are required for interfacing with objcopy */
extern const PROGMEM unsigned char _binary_test_bin_start;
extern const PROGMEM unsigned char _binary_test_bin_end;
extern const PROGMEM unsigned char _binary_test_bin_size;

int main (void) {
  const uint8_t *ptr = &_binary_test_bin_start;

  /* enable interrupts, used for usart */
  sei();

  /* initilise the usart and write boot message */
  usart_init(MYUBRR);
  usart_printf("Hello World\n");

  /* iterate over the bytes and print 1 at a time */
  while (ptr != &_binary_test_bin_end) {
    usart_printf("Byte: %02X\n", pgm_read_byte(ptr));
    ptr++;
  }

  usart_printf("Done\n");

  while (1) {}
}
