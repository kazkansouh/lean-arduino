#include <util/delay.h>
#include <avr/io.h>
#include <string.h> /* for memcpy */
#include <stdbool.h>

#include "screen.h"
#include "spi.h"
#include "pins.h"
#include "usart.h"

#define ROWS    10
#define COLUMNS 20

/* double buffering of data buffer, lower COLUMNS bits of each row
   define the bitmap of the row */
uint32_t data[ROWS];
uint32_t data2[ROWS];

bool b_screen_enable_scroll = true;

void screen_init(void) {
  /* set slave select as output */
  setMode(PIN_SS, output);
  /* enable spi master */
  spi_master_init();
  /* disable output on screen */
  writePin(PIN_OE,true);
  /* set output enable as output */
  setMode(PIN_OE, output);
}

void screen_enable(void) {
  /* enable output on screen */
  writePin(PIN_OE,false);
}

void screen_clear(void) {
  int i;
  for (i = 0; i < ROWS; i++) {
    data[i] = 0x00055555;
  }
}

/*
  Format of uint32 that is written to screen:

  MSB                                 LSB
  XXYY YYYY YYYY YYYY YYYY YYZZ ZZZZ ZZZZ

  where:
  X is unused
  Y are the 20 bits that define the led in row that are on
  Z are the bits that select a row (at most one should be high)

  E.g.: To set all LEDs on, on a given line, the following double
  word would be used:

  3    F    F    F    F    C    0    1
  0011 1111 1111 1111 1111 1100 0000 0001

  writePin(PIN_SS, false);
  spi_master_transmit_32(0x3FFFFC01);
  writePin(PIN_SS, true);

  This uint32_t should be written 10 times, each with a different Z
  set high.
*/
/* define 4 null bytes */
const uint32_t i_null = 0;

void screen_render(void) {
  int i;
  for (i = 0; i < ROWS; i++) {
    /* shift the display buffer along by 10 bits and set the row
       selector bit in the lower 10 bits */
    uint32_t row = data[i] << 10 | (1 << i);

    /* write the current row to the 74HC595 */
    writePin(PIN_SS, false);
    spi_master_transmit_32(&row);
    writePin(PIN_SS, true);
    /* small delay to allow the leds to illuminate */
    _delay_us(10);
  }
  writePin(PIN_SS, false);
  spi_master_transmit_32(&i_null);
  writePin(PIN_SS, true);
}

void screen_update(void) {
  int i;
  static int j = 0;

  if (!b_screen_enable_scroll) {
    return;
  }

  screen_render();

  memcpy(data2, data, sizeof(uint32_t) * ROWS);
  j = (j + 1) % 5;
  for (i = 0; i < ROWS; i++) {
    data2[i] <<= 1;
    if (i >=5) {
      data2[i] |= i-5 > ((j + 3) % 5);
    } else {
      data2[i] |= i > j;
    }
  }

  memcpy(data, data2, sizeof(uint32_t) * ROWS);
}


void screen_usart_dump() {
  uint8_t dump[(5 * ROWS) + 1];
  uint8_t i = 0;
  uint8_t j;
  dump[i++] = 0xFF;

  for (j = 0; j < ROWS; j++) {
    dump[i++] = 0xFF;
    dump[i++] = data[j] >> 24;
    dump[i++] = (data[j] >> 16) & 0xFF;
    dump[i++] = (data[j] >> 8) & 0xFF;
    dump[i++] = data[j] & 0xFF;
  }

  usart_write_bytes(dump,(5 * ROWS) + 1);
}
