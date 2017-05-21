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

  The below two variables define the uint32's that are written to the
  screen. Two are used, so that it allows for double buffering,
  i.e. data2 is copied to data when ready to be displayed.
*/
uint32_t data[ROWS];
uint32_t data2[ROWS];

#define INITIAL_PATTERN(row) (0x15555400 | (1 << row))

/* pre-calculated row selectors */
const uint16_t rowselector[ROWS] = {
  (1 << 0),  (1 << 1),  (1 << 2),  (1 << 3),  (1 << 4),
  (1 << 5),  (1 << 6),  (1 << 7),  (1 << 8),  (1 << 9)
};

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

/*
  The initial pattern sets the row bit, this is to save having to
  compute it again later.
*/
void screen_clear(void) {
  int i;
  for (i = 0; i < ROWS; i++) {
    data[i] = INITIAL_PATTERN(i);
  }
}

/* define 4 null bytes */
const uint32_t i_null = 0;

void screen_render(void) {
  int i;
  for (i = 0; i < ROWS; i++) {
    /* write the current row to the 74HC595 */
    writePin(PIN_SS, false);
    spi_master_transmit_32(data + i);
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
    volatile uint16_t* ptr = (uint16_t*)(data2 + i);
    /* mask the row selector */
    *ptr &= 0xFC00;
    /* shit all bits by 1 to scroll the screen */
    data2[i] <<= 1;
    /* set bit 10 */
    if (i >=5) {
      *ptr |= (i-5 > ((j + 3) % 5)) << 10;
    } else {
      *ptr |= (i > j) << 10;
    }
    /* set the row selector */
    *ptr |= rowselector[i];
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
    dump[i++] = ((uint8_t*)(data + j))[3];
    dump[i++] = ((uint8_t*)(data + j))[2];
    dump[i++] = ((uint8_t*)(data + j))[1];
    dump[i++] = ((uint8_t*)(data + j))[0];
  }

  usart_write_bytes(dump,(5 * ROWS) + 1);
}
