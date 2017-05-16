#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "usart.h"
#include "spi.h"
#include "timer.h"

#define DELAY_MS 500
#define MYUBRR F_CPU/16/BAUD-1

/*
  PORTB
  pin5 |-> pin13 (SCK)
  pin3 |-> pin11 (MOSI)
  pin2 |-> pin10 (output/SS)
  pin1 |-> pin9 (output, controls led to report errors)
  pin0 |-> pin8 (input pulled up, low blocks writing to usart)

  Connected to: SN74HC595
  Arduino pin13 (SCK) connected to SN74HC595 pin11 (SRCLK)
  Arduino pin11 (MOSI) connected to SN74HC595 pin14 (SER)
  Arduino pin10 connected to SN74HC595 pin12 (RCLK)
  SN74HC595 pin13 low
  SN74HC595 pin10 high
*/

int main (void) {
  unsigned char counter = 0;
  /* set pin 1 and 2 of PORTB for output*/
  DDRB |= _BV(DDB1) | _BV(DDB2);
  /* enable pullup for pin 0 of PORTB */
  PORTB |= _BV(PORTB0);

  /* enable interrupts, used for usart */
  sei();

  /* initilise timer/counter, should be called after sei */
  timer_init();

  /* initilise the usart and write boot message */
  usart_init(MYUBRR);
  usart_printf("Hello World\n");

  /* set ss high */
  PORTB |= _BV(PORTB2);
  spi_master_init();

  /* set ss to low, write data to SN74HC595, then ss high to store
     data */
  PORTB &= ~_BV(PORTB2);
  spi_master_transmit(0x00);
  PORTB |= _BV(PORTB2);

  while(1) {
    /* check input on pin 1 of port b, if low, do nothing */
    if (0x01 & PINB) {
      /* increment counter, allowing for wrap around */
      counter++;

      /* set ss to low, write data to SN74HC595, then ss high to store
         data */
      PORTB &= ~_BV(PORTD2);
      spi_master_transmit(counter);
      PORTB |= _BV(PORTD2);

      _delay_ms(DELAY_MS);
      /* clear output on pin1 so led is off, usart driver might set if
         high if error detected */
      PORTB &= ~_BV(PORTB1);
      _delay_ms(DELAY_MS);

      /* print number that is being written to SN74HC595 */
      usart_printf("Displaying: 0x%02X\n", counter);
      usart_printf("Time: %d\n", timer_millis());
    }
  }
}
