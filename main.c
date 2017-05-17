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
  pin0 |-> pin8 (ICR0)

  PORTD
  pin7 |-> pin7 (input pulled up, low blocks writing to usart)

  Connected to: SN74HC595
  Arduino pin13 (SCK) connected to SN74HC595 pin11 (SRCLK)
  Arduino pin11 (MOSI) connected to SN74HC595 pin14 (SER)
  Arduino pin10 connected to SN74HC595 pin12 (RCLK)
  SN74HC595 pin13 low
  SN74HC595 pin10 high
*/

volatile bool i = false;
volatile bool e = false;
volatile bool r = false;
volatile uint16_t v = 0;

void pcint_enable(void) {
  e = false;
  i = false;
  r = true;
  /* enable pin change interrupt 0 */
  PCICR = _BV(PCIE0);
  /* configure pin change interrupt 0 to fire on PCINT0 */
  PCMSK0 = _BV(PCINT0);
}

int main (void) {
  unsigned char counter = 0;
  uint32_t next_time = 0;

  /* set pin 1 and 2 of PORTB for output*/
  DDRB |= _BV(DDB1) | _BV(DDB2);
  /* set pin 6 of PORTD for output*/
  DDRD |= _BV(DDD6);
  /* enable pullup for pin 7 of PORTD */
  PORTD |= _BV(PORTD7);

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
    uint32_t now = timer_millis();
    if (now >= next_time) {
      next_time += 1000;

      if (!r) {
        /* enable pin change interrupt */
        pcint_enable();
        /* set portd pin 6 high for 10 microseconds */
        PORTD |= _BV(PORTD6);
        _delay_us(10);
        PORTD &= ~_BV(PORTD6);
      }

      /* check input on pin 7 of port d, if low, do nothing */
      if (_BV(PORTD7) & PIND) {
        /* increment counter, allowing for wrap around */
        counter++;

        /* set ss to low, write data to SN74HC595, then ss high to store
           data */
        PORTB &= ~_BV(PORTB2);
        spi_master_transmit(counter);
        PORTB |= _BV(PORTB2);

       /* print number that is being written to SN74HC595 */
        usart_printf("Displaying: 0x%02X\n", counter);
        usart_printf("Time: %lu\n", now);
        if (i) {
          usart_printf("Input Captured: %ucm\n", v/58);
          i = false;
          r = false;
        }
        if (e) {
          usart_printf("No Input Captured\n");
          e = false;
          r = false;
        }
      }
    }
  }
}

ISR(PCINT0_vect) {
  /* disable pinchange interrupt 0 */
  PCICR &= ~_BV(PCIE0);
  /* disable PCINT0 */
  PCMSK0 &= ~_BV(PCINT0);
  /* set timer 1 in nominal mode */
  TCCR1A = 0;
  /* no force compare */
  TCCR1C = 0;
  /* enable interrupts for input capture register and overflow */
  TIMSK1 = _BV(ICIE1) | _BV(TOIE1);
  /* clear the timer 1 counter */
  TCNT1 = 0;
  /* start timer 1 with prescaler or 8 */
  TCCR1B = _BV(CS11) | _BV(ICNC1);
  /*  i = true;
      v = PINB & _BV(PORTB0);*/
}

ISR(TIMER1_OVF_vect) {
  /* stop timer1 */
  TCCR1B = 0;
  /* mast timer1 interrupts */
  TIMSK1 = 0;
  e = true;
}

ISR(TIMER1_CAPT_vect) {
  /* stop timer1 */
  TCCR1B = 0;
  /* mast timer1 interrupts */
  TIMSK1 = 0;
  i = true;
  /* divide ICR1 by 2 to convert into microseconds */
  v = ICR1 / 2;
}
