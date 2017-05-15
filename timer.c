#include <avr/interrupt.h>

#include "timer.h"

/* define prescaler for timer 0 of 64 */
#define PRESCALER _BV(CS01) | _BV(CS00)
/*
  64 cycles in a increment, 256 increment to overflow timer 0

  each overflow is 16384 cycles

  clock at 16MHz, so 976.5625 overflows in a second

  1000/976.5625 = 1.024 ticks to a millisecond
 */
#define TICKS_TO_MILLISECOND 1.024

/*
  overflows every 4398046 seconds (51 years)
 */
volatile uint32_t ticks = 0;

void timer_init(void) {
  /* initilise timer 0 counter with 0 */
  TCNT0 = 0;
  /* enable overflow interrupt on timer 0 */
  TIMSK0 = _BV(TOIE0);
  /* setup timer 0 with no pwm and normal counter operation */
  TCCR0A = 0;
  /* setup timer 0 with prescaler of 64, i.e. 976.5625 TOV in a
     second. this line enables the timer */
  TCCR0B = PRESCALER;
}

uint32_t timer_millis(void) {
  return ticks * TICKS_TO_MILLISECOND;
}

ISR(TIMER0_OVF_vect) {
  ticks++;
}
