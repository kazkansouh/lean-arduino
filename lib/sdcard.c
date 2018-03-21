#include <stdint.h>
#include <stddef.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#include "sdcard.h"

#include "pins.h"
#include "spi.h"
#include "timer.h"

#if !defined(CHIP_SELECT)
  #error "CHIP_SELECT must be defined as the pin connected to SDCard CS"
#endif

#if !defined(TIMEOUT_MS)
  #define TIMEOUT_MS 1000
#endif

/* debugging statements are only included if debug flag is set */
#if defined(DEBUG)
  #include "usart_p.h"
  #define printf_P(fmt,...) usart_printf_P(PSTR("SD> " fmt),__VA_ARGS__)
  #define print_P(fmt) printf_P(fmt,0)
#else
  #define printf_P(...)
  #define print_P(...)
#endif

/* define the SPI pins */
#define SCK       13
#define MISO      12
#define MOSI      11

/*
  Extracts (maximum 32) bits from a sequence of bytes.
 */
uint32_t sdcard_extract_bits(const uint8_t* const pch_buff,
                             const uint8_t start,
                             const uint8_t width) {
  uint32_t result = 0;
  uint8_t i;
  for (i = 0; i < width; i++) {
    result <<= 1;
    result |= (pch_buff[(start + i) / 8] >> (7 - (start + i) % 8)) & 0x1;
  }
  return result;
}

/*
  Send a command frame over SPI to the SD card, if timeout occours the
  msb will be set.
*/
static
uint8_t sdcard_send_command_frame(const uint8_t cmd,
                                  const uint8_t arg1,
                                  const uint8_t arg2,
                                  const uint8_t arg3,
                                  const uint8_t arg4,
                                  const uint8_t crc) {
  uint32_t timeout = timer_millis() + TIMEOUT_MS;
  uint8_t r = 0;

  /* drive cs low to card will receive command */
  writePin(CHIP_SELECT,false);

  /* send command frame */
  spi_master_transmit(cmd);
  spi_master_transmit(arg1);
  spi_master_transmit(arg2);
  spi_master_transmit(arg3);
  spi_master_transmit(arg4);
  spi_master_transmit(crc);

  /* wait for result */
  while ((r = spi_master_transmit(0xFF)) & 0x80 &&
         timer_millis() < timeout) {}

  /* drive cs high */
  writePin(CHIP_SELECT,true);

  return r;
}

/*
  Send a command frame over SPI to the SD card, if timeout occours the
  msb will be set.

  Expects the result to be in R3 format. I.e. an R1 followed by 4
  bytes that are saved into r3 in little endian format.
*/
static
uint8_t sdcard_send_command_frame_r3(const uint8_t cmd,
                                     const uint8_t arg1,
                                     const uint8_t arg2,
                                     const uint8_t arg3,
                                     const uint8_t arg4,
                                     const uint8_t crc,
                                     uint32_t* const r3) {
  uint32_t timeout = timer_millis() + TIMEOUT_MS;
  uint8_t r = 0;

  /* drive cs low to card will receive command */
  writePin(CHIP_SELECT,false);

  /* send command frame */
  spi_master_transmit(cmd);
  spi_master_transmit(arg1);
  spi_master_transmit(arg2);
  spi_master_transmit(arg3);
  spi_master_transmit(arg4);
  spi_master_transmit(crc);

  /* wait for result */
  while ((r = spi_master_transmit(0xFF)) & 0x80 &&
         timer_millis() < timeout) {}

  /* read 4 bytes that make the r3 format */
  *r3
    = ((uint32_t)(spi_master_transmit(0xFF)) << 24)
    | ((uint32_t)(spi_master_transmit(0xFF)) << 16)
    | ((uint32_t)(spi_master_transmit(0xFF)) << 8)
    | (uint32_t)(spi_master_transmit(0xFF));

  /* drive cs high */
  writePin(CHIP_SELECT,true);

  return r;
}

