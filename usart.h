#ifndef _USART_H
#define _USART_H

#include <stdint.h>
#include <stdbool.h>

void usart_init(uint16_t ubrr);
void usart_write_bytes( const uint8_t* const data , bool istext);
void usart_printf(const char *__fmt, ...);

#endif
