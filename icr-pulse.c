#include <avr/interrupt.h>
#include <stdbool.h>

volatile bool icr_pulse_done = false;
volatile bool icr_pulse_error = false;
volatile uint16_t icr_pulse_value = 0;

/*
  The below code is designed to mimic the Arduino pulseIn function,
  however using the ICR mechanism of timer1 with interrupts instead of
  a software implementation. This means that it will only work on
  portb, pin0 of the atmega328p.

  The below was developed to wait for the rising edge on to be
  detected by ICR, start timer 1 with prescaler 8, then wait for
  faling edge and save the value of the timer into ICR1.
 */

void icr_pulse_enable(void) {
  /* initilise the state tracking variables */
  icr_pulse_error = false;
  icr_pulse_done = false;
  /* configure icr on rising edge with noise filter */
  TCCR1B = _BV(ICES1) | _BV(ICNC1);
  /* unmask icr */
  TIMSK1 = _BV(ICIE1);
}

void icr_pulse_disable(void) {
  /* initilise the state tracking variables */
  icr_pulse_error = false;
  icr_pulse_done = false;
  /* disable timer1 and ICR settings */
  TCCR1B = 0;
  /* mask all timer1 interrupts */
  TIMSK1 = 0;
}

ISR(TIMER1_OVF_vect) {
  /* stop timer1 */
  TCCR1B = 0;
  /* mask timer1 interrupts */
  TIMSK1 = 0;
  /* mark error status */
  icr_pulse_error = true;
}

ISR(TIMER1_CAPT_vect) {
  if (TCCR1B & _BV(ICES1)) {
    /* looking for up edge */

    /* clear the timer 1 counter */
    TCNT1 = 0;
    /* enable interrupt for overflow */
    TIMSK1 |= _BV(TOIE1);
    /* start timer 1 with prescaler or 8 */
    TCCR1B = _BV(CS11) | _BV(ICNC1);
  } else {
    /* looking for down edge */

    /* stop timer1 */
    TCCR1B = 0;
    /* mask timer1 interrupts */
    TIMSK1 = 0;
    /* make cycle as complete */
    icr_pulse_done = true;
    /* divide ICR1 by 2 to convert into microseconds */
    icr_pulse_value = ICR1 / (F_CPU/1000000/8);
  }
}
