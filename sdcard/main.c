#include <avr/interrupt.h>

#include "usart_p.h"
#include "usart.h"
#include "sdcard.h"
#include "sdcard-fat.h"

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

  Connected to: SD Card
  Arduino pin13 (SCK) connected to (SCK)
  Arduino pin12 (MISO) connected to (DO)
  Arduino pin11 (MOSI) connected to (DI)
  Arduino pin10 (SS) connected to (CS)

  Opens a file on sdcard and prints its bytes to the terminal.
*/

/* change this to the name of the file, uses unix syntax and supports
   vfat names */
#define FILENAME "/test.txt"

/* declare space for file system structures */
SSDCard g_sdcard;
SSDFATCard g_sdfatcard;

int main(void) {
  uint8_t r;

  /* enable interrupts, used for usart */
  sei();

  /* initilise the usart and write boot message */
  usart_init(MYUBRR);
  usart_printf_P(PSTR("SDCard Demonstrator\n"));

  /* initilise the sdcard interface (includes spi) */
  r = sdcard_init(&g_sdcard);
  if (r != 0) {
    goto end;
  }

  /* initilise the fat partition structure */
  r = fat32_init(&g_sdcard, &g_sdfatcard);
  if (r != 0) {
    goto end;
  }

  /* read the file */
  {
    SSDFAT_File s_sdfile;
    int16_t byte;
    uint32_t size = 0;

    r = fat32_file_open(&g_sdfatcard,
                        &s_sdfile,
                        FILENAME);
    if (r != 0) {
      goto end;
    }

    /* read the file byte by byte  */
    while ((byte = fat32_file_read_byte(&s_sdfile)) >= 0) {
      if (size % 16 == 0) {
        if (size > 0) {
          usart_printf("\n");
        }
        usart_printf("%08lx: ", size);
      }

      usart_printf("%02x ", byte);

      size++;
    }

    usart_printf("\n");

    /* incase of error, print the position */
    if (size != s_sdfile.ui_file_size) {
      usart_init(MYUBRR);

      usart_printf_P(PSTR("ERROR reading at position: %lu\n"), size);
      goto end;
    }
  }

 end:
  usart_printf_P(PSTR("Done"));
  while(1) {}
}
