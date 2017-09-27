#ifndef _SDCARD_FAT_H
#define _SDCARD_FAT_H

#include <stdint.h>
#include "sdcard.h"

typedef struct {
  SSDCard *p_sdcard;

  /* number of sectors in a cluster */
  uint8_t ui_sectors_per_cluster __attribute__ ((aligned (2)));

  /* offset from start of partition that first fat is located (in
     sectors) */
  uint16_t ui_fat_offset;

  /* sectors in a fat */
  uint32_t ui_fat_sectors;

  /* total sectors in the fs (as defined by the boot sector) */
  uint32_t ui_fs_sectors;

  /* number of fats in the fs, normally 2*/
  uint8_t ui_fat_count __attribute__ ((aligned (2)));

  /* cluster of the root directory */
  uint32_t ui_root_directory;

  /* fs identifiers */
  uint32_t ui_vol_id;
  uint8_t pch_vol_label[12];
} SSDFATCard;

typedef struct {
  SSDFATCard* p_sdfatcard;

  /* current cluster */
  uint32_t ui_cluster;

  /* value of current cluster, i.e. result of looking up the cluster
     in the fat (could be the end of chain marker) */
  uint32_t ui_next_cluster;

  /* sector within the current cluster */
  uint8_t ui_sector __attribute__ ((aligned (2)));
} SSDFAT_Chain;

typedef struct {
  /* stores a reference to the file name (no memory allocation is
     provided) */
  const char* pch_filename;
  
  /* the underlying chain that represents the file */
  SSDFAT_Chain s_chain;

  /* current position in the file byte stream */
  uint32_t ui_position;

  /* total file size */
  uint32_t ui_file_size;
} SSDFAT_File;

/*
  reads the boot sector of the partition. assumes that p_sdcard has
  been initilised correctly with sdcard.
*/
uint8_t fat32_init(SSDCard* const p_sdcard,
                   SSDFATCard* const p_sdfatcard);

/*
  Opens a file identified by pch_path, and populates p_sdfile with
  needed data.

  Assumes a unix style path, i.e. /path/to/file.ext

  There is no need for a close as this is read only and all state is
  stored in the p_sdfile structure.
*/
uint8_t fat32_file_open(SSDFATCard* const p_sdfatcard,
                        SSDFAT_File* const p_sdfile,
                        const char* pch_path);

/*
  reads a byte from the file, returns -1 on eof
*/
int16_t fat32_file_read_byte(SSDFAT_File* const p_sdfile);

#endif
