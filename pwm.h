#ifndef _PWM_H
#define _PWM_H

#include <stdint.h>

void pwm_init(void);

inline
void pwm_set_value(uint8_t val) {
  OCR2B = val;
}

#endif
