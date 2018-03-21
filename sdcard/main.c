#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "usart.h"
#include "sdcard.h"
#include "sdcard-fat.h"

#include "pins.h"

#define MYUBRR F_CPU/16/BAUD-1

#define SCK       13
#define MISO      12
#define MOSI      11
#define SS        10

/*
  PORTB
  pin5 |-> pin13 (SCK)
  pin4 |-> pin12 (MISO)
  pin3 |-> pin11 (MOSI)
  pin2 |-> pin10 (output/SS)
  pin1 |-> pin9  (error pin, defined in makefile)

  PORTD
  pin0 |-> pin0 (LSB)
  pin1 |-> pin1
  pin2 |-> pin2
  pin3 |-> pin3
  pin4 |-> pin4
  pin5 |-> pin5
  pin6 |-> pin6
  pin7 |-> pin7 (MSB)

  Connected to: SD Card
  Arduino pin13 (SCK) connected to (SCK)
  Arduino pin12 (MISO) connected to (DO)
  Arduino pin11 (MOSI) connected to (DI)
  Arduino pin10 (SS) connected to (CS)

  Connected to: R2R Ladder
  All pins of PORTD are connected to the R2R
  Ladder

  Description:
  Program demonstrates playing audio files that are stored on a SD
  card and writing the output to PORTD that is connected to a R2R
  ladder for DAC. The input file is expected to be unsigned 8bit
  uncompressed PCM with no headers (i.e. biased at 128). In tests, the
  system was capable of playing data sampled at 44.1KHz. To create a
  valid file, Audacity was used to export a clip of the audio. I.e:
    File -> Export -> Select "Other uncompressed files"
    Select "Options...", then
      Headers: RAW
      Encoding: Unsigned 8 bit PCM
*/

SSDCard g_sdcard;
SSDFATCard g_sdfatcard;

int main(void) {
  uint8_t r;

  DDRD = 0xFF; /* port D to output */

  /* flash led on on boot */
  writePin(PIN_ERROR, true);
  setMode(PIN_ERROR, output);
  _delay_ms(250);
  writePin(PIN_ERROR, false);

  /* enable interrupts, used for timer */
  sei();

  /* initilise the sdcard interface (includes spi) */
  r = sdcard_init(&g_sdcard);
  if (r != 0) {
    usart_init(MYUBRR);
    usart_printf("Could not init sdcard");
    goto end;
  }

  /* initilise the fat partition structure */
  r = fat32_init(&g_sdcard, &g_sdfatcard);
  if (r != 0) {
    usart_init(MYUBRR);
    usart_printf("Could not init fat");
    goto end;
  }

  {
    SSDFAT_File s_sdfile;
    int16_t byte;

    r = fat32_file_open(&g_sdfatcard,
                        &s_sdfile,
                        FILE_NAME);
    if (r != 0) {
      usart_init(MYUBRR);
      usart_printf("Could not open file");
      goto end;
    }

    /* read file byte by byte and write to portd, the delay of 17us
       was manually calculated by timing the duration of audio
       playback */
    while ((byte = fat32_file_read_byte_spi(&s_sdfile)) >= 0) {
      PORTD = byte & 0xFF;
      _delay_us(17);
    }
  }

  /* once finished, flash led */
  while(1) {
    writePin(PIN_ERROR, true);
    _delay_ms(250);
    writePin(PIN_ERROR, false);
    _delay_ms(250);
  }

 end:
  /* on error set led to always on */
  writePin(PIN_ERROR, true);
  while(1) {}
}
