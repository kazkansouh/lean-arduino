#ifndef _USART_H
#define _USART_H

#include <stdint.h>
#include <stdbool.h>

void usart_init(uint16_t ubrr);
void usart_write_bytes(const uint8_t* const data, uint16_t length);
#if defined USE_PRINTF
void usart_printf(const char *__fmt, ...);
#endif

bool usart_data_pending(void);
bool usart_get_char(uint8_t* data);

#endif
