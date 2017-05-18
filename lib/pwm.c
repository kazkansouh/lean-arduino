
#include <avr/interrupt.h>

void pwm_init(void) {
  /* PORTD pin 3 is OC2B */
  /* set pin as output */
  DDRD |= _BV(DDD3);
  /* enable the fast pwm control of the pin (non-inverting) */
  TCCR2A |= _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
  /* start timer 2 with prescaler of 32 */
  TCCR2B |= _BV(CS21) | _BV(CS20);  
}
