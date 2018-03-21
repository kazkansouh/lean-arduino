#ifndef _SDCARD_H
#define _SDCARD_H

#include <stdint.h>

typedef struct  {
  uint8_t pch_csd[16];
  uint8_t pch_cid[16];
  uint32_t ui_ocr;
  uint8_t ui_csd_version __attribute__ ((aligned (2)));
  uint32_t ui_sectors;
  uint32_t ui_sector;
  uint8_t pch_sector[512];
  uint32_t ui_partition_first_sector;
  uint32_t ui_partition_sectors;
} SSDCard;

/*
  reads information registers and the mbr of the sdcard, assumes card
  is connected via spi.

  uses the default spi speed configured in the spi.c file.
*/
uint8_t sdcard_init(SSDCard* const p_sdcard);

/*
  reads the mbr segment and scans the parition table. currently hard
  coded to find the first fat32 lba partition.
 */
uint8_t sdcard_mbr_read(SSDCard* const p_sdcard);

/*
  generic read, the result is buffered into the pch_sector field of
  the SSDCard structure.
*/
uint8_t sdcard_sector_read(SSDCard* const p_sdcard, const uint32_t ui_sector);

/*
  generic read, no buffering is provided.
*/
uint8_t sdcard_sector_read_begin(SSDCard* const p_sdcard, const uint32_t ui_sector);

/*
  Cleanup after begin call, should only be called once the expected
  number of bytes (without crc) is read from the spi bus.
*/
uint8_t sdcard_send_command_frame_data_end();

/*
  utility function for extracting bits from a sequence of bytes. used
  for extracting data from the CSD and CID registers.
*/
uint32_t sdcard_extract_bits(const uint8_t* const pch_buff,
                             const uint8_t start,
                             const uint8_t width);
#endif
