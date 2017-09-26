#ifndef _SPI_H
#define _SPI_H

#include <stdint.h>
#include <avr/io.h>

void spi_master_init(void);
void spi_master_transmit_16(uint16_t i_data);
void spi_master_transmit_32(const uint32_t* const pi_data);

inline
uint8_t spi_master_transmit(uint8_t i_data) {
  /* Start transmission */
  SPDR = i_data;
  /* Wait for transmission complete */
  while(!(SPSR & (1<<SPIF)))
    ;
  return SPDR;
}

#endif
