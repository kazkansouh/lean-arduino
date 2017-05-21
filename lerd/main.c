#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "usart.h"
#include "timer.h"
#include "icr-pulse.h"
#include "pins.h"
#include "command.h"
#include "screen.h"

#define DELAY_MS 500
#define MYUBRR F_CPU/16/BAUD-1

#define ECHO      8
#define TRIGGER   6

/*
  PORTB
  pin5 |-> pin13 (SCK)
  pin3 |-> pin11 (MOSI)
  pin2 |-> pin10 (output/SS)
  pin1 |-> pin9 (output, controls led to report errors)
  pin0 |-> pin8 (ICP1, with external 10k pulldown)

  PORTD
  pin7 |-> pin7 (output, low to enable outputs on SN74CH595)
  pin6 |-> pin6 (output, trigger HC-SR04)

  Connected to: SN74HC595
  Arduino pin13 (SCK) connected to SN74HC595 pin11 (SRCLK)
  Arduino pin11 (MOSI) connected to SN74HC595 pin14 (SER)
  Arduino pin10 connected to SN74HC595 pin12 (RCLK)
  Arduino pin7 connected to SN74HC595 pin13 (OE)
  SN74HC595 pin10 high

  Connected to: HC-SR04
  Arduino pin6 (output) to HC-SR04 trigger
  Arduino pin8 (ICP1) to HC-SR04 echo
*/

int main (void) {
  uint32_t next_ui_update = 0;
  uint32_t next_clock_tick = 0;
  uint32_t next_time_scan = 0;
  uint16_t cm = 100;

#if defined PIN_ERROR
  setMode(PIN_ERROR,output);
#endif

  /* configure trigger pin for HC-SR04 as output */
  setMode(TRIGGER, output);

  /* enable interrupts, used for usart, timer */
  sei();

  /* initilise timer/counter, should be called after sei */
  timer_init();

  /* initilise the usart and write boot message */
  usart_init(MYUBRR);

  /* initilise the screen */
  screen_init();
  screen_clear();
  screen_render();
  screen_enable();

  while(1) {
    uint32_t now = timer_millis();

    /* process any serial commands waiting */
    command_process();

    /* update the clock */
    if (now >= next_clock_tick) {
      next_clock_tick += ui_command_millisecond_in_second;
      ui_command_clock++;
    }

    /* send an echo pulse */
    if (now >= next_time_scan) {
      next_time_scan += 123;
      /* enable pin change interrupt */
      icr_pulse_enable();
      /* set trigger high for 10 microseconds */
      writePin(TRIGGER,true);
      _delay_us(10);
      writePin(TRIGGER,false);
    }

    if (icr_pulse_done) {
      cm = icr_pulse_value / 58;
      icr_pulse_done = false;
    }

    if (icr_pulse_error) {
      /* todo: report error */
      icr_pulse_error = false;
    }

    if (now >= next_ui_update) {
      next_ui_update += cm * 10;

      screen_update();
      screen_usart_dump();
    }

    screen_render();
  }
}
