#include "command.h"
#include "usart.h"
#include "screen.h"

typedef enum {
  eSearching = 0,
  eCommandId = 1,
  eCommandSpeed1 = 2,
  eCommandSpeed2 = 3,
  eCommandSetTime1 = 4,
  eCommandSetTime2 = 5,
  eCommandSetTime3 = 6,
  eCommandSetTime4 = 7
} EParseState;

EParseState e_command_parse_state = eSearching;

uint16_t ui_command_new_millisecond_in_second = 0;
uint16_t ui_command_millisecond_in_second = 1000;

uint32_t ui_command_clock = 0;
uint32_t ui_command_new_clock = 0;

/*
  below code responds to input on the serial port. it follows a simple
  protocol where 0xFF indicates the beginning of a command, followed
  by a byte that details the command.

  the following bytes are defined by the command payload.
 */
void command_process() {
  uint8_t c;
  while (usart_get_char(&c)) {
    switch (e_command_parse_state) {
    case eSearching:
      if (c == 0xFF) {
        e_command_parse_state = eCommandId;
      }
      break;
    case eCommandId:
      switch (c) {
      case 0x00:
        /* reset counter */
        ui_command_clock = 0;
        e_command_parse_state = eSearching;
        break;
      case 0x01:
        /* enable scroll */
        b_screen_enable_scroll = true;
        e_command_parse_state = eSearching;
        break;
      case 0x02:
        /* disable scroll */
        b_screen_enable_scroll = false;
        /*uiOffset = 0;*/
        e_command_parse_state = eSearching;
        break;
      case 0x03:
        /* set clock speed */
        e_command_parse_state = eCommandSpeed1;
        break;
      case 0x04:
        /* set clock time */
        e_command_parse_state = eCommandSetTime1;
        break;
      default:
        e_command_parse_state = eSearching;
      }
      break;
    case eCommandSpeed1:
      ui_command_new_millisecond_in_second = ((uint16_t)c) << 8;
      e_command_parse_state = eCommandSpeed2;
      break;
    case eCommandSpeed2:
      ui_command_new_millisecond_in_second |= c;
      ui_command_millisecond_in_second = ui_command_new_millisecond_in_second;
      e_command_parse_state = eSearching;
      break;
    case eCommandSetTime1:
      ui_command_new_clock = ((uint32_t)c) << 24;
      e_command_parse_state = eCommandSetTime2;
      break;
    case eCommandSetTime2:
      ui_command_new_clock |= ((uint32_t)c) << 16;
      e_command_parse_state = eCommandSetTime3;
      break;
    case eCommandSetTime3:
      ui_command_new_clock |= ((uint32_t)c) << 8;
      e_command_parse_state = eCommandSetTime4;
      break;
    case eCommandSetTime4:
      ui_command_new_clock |= c;
      ui_command_clock = ui_command_new_clock;
      e_command_parse_state = eSearching;
      break;
    }
  }
}
