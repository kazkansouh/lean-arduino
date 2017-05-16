#include <avr/interrupt.h>

#include "timer.h"

/* define prescaler for timer 0 of 64 */
#define PRESCALER _BV(CS01) | _BV(CS00)
/* define the 8bit wraparound value for timer */
#define COUNTER_MAX 250
/*
  64 cycles in a increment, 250 increment to overflow timer 0

  each overflow is 16000 cycles

  clock at 16MHz, so 1000 overflows in a second

  i.e. 1 overflow is 1 millisecond
*/

/*
  overflows every (49.7 days)
 */
volatile uint32_t ticks = 0;

void timer_init(void) {
  /* initilise timer 0 counter with 0 */
  TCNT0 = 0;
  /* enable overflow interrupt on timer 0 */
  TIMSK0 = _BV(OCF0A);
  /* setup timer 0 with no pwm and ctc operation */
  TCCR0A = _BV(WGM01);
  /* set a comparer of 250 */
  OCR0A = COUNTER_MAX;
  /* setup timer 0 with prescaler of 64. this line enables the
     timer */
  TCCR0B = PRESCALER;
}

uint32_t timer_millis(void) {
  return ticks;
}

ISR(TIMER0_COMPA_vect) {
  ticks++;
}
