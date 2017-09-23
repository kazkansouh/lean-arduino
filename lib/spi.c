#include <avr/io.h>

void spi_master_init(void) {
  /* Set MOSI and SCK output, all others input */
  /* DDR_SPI = (1<<DD_MOSI)|(1<<DD_SCK); */
  DDRB |= (1<<DDB3)|(1<<DDB5);
  /* Enable SPI, Master, set clock rate fck/16 */
  SPCR = (1<<SPE)|(1<<MSTR)|(1<<SPR0);
}

void spi_master_transmit_16(uint16_t i_data) {
  /* Transmit high byte */
  SPDR = i_data >> 8;
  /* Wait for transmission complete */
  while(!(SPSR & (1<<SPIF)))
    ;
  /* Transmit low byte */
  SPDR = i_data & 0xFF;
  /* Wait for transmission complete */
  while(!(SPSR & (1<<SPIF)))
    ;
}

void spi_master_transmit_32(const uint32_t* const pi_data) {
  /* Transmit high byte */
  SPDR = ((const uint8_t* const)pi_data)[3];
  /* Wait for transmission complete */
  while(!(SPSR & (1<<SPIF)))
    ;
  /* Transmit next byte */
  SPDR = ((const uint8_t* const)pi_data)[2];
  /* Wait for transmission complete */
  while(!(SPSR & (1<<SPIF)))
    ;
  /* Transmit next byte */
  SPDR = ((const uint8_t* const)pi_data)[1];
  /* Wait for transmission complete */
  while(!(SPSR & (1<<SPIF)))
    ;
  /* Transmit low byte */
  SPDR = ((const uint8_t* const)pi_data)[0];
  /* Wait for transmission complete */
  while(!(SPSR & (1<<SPIF)))
    ;
}