/*
  Send a command frame over SPI to the SD card, if timeout occours the
  msb will be set.

  The data will be placed into the buffer, and will read buffer_len
  bytes.
*/
static
uint8_t sdcard_send_command_frame_data(const uint8_t cmd,
                                       const uint8_t arg1,
                                       const uint8_t arg2,
                                       const uint8_t arg3,
                                       const uint8_t arg4,
                                       const uint8_t crc,
                                       uint8_t* const buffer,
                                       const size_t buffer_len) {
  uint32_t timeout = timer_millis() + TIMEOUT_MS;
  uint8_t r = 0;
  size_t i;

  /* drive cs low to card will receive command */
  writePin(CHIP_SELECT,false);

  /* send command frame */
  spi_master_transmit(cmd);
  spi_master_transmit(arg1);
  spi_master_transmit(arg2);
  spi_master_transmit(arg3);
  spi_master_transmit(arg4);
  spi_master_transmit(crc);

  /* wait for result */
  while ((r = spi_master_transmit(0xFF)) & 0x80 &&
         timer_millis() < timeout) {}

  if (r != 0) {
    /* drive cs high */
    writePin(CHIP_SELECT,true);
    return r;
  }

  /* wait for result token */
  timeout = timer_millis() + TIMEOUT_MS;
  while (spi_master_transmit(0xFF) != 0xFE &&
         timer_millis() < timeout) {}
  if (timer_millis() >= timeout) {
    /* drive cs high */
    writePin(CHIP_SELECT,true);
    return 0xFF;
  }

  /* read data */
  for (i = 0; i < buffer_len; i++) {
    buffer[i] = spi_master_transmit(0xFF);
  }

  /* read two crc bytes */
  spi_master_transmit(0xFF);
  spi_master_transmit(0xFF);

  /* drive cs high */
  writePin(CHIP_SELECT,true);

  return r;
}

/*
  Begins a non-buffered read. Send a command frame over SPI to the SD
  card, if timeout occours the msb will be set.

  The caller is expected to directly read the bytes from the spi bus,
  e.g.

    /\* read data *\/
    size_t i;
    for (i = 0; i < buffer_len; i++) {
      buffer[i] = spi_master_transmit(0xFF);
    }

  After notify the _end varient of this function after required
  number of bytes is returned.
*/
static
uint8_t sdcard_send_command_frame_data_begin(const uint8_t cmd,
                                             const uint8_t arg1,
                                             const uint8_t arg2,
                                             const uint8_t arg3,
                                             const uint8_t arg4,
                                             const uint8_t crc) {
  uint32_t timeout = timer_millis() + TIMEOUT_MS;
  uint8_t r = 0;

  /* drive cs low to card will receive command */
  writePin(CHIP_SELECT,false);

  /* send command frame */
  spi_master_transmit(cmd);
  spi_master_transmit(arg1);
  spi_master_transmit(arg2);
  spi_master_transmit(arg3);
  spi_master_transmit(arg4);
  spi_master_transmit(crc);

  /* wait for result */
  while ((r = spi_master_transmit(0xFF)) & 0x80 &&
         timer_millis() < timeout) {}

  if (r != 0) {
    /* drive cs high */
    writePin(CHIP_SELECT,true);
    return r;
  }

  /* wait for result token */
  timeout = timer_millis() + TIMEOUT_MS;
  while (spi_master_transmit(0xFF) != 0xFE &&
         timer_millis() < timeout) {}
  if (timer_millis() >= timeout) {
    /* drive cs high */
    writePin(CHIP_SELECT,true);
    return 0xFF;
  }

  return r;
}

/*
  Cleanup after begin call, should only be called once the expected
  number of bytes (without crc) is read from the spi bus.
*/
uint8_t sdcard_send_command_frame_data_end() {
  /* read two crc bytes */
  spi_master_transmit(0xFF);
  spi_master_transmit(0xFF);

  /* drive cs high */
  writePin(CHIP_SELECT,true);

  return 0x00;
}

