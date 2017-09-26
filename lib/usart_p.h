#ifndef _USART_P_H
#define _USART_P_H

#include <avr/pgmspace.h>

#if defined USE_PRINTF
void usart_printf_P(const char *__fmt, ...);
#endif

#endif
