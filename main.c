#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "usart.h"
#include "spi.h"
#include "timer.h"
#include "icr-pulse.h"
#include "pwm.h"

#include "pins.h"

#define DELAY_MS 500
#define MYUBRR F_CPU/16/BAUD-1

#define SCK       13
#define MOSI      11
#define SS        10
#define ERROR_LED 9
#define ECHO      8
#define NO_REPORT 7
#define TRIGGER   6

/*
  PORTB
  pin5 |-> pin13 (SCK)
  pin3 |-> pin11 (MOSI)
  pin2 |-> pin10 (output/SS)
  pin1 |-> pin9 (output, controls led to report errors)
  pin0 |-> pin8 (ICP1, with external 10k pulldown)

  PORTD
  pin7 |-> pin7 (input pulled up, low blocks writing to usart)
  pin6 |-> pin6 (output, trigger HC-SR04)
  pin3 |-> pin3 (OC2B, connected to led)

  Connected to: SN74HC595
  Arduino pin13 (SCK) connected to SN74HC595 pin11 (SRCLK)
  Arduino pin11 (MOSI) connected to SN74HC595 pin14 (SER)
  Arduino pin10 connected to SN74HC595 pin12 (RCLK)
  SN74HC595 pin13 low
  SN74HC595 pin10 high

  Connected to: HC-SR04
  Arduino pin6 (output) to HC-SR04 trigger
  Arduino pin8 (ICP1) to HC-SR04 echo
*/

int main (void) {
  uint32_t next_time_print = 0;
  uint32_t next_time_scan = 0;
  uint8_t depth = 0;
  double accum = 0;

  setMode(ERROR_LED, output);
  setMode(SS, output);
  setMode(TRIGGER, output);
  setMode(NO_REPORT, input_pullup);

  /* initilise pwm  */
  pwm_init();

  /* enable interrupts, used for usart */
  sei();

  /* initilise timer/counter, should be called after sei */
  timer_init();

  /* initilise the usart and write boot message */
  usart_init(MYUBRR);
  usart_printf("Hello World\n");

  /* set ss high */
  writePin(SS,true);
  spi_master_init();

  /* set ss to low, write data to SN74HC595, then ss high to store
     data */
  writePin(SS,false);
  spi_master_transmit(0x00);
  writePin(SS,true);

  while(1) {
    uint32_t now = timer_millis();

    if (now >= next_time_scan) {
      next_time_scan += 10;
      /* enable pin change interrupt */
      icr_pulse_enable();
      /* set portd pin 6 high for 10 microseconds */
      writePin(TRIGGER,true);
      _delay_us(10);
      writePin(TRIGGER,false);
    }

    if (icr_pulse_done) {
      double cm = icr_pulse_value/58;
      accum += cm;
      cm = accum / 10;
      accum -= cm;
      if (cm < 10) {
        depth = 0x01;
      } else if (cm < 15) {
        depth = 0x03;
      } else if (cm < 20) {
        depth = 0x07;
      } else if (cm < 25) {
        depth = 0x0F;
      } else if (cm < 30) {
        depth = 0x1F;
      } else if (cm < 35) {
        depth = 0x3F;
      } else if (cm < 45) {
        depth = 0x7F;
      } else {
        depth = 0xFF;
      }

      icr_pulse_done = false;

      /* set ss to low, write data to SN74HC595, then ss high to store
         data */
      writePin(SS,false);
      spi_master_transmit(depth);
      pwm_set_value(depth);
      writePin(SS,true);
    }

    if (now >= next_time_print) {
      next_time_print += 1000;

      /* check input on pin 7 of port d, if low, do nothing */
      if (readPin(NO_REPORT)) {
       /* print number that is being written to SN74HC595 */
        usart_printf("Displaying: 0x%02X\n", depth);
        usart_printf("Time: %lu\n", now);
      }
    }
  }
}
