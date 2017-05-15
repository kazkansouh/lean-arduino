#include <avr/io.h>

void spi_master_init(void) {
  /* Set MOSI and SCK output, all others input */
  /* DDR_SPI = (1<<DD_MOSI)|(1<<DD_SCK); */
  DDRB |= (1<<DDB3)|(1<<DDB5);
  /* Enable SPI, Master, set clock rate fck/16 */
  SPCR = (1<<SPE)|(1<<MSTR)|(1<<SPR0);
}

void spi_master_transmit(char cData) {
  /* Start transmission */
  SPDR = cData;
  /* Wait for transmission complete */
  while(!(SPSR & (1<<SPIF)))
    ;
}
