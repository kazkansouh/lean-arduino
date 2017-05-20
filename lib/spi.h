#ifndef _SPI_H
#define _SPI_H

#include <stdint.h>

void spi_master_init(void);
void spi_master_transmit(uint8_t i_data);
void spi_master_transmit_16(uint16_t i_data);
void spi_master_transmit_32(const uint32_t* const pi_data);

#endif
