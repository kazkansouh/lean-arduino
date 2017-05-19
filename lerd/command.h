#ifndef _COMMAND_H
#define _COMMAND_H

#include <stdint.h>

void command_process(void);

extern uint16_t ui_command_millisecond_in_second;
extern uint32_t ui_command_clock;

#endif
