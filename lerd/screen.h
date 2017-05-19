#ifndef _SCREEN_H
#define _SCREEN_H

#define PIN_SCK       13
#define PIN_MOSI      11
#define PIN_SS        10
#define PIN_OE        7

void screen_init(void);
void screen_update(void);
void screen_clear(void);
void screen_usart_dump(void);
void screen_render(void);
void screen_enable(void);

extern bool b_screen_enable_scroll;

#endif
