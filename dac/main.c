#include <avr/io.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdint.h>

#define PAUSE_MS 1
#define DELAY_MS 500

extern unsigned char _binary_hello_8u_full_raw_start;
extern unsigned char _binary_hello_8u_full_raw_end;
extern unsigned char _binary_hello_8u_full_raw_size;

int main (void) {
  uint8_t *table = &_binary_hello_8u_full_raw_start;
  DDRD = 0xFF; /* port D to output */

  while (true) {
    PORTD = *table;
    if (++table >= &_binary_hello_8u_full_raw_end) {
      table = &_binary_hello_8u_full_raw_start;
      _delay_ms(DELAY_MS);
    }
    _delay_us(512);
  }
  return 0;
}