/*
  Reads the identified sector from the SC Card.

  The sector is placed into the SDCard buffer, and the associated
  sector is saved into the sdcard stucture.
*/
uint8_t sdcard_sector_read(SSDCard* const p_sdcard,
                           const uint32_t ui_sector) {
  uint8_t r;

  /* sector is already in memeory */
  if (ui_sector == p_sdcard->ui_sector) {
    return 0;
  }

  /* requested sector is larger than max */
  if (ui_sector >= p_sdcard->ui_sectors) {
    printf_P("Sector 0x%lX is larger than size supported by card: 0x%lX\n",
             ui_sector,
             p_sdcard->ui_sector);

    return 0xFE;
  }

  /* read sector (512 bytes) and place in buffer */
  r = sdcard_send_command_frame_data(0x51,
                                     ui_sector >> 24 & 0xFF,
                                     ui_sector >> 16 & 0xFF,
                                     ui_sector >> 8 & 0xFF,
                                     ui_sector & 0xFF,
                                     0xFF,
                                     p_sdcard->pch_sector,
                                     512);

  /* updated sector index in memory */
  if (r == 0) {
    p_sdcard->ui_sector = ui_sector;
  }

  return r;
}

/*
  Reads the identified sector from the SC Card without using internal
  buffer.

  The sector is placed into the SDCard buffer, and the associated
  sector is written.
*/
uint8_t sdcard_sector_read_begin(SSDCard* const p_sdcard,
                                 const uint32_t ui_sector) {
  uint8_t r;

  /* sector is already in memeory */
  if (ui_sector == p_sdcard->ui_sector) {
    return 0;
  }

  /* requested sector is larger than max */
  if (ui_sector >= p_sdcard->ui_sectors) {
    printf_P("Sector 0x%lX is larger than size supported by card: 0x%lX\n",
             ui_sector,
             p_sdcard->ui_sector);

    return 0xFE;
  }

  /* read sector (512 bytes) and place in buffer */
  r = sdcard_send_command_frame_data_begin(0x51,
                                           ui_sector >> 24 & 0xFF,
                                           ui_sector >> 16 & 0xFF,
                                           ui_sector >> 8 & 0xFF,
                                           ui_sector & 0xFF,
                                           0xFF);

  /* updated sector index in memory */
  if (r == 0) {
    p_sdcard->ui_sector = ui_sector;
  }

  return r;
}

/*
  Initilise SPI and communicate with the sdcard to read basic
  information.
 */
uint8_t sdcard_init(SSDCard* const p_sdcard) {
  uint8_t r;
  uint16_t i = 0;

  /* small delay for initial poweron to let sdcard settle */
  _delay_ms(250);

  /* setup pin as output */
  writePin(CHIP_SELECT,true);
  setMode(CHIP_SELECT, output);

  /* configure spi into master mode */
  spi_master_init();

  /* send < 80 clock pulses to sdcard, 512 bytes chosen to flush any
     operation if a software reset occours during an SPI operation to
     card */
  for(i = 0; i < 512; i++) {
    spi_master_transmit(0xFF);
  }

  /* wait for card to become ready
     TODO: add timeout
   */
  while (readPin(MISO) == 0) {}

  /* send software reset to card and enter SPI mode */
  r = sdcard_send_command_frame(0x40, 0x00, 0x00, 0x00, 0x00, 0x95);
  printf_P("Reset result: %02X\n", r);
  if (r != 1) {
    r = 1;
    goto end;
  }

  /* wait for card to become ready */
  while (sdcard_send_command_frame(0x41, 0x00, 0x00, 0x00, 0x00, 0xFF) != 0) {}

  /* read the CSD register */
  r = sdcard_send_command_frame_data(0x49, 0x00, 0x00, 0x00, 0x00, 0xFF,
                              p_sdcard->pch_csd, sizeof(p_sdcard->pch_csd));

  if (r != 0) {
    printf_P("CSD Result: %02X\n", r);
    goto end;
  }

  /* print data */
  print_P("CSD: ");
  for (i = 0; i < sizeof(p_sdcard->pch_csd); i++) {
    printf_P("%02X ", p_sdcard->pch_csd[i]);
  }
  print_P("\n");

  /* identify the csd version */
  p_sdcard->ui_csd_version = sdcard_extract_bits(p_sdcard->pch_csd, 0, 2);
  printf_P("CSD_STRUCTURE: %X\n", p_sdcard->ui_csd_version);
  if (p_sdcard->ui_csd_version != 1) {
    print_P("Structure version not supported\n");
    goto end;
  }

  /* exctract the C_Size, that when csd version == 1 is the number of
     sectors - 1 */
  p_sdcard->ui_sectors = sdcard_extract_bits(p_sdcard->pch_csd, 58, 22);
  printf_P("C_Size: %X\n", p_sdcard->ui_sectors);
  p_sdcard->ui_sectors = (p_sdcard->ui_sectors + 1) * 1024;

  /* mark ui_sector as invalid */
  p_sdcard->ui_sector = 0xFFFFFFFF;

  /* read the CID register */
  r = sdcard_send_command_frame_data(0x4A, 0x00, 0x00, 0x00, 0x00, 0xFF,
                                     p_sdcard->pch_cid,
                                     sizeof(p_sdcard->pch_cid));

  if (r != 0) {
    printf_P("CID Result: %02X\n", r);
    goto end;
  }

  /* print data */
  print_P("CID: ");
  for (i = 0; i < sizeof(p_sdcard->pch_cid); i++) {
    printf_P("%02X ", p_sdcard->pch_cid[i]);
  }
  print_P("\n");

  /* read the OCR register */
  r = sdcard_send_command_frame_r3(0x7A, 0x00, 0x00, 0x00, 0x00, 0xFF,
                                   &(p_sdcard->ui_ocr));
  if (r != 0) {
    printf_P("OCR Result: %02X\n", r);
    goto end;
  }
  /* print data */
  printf_P("OCR: %08lX\n", p_sdcard->ui_ocr);

  /* load the file system: read MBR, search for fat32 partition
     with LBA and read the boot sector of the partition */
  r = sdcard_mbr_read(p_sdcard);
  if (r != 0) {
    printf_P("MBR read Result: %02X\n", r);
    goto end;
  }

 end:
  print_P("SDCard init finished\n");
  return r;
}

