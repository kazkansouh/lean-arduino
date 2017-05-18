#ifndef _ICR_PULSE_H
#define _ICR_PULSE_H

/* these are set by ISR */
extern volatile bool icr_pulse_done;
extern volatile bool icr_pulse_error;
extern volatile uint16_t icr_pulse_value;

void icr_pulse_enable(void);
void icr_pulse_disable(void);

#endif