/*
  Read the MBR of the record and search for a FAT32 partition.
 */
uint8_t sdcard_mbr_read(SSDCard* const p_sdcard) {
  uint16_t i = 0;

  /* read mbr (first 512 bytes) */
  uint8_t r = sdcard_sector_read(p_sdcard, 0);
  if (r != 0) {
    print_P("Failed to read MBR\n");
    goto end;
  }

  /* check signature is valid */
  if (p_sdcard->pch_sector[0x1FE] != 0x55 ||
      p_sdcard->pch_sector[0x1FF] != 0xAA) {
    print_P("Invalid signature in MBR\n");
    r = 0xFD;
    goto end;
  }

  /* set error code for no valid partitons */
  r = 0xFC;

  /* iterate over partition */
  for (i = 0x1BE; i < 0x1FE; i += 0x10) {
    /* check that partition is active */
    if (p_sdcard->pch_sector[i] & 0x80) {
      /* check partition type is set as 0x0C (FAT32 with LBA) */
      if (p_sdcard->pch_sector[i + 4] & 0x0C) {
        /* extract out the first sector and number of sectors from the
           record*/
        p_sdcard->ui_partition_first_sector
          = (uint32_t)(p_sdcard->pch_sector[i + 8])
          | ((uint32_t)(p_sdcard->pch_sector[i + 9]) << 8)
          | ((uint32_t)(p_sdcard->pch_sector[i + 10]) << 16)
          | ((uint32_t)(p_sdcard->pch_sector[i + 11]) << 24);
        p_sdcard->ui_partition_sectors
          = (uint32_t)(p_sdcard->pch_sector[i + 12])
          | ((uint32_t)(p_sdcard->pch_sector[i + 13]) << 8)
          | ((uint32_t)(p_sdcard->pch_sector[i + 14]) << 16)
          | ((uint32_t)(p_sdcard->pch_sector[i + 15]) << 24);
        printf_P("Found FAT32 (with LBA) of size %lu MiB\n",
                 p_sdcard->ui_partition_sectors / 0x800);
        r = 0;
        break;
      } else {
        printf_P("Unsuported partition type found %02X,"
                 " looking for (FAT32 with LBA)\n",
                 p_sdcard->pch_sector[i + 4]);
      }
    }
  }
 end:
  return r;
}
