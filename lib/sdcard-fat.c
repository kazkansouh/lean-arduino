#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "sdcard-fat.h"

/* debugging statements are only included if debug flag is set */
#if defined(DEBUG)
  #include "usart_p.h"
  #define printf_P(fmt,...) usart_printf_P(PSTR("FAT> " fmt),__VA_ARGS__)
  #define print_P(fmt) printf_P(fmt, 0)
#else
  #define printf_P(...)
  #define print_P(...)
#endif

#define MAKE_UINT16(ptr,b1,b2)                  \
  ((uint16_t)((ptr)[b1])         |              \
   ((uint16_t)((ptr)[b2]) << 8))

#define MAKE_UINT32(ptr,b1,b2,b3,b4)            \
  ((uint32_t)((ptr)[b1])         |              \
   ((uint32_t)((ptr)[b2]) << 8)  |              \
   ((uint32_t)((ptr)[b3]) << 16) |              \
   ((uint32_t)((ptr)[b4]) << 24))

/*
  Simple conversion of a wide char (int16_t) into an uint8_t. Assumes
  the value passed in is a pointer to two uint8_t that represent
  int16_t.

  If the high byte is not 0x00, then set the char to '_', else use the
  value of the low byte. Note: this will set all the 0xFFFF in the
  UCS-2 stream to '_', but should not matter as after the 0x00 byte.
*/
#define WIDE_TO_CHAR(wchar_addr) \
  (*(wchar_addr + 1) == 0x00 ? *(wchar_addr) : '_')

/*
  Read fat32 boot sector and setup the p_sdfatcard structure.
*/
uint8_t fat32_init(SSDCard* const p_sdcard,
                   SSDFATCard* const p_sdfatcard) {
  uint8_t r = 0;
  uint8_t i = 0;

  /* save reference to card in fat structure */
  p_sdfatcard->p_sdcard = p_sdcard;

  /* read the first sector of the partition */
  r = sdcard_sector_read(p_sdcard, p_sdcard->ui_partition_first_sector);
  if (r != 0) {
    printf_P("Read Result: %02X\n", r);
    goto end;
  }

  /* check signature is valid */
  if (p_sdcard->pch_sector[0x1FE] != 0x55 ||
      p_sdcard->pch_sector[0x1FF] != 0xAA) {
    print_P("Invalid signature in boot sector\n");
    r = 0xFD;
    goto end;
  }

  /* check extended signature */
  if (p_sdcard->pch_sector[0x42] != 0x29) {
    print_P("Invalid extended boot signature\n");
    r = 0xFD;
    goto end;
  }

  /* check bytes per sector */
  if (p_sdcard->pch_sector[0x0B] != 0x00 ||
      p_sdcard->pch_sector[0x0C] != 0x02) {
    print_P("Bytes per sector != 512\n");
    r = 0xFC;
    goto end;
  }

  /* find sectors per cluster */
  p_sdfatcard->ui_sectors_per_cluster = p_sdcard->pch_sector[0x0D];
  printf_P("Sectors per cluster: %02X\n", p_sdfatcard->ui_sectors_per_cluster);

  /* find sectors per fat */
  p_sdfatcard->ui_fat_count = p_sdcard->pch_sector[0x10];
  printf_P("No of FAT: %02X\n", p_sdfatcard->ui_fat_count);

  /* find location of first fat (begining of fs) */
  p_sdfatcard->ui_fat_offset =
    MAKE_UINT16(p_sdcard->pch_sector, 0x0E, 0x0F);

  /* find total sectors in fs */
  p_sdfatcard->ui_fs_sectors
    = MAKE_UINT16(p_sdcard->pch_sector, 0x13, 0x14);
  if (p_sdfatcard->ui_fs_sectors == 0) {
    /* assume value is at 0x20 */
    p_sdfatcard->ui_fs_sectors
      = MAKE_UINT32(p_sdcard->pch_sector, 0x20, 0x21, 0x22, 0x23);
  }

  /* find total sectors in fat */
  p_sdfatcard->ui_fat_sectors
    = MAKE_UINT16(p_sdcard->pch_sector, 0x16, 0x17);
  if (p_sdfatcard->ui_fat_sectors == 0) {
    /* assume value is at 0x24 */
    p_sdfatcard->ui_fat_sectors
      = MAKE_UINT32(p_sdcard->pch_sector, 0x24, 0x25, 0x26, 0x27);
  }
  printf_P("Sectors in FAT: %lX\n", p_sdfatcard->ui_fat_sectors);

  /* find root directory cluster number */
  p_sdfatcard->ui_root_directory
    = MAKE_UINT32(p_sdcard->pch_sector, 0x2C, 0x2D, 0x2E, 0x2F);
  printf_P("Root directory cluster: %lX\n", p_sdfatcard->ui_root_directory);

  /* extract the volume id (assumes that extended boot signature is
     set to 0x29) */
  p_sdfatcard->ui_vol_id
    = MAKE_UINT32(p_sdcard->pch_sector, 0x43, 0x44, 0x45, 0x46);
  printf_P("VolID: %04X-%04X\n",
           (uint16_t)(p_sdfatcard->ui_vol_id >> 16),
           (uint16_t)(p_sdfatcard->ui_vol_id & 0xFFFF));

  /* extract the volume label (assumes that extended boot signature is
     set to 0x29) */
  for (i = 0; i < 11; i++) {
    p_sdfatcard->pch_vol_label[i] = p_sdcard->pch_sector[0x47 + i];
    p_sdfatcard->pch_vol_label[i+1] = 0;
  }
  printf_P("VolLabel: %s\n", p_sdfatcard->pch_vol_label);

  p_sdfatcard->ui_cluster_offset =
    p_sdfatcard->p_sdcard->ui_partition_first_sector +
    p_sdfatcard->ui_fat_offset +
    p_sdfatcard->ui_fat_sectors * p_sdfatcard->ui_fat_count;

 end:
  return r;
}

/*
  read a sector from a fat32 cluster, the caller is required to call
  spi 512 times to read the whole sector. during invocation, the CS is
  held high, if required it can be set low but its the callers
  responsibility to set it high again to read data.
*/
static
uint8_t fat32_cluster_read(SSDFATCard* p_sdfatcard,
                           uint32_t ui_cluster,
                           uint8_t ui_sector) {
  return sdcard_sector_read_begin(
    p_sdfatcard->p_sdcard,
    p_sdfatcard->ui_cluster_offset +
    p_sdfatcard->ui_sectors_per_cluster * (ui_cluster - 2) +
    ui_sector);
}

/*
  Lookup a cluster value in the fat.

  A value of 0xFFFFFFFF is an invalid cluster.
*/
static
uint32_t fat32_cluster_lookup(SSDFATCard* p_sdfatcard, uint32_t ui_cluster) {
  uint32_t ui_sector = ui_cluster / 128; /* 128 is the number of
                                            clusters in a sector */
  uint16_t ui_sector_offset = (ui_cluster % 128) * 4;
  uint8_t r;
  uint32_t ui_cluster_value;

  /* cluster 0 and 1 are special */
  if (ui_cluster < 2) {
    print_P("Cluster number is out of range (too small)\n");
    return 0xFFFFFFFF;
  }

  /* sector is valid by the fs */
  if (ui_sector > p_sdfatcard->ui_fat_sectors) {
    print_P("Cluster number is out of range (too large)\n");
    return 0xFFFFFFFF;
  }

  /* read the sector where the cluster is */
  r = sdcard_sector_read(p_sdfatcard->p_sdcard,
                         p_sdfatcard->p_sdcard->ui_partition_first_sector +
                         p_sdfatcard->ui_fat_offset +
                         ui_sector);
  if (r != 0) {
    print_P("Failed to read FAT\n");
    return 0xFFFFFFFF;
  }

  /* construct the value of the cluster in the fat */
  ui_cluster_value
    = MAKE_UINT32(p_sdfatcard->p_sdcard->pch_sector,
                  ui_sector_offset,
                  ui_sector_offset + 1,
                  ui_sector_offset + 2,
                  ui_sector_offset + 3);

  return ui_cluster_value;
}

/*
  Initialise a file system chain
*/
static
uint8_t fat32_chain_init(SSDFAT_Chain* const p_chain,
                         SSDFATCard* const p_sdfatcard,
                         const uint32_t ui_cluster) {
  uint8_t r = 0;

  /* setup the chain structure */
  p_chain->ui_sector = 0;
  p_chain->ui_cluster = ui_cluster;
  p_chain->ui_next_cluster = fat32_cluster_lookup(p_sdfatcard, ui_cluster);
  p_chain->p_sdfatcard = p_sdfatcard;

  /* if cluster was not used or invalid, abort */
  if (p_chain->ui_next_cluster == 0 ||
      p_chain->ui_next_cluster == 0xFFFFFFFF) {
    return 0xFB;
  }

  /* prepare to read identified sector */
  r = fat32_cluster_read(p_sdfatcard, ui_cluster, p_chain->ui_sector);
  if (r != 0) {
    return r;
  }

  return r;
}

/*
  Initialise a file system chain
*/
static
uint8_t fat32_chain_buffer(SSDFAT_Chain* const p_chain) {
  /* read whole sector and save into internal buffer */
  uint16_t i = 0;
  for (i = 0; i < 512; i++) {
     p_chain->p_sdfatcard->p_sdcard->pch_sector[i]
       = spi_master_transmit(0xFF);
  }

  /* end of sector reached, close spi */
  return sdcard_send_command_frame_data_end();
}

/*
  Read next sector in chain
*/
uint8_t fat32_chain_next(SSDFAT_Chain* p_chain) {
  uint8_t r;
  /* if reached end of cluster, calculate next cluster */
  if (++(p_chain->ui_sector) >= p_chain->p_sdfatcard->ui_sectors_per_cluster) {
    p_chain->ui_sector = 0;
    /* check if end of chain */
    if (p_chain->ui_next_cluster != 0x0FFFFFFF) {
      p_chain->ui_cluster = p_chain->ui_next_cluster;
      p_chain->ui_next_cluster
        = fat32_cluster_lookup(p_chain->p_sdfatcard, p_chain->ui_cluster);
      if (p_chain->ui_next_cluster == 0 ||
          p_chain->ui_next_cluster == 0xFFFFFFFF) {
        /* broken fat */
        print_P("Possibly broken FAT\n");
        return 0xFA;
      } else {
        printf_P("Cluster %lX has value %lX\n", p_chain->ui_cluster, p_chain->ui_next_cluster);
      }
    } else {
      /* end of chain */
      return 0xF9;
    }
  }

  /* prepare to read identified sector into memory */
  r = fat32_cluster_read(p_chain->p_sdfatcard,
                         p_chain->ui_cluster,
                         p_chain->ui_sector);
  return r;
}

/*
  Calculate the checksum byte for long file names
*/
static
uint8_t fat32_lfn_checksum(const uint8_t *pch_FCBName) {
   int i;
   uint8_t ui_sum = 0;

   for (i = 11; i; i--) {
      ui_sum = ((ui_sum & 1) << 7) + (ui_sum >> 1) + *pch_FCBName++;
   }

   return ui_sum;
}

#if defined(DEBUG)
/*
  Lists a directory on a fat32 file system. The cluster is the index
  of the directory file.
 */
uint8_t fat32_directory_list(SSDFATCard* p_sdfatcard, uint32_t ui_cluster) {
  uint8_t r;
  SSDFAT_Chain chain;
  uint16_t ui_entry;
  bool b_end_of_directory = false;
  uint8_t pch_longfn[260];
  uint16_t ui_lfn_idx = 0xFFFF;
  uint8_t ui_lfn_checksum = 0;
  uint8_t* const pch_sector = p_sdfatcard->p_sdcard->pch_sector;

  /* initilise a fat chain */
  r = fat32_chain_init(&chain, p_sdfatcard, ui_cluster);

  /* iterate over the sectors until end of directory (or end of
     cluster) is found */
  while (!b_end_of_directory && r == 0) {
    /* buffer the sectors data into ram */
    fat32_chain_buffer(&chain);

    for (ui_entry = 0; ui_entry < 512; ui_entry += 32) {
      /* check end of directory listing is not reached */
      if (pch_sector[ui_entry] == 0x00) {
        b_end_of_directory = true;
        break;
      }

      /* check if record in use */
      if (pch_sector[ui_entry] != 0xE5) {
        if (pch_sector[ui_entry] == 0x05) {
          pch_sector[ui_entry] = 0xE5;
        }
        if (pch_sector[ui_entry + 0x0B] == 0x0F) {
          print_P("VFAT entry\n");
          /* beginning of vfat chain */
          if (pch_sector[ui_entry] & 0x40) {
            ui_lfn_idx = sizeof(pch_longfn);
            ui_lfn_checksum = pch_sector[ui_entry + 0x0D];
          }
          /* check that checksum matches */
          if (pch_sector[ui_entry + 0x0D] != ui_lfn_checksum ||
              ui_lfn_idx < 13 ||
              ui_lfn_idx == 0xFFFF) {
            /* corrupt vfat entry */
            ui_lfn_idx = 0xFFFF;
          } else {
            /* copy 13 chars into the lfn buffer (only supporting ansi
               characters) */
            ui_lfn_idx -= 13;
            pch_longfn[ui_lfn_idx     ]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x01);
            pch_longfn[ui_lfn_idx +  1]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x03);
            pch_longfn[ui_lfn_idx +  2]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x05);
            pch_longfn[ui_lfn_idx +  3]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x07);
            pch_longfn[ui_lfn_idx +  4]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x09);

            pch_longfn[ui_lfn_idx +  5]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x0E);
            pch_longfn[ui_lfn_idx +  6]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x10);
            pch_longfn[ui_lfn_idx +  7]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x12);
            pch_longfn[ui_lfn_idx +  8]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x14);
            pch_longfn[ui_lfn_idx +  9]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x16);
            pch_longfn[ui_lfn_idx + 10]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x18);

            pch_longfn[ui_lfn_idx + 11]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x1C);
            pch_longfn[ui_lfn_idx + 12]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x1E);
          }
        } else {
          uint32_t ui_start_cluser
            = MAKE_UINT32(pch_sector,
                          ui_entry + 0x1A,
                          ui_entry + 0x1B,
                          ui_entry + 0x14,
                          ui_entry + 0x15);
          uint32_t ui_file_size
            = MAKE_UINT32(pch_sector,
                          ui_entry + 0x1C,
                          ui_entry + 0x1D,
                          ui_entry + 0x1E,
                          ui_entry + 0x1F);
          if (ui_lfn_idx != 0xFFFF &&
              fat32_lfn_checksum(pch_sector + ui_entry) == ui_lfn_checksum) {
            usart_printf_P(PSTR("%s - %04lu - %luB\n"),
                           pch_longfn + ui_lfn_idx,
                           ui_start_cluser,
                           ui_file_size);
          } else {
            usart_printf_P(PSTR("%c%c%c%c%c%c%c%c.%c%c%c - %04lu - %luB\n"),
                           pch_sector[ui_entry],
                           pch_sector[ui_entry + 1],
                           pch_sector[ui_entry + 2],
                           pch_sector[ui_entry + 3],
                           pch_sector[ui_entry + 4],
                           pch_sector[ui_entry + 5],
                           pch_sector[ui_entry + 6],
                           pch_sector[ui_entry + 7],
                           pch_sector[ui_entry + 8],
                           pch_sector[ui_entry + 9],
                           pch_sector[ui_entry + 10],
                           ui_start_cluser,
                           ui_file_size);
          }
          /* indicate lfn is now invalid (as dir entry has been processed) */
          ui_lfn_idx = 0xFFFF;
        }
      }
    }
    r = fat32_chain_next(&chain);
  }

  if (b_end_of_directory || r == 0xF9) {
    usart_printf_P(PSTR("End of listing\n"));
  }

  return r;
}
#endif

/*
  checks if a filename matches a raw (11 byte, space padded,
  uppercase) fat 8.3 file name.
*/
inline
bool is_8_3_equal(const char* pch_83_raw,
                  char* pch_filename,
                  size_t s_filename) {
  uint16_t i, j;
  char pch_name_raw[] = "           ";
  char *pch_ext = pch_name_raw + 8;
  bool b_in_name = true;

  /* copy the first segment of the name across */
  for (i = 0; i < s_filename && b_in_name && i < 9; i++) {
    if (pch_filename[i] == '.') {
      b_in_name = false;
    } else {
      pch_name_raw[i] = toupper(pch_filename[i]);
    }
  }

  /* invalid format */
  if (i == 0 || (i > 8 && b_in_name)) {
    return false;
  }

  /* copy the extension across */
  for (j = i; j < s_filename && j < i + 4; j++) {
    pch_ext[j - i] = toupper(pch_filename[j]);
  }

  /* unused chars at end of file name */
  if (j < s_filename) {
    return false;
  }

  /* compare the result */
  return strncmp(pch_83_raw, pch_name_raw, 11) == 0;
}

/*
  Locates a file identified by pch_path, what is returned is a
  reference to the directory reference. I.e. a sector and offset.

  Assumes a unix style path, i.e. /path/to/file.ext
 */
uint8_t fat32_file_locate(SSDFATCard* const p_sdfatcard,
                          const char* pch_path,
                          uint32_t* const ui_file_cluster,
                          uint32_t* const ui_file_size) {
  uint8_t r;
  SSDFAT_Chain chain;
  uint16_t ui_entry;
  bool b_end_of_directory = false;
  uint8_t pch_longfn[260];
  uint16_t ui_lfn_idx = 0xFFFF;
  uint8_t ui_lfn_checksum = 0;
  /* start in the root directory */
  uint32_t ui_cluster = p_sdfatcard->ui_root_directory;
  const char *pch_path_segment_end = pch_path;
  bool b_is_file;
  uint8_t* const pch_sector = p_sdfatcard->p_sdcard->pch_sector;

  /* empty string */
  if (pch_path[0] != '/') {
    print_P("Path string is invalid\n");
    return 0xF1;
  }

  /* iterate over path segments */
 nextsegment:
  /* move to next segment */
  pch_path = pch_path_segment_end + 1;
  pch_path_segment_end = strchrnul(pch_path,'/');
  if (pch_path_segment_end == pch_path) {
    /* badly formed file path */
    print_P("Path string is invalid\n");
    return 0xF1;
  }
  /* is file when in last segment of path */
  b_is_file = *pch_path_segment_end == '\0';

  /* initilise a fat chain */
  r = fat32_chain_init(&chain, p_sdfatcard, ui_cluster);

  /* iterate over the sectors until end of directory (or end of
     cluster) is found */
  while (!b_end_of_directory && r == 0) {
    /* buffer sector into internal memory */
    fat32_chain_buffer(&chain);

    for (ui_entry = 0; ui_entry < 512; ui_entry += 32) {
      /* check end of directory listing is not reached */
      if (pch_sector[ui_entry] == 0x00) {
        b_end_of_directory = true;
        break;
      }

      /* check if record in use */
      if (pch_sector[ui_entry] != 0xE5) {
        if (pch_sector[ui_entry] == 0x05) {
          pch_sector[ui_entry] = 0xE5;
        }
        if (pch_sector[ui_entry + 0x0B] == 0x0F) {
          /* beginning of vfat chain */
          if (pch_sector[ui_entry] & 0x40) {
            ui_lfn_idx = sizeof(pch_longfn);
            ui_lfn_checksum = pch_sector[ui_entry + 0x0D];
          }
          /* check that checksum matches */
          if (pch_sector[ui_entry + 0x0D] != ui_lfn_checksum ||
              ui_lfn_idx < 13 ||
              ui_lfn_idx == 0xFFFF) {
            /* corrupt vfat entry */
            ui_lfn_idx = 0xFFFF;
          } else {
            /* copy 13 chars into the lfn buffer (only supporting ansi
               characters) */
            ui_lfn_idx -= 13;
            pch_longfn[ui_lfn_idx     ]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x01);
            pch_longfn[ui_lfn_idx +  1]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x03);
            pch_longfn[ui_lfn_idx +  2]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x05);
            pch_longfn[ui_lfn_idx +  3]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x07);
            pch_longfn[ui_lfn_idx +  4]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x09);

            pch_longfn[ui_lfn_idx +  5]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x0E);
            pch_longfn[ui_lfn_idx +  6]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x10);
            pch_longfn[ui_lfn_idx +  7]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x12);
            pch_longfn[ui_lfn_idx +  8]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x14);
            pch_longfn[ui_lfn_idx +  9]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x16);
            pch_longfn[ui_lfn_idx + 10]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x18);

            pch_longfn[ui_lfn_idx + 11]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x1C);
            pch_longfn[ui_lfn_idx + 12]
              = WIDE_TO_CHAR(pch_sector + ui_entry + 0x1E);
          }
        } else {
          *ui_file_cluster
            = MAKE_UINT32(pch_sector,
                          ui_entry + 0x1A,
                          ui_entry + 0x1B,
                          ui_entry + 0x14,
                          ui_entry + 0x15);
          *ui_file_size
           = MAKE_UINT32(pch_sector,
                          ui_entry + 0x1C,
                          ui_entry + 0x1D,
                          ui_entry + 0x1E,
                          ui_entry + 0x1F);
          /* first check vfat (if available) */
          if (ui_lfn_idx != 0xFFFF &&
              fat32_lfn_checksum(pch_sector + ui_entry) == ui_lfn_checksum) {
            if (strncmp((char*)(pch_longfn + ui_lfn_idx),
                        pch_path,
                        pch_path_segment_end - pch_path) == 0) {
              /* recurse on subdirectory */
              if (pch_sector[ui_entry + 0x0B] & 0x10 && !b_is_file) {
                ui_cluster = *ui_file_cluster;
                goto nextsegment;
              } else if (!(pch_sector[ui_entry + 0x0B] & 0x10) && b_is_file) {
                /*
                   file found

                   TODO: return a reference to the directory entry for
                   write capability
                 */
                return 0;
              }
              /* file not found (file/directory does not agree) */
              return 0xF0;
            }
          }
          /* if no match was successful in vfat, try the 8.3 name */
          if (is_8_3_equal((char*)(pch_sector + ui_entry),
                           (char*)(pch_path),
                           (uintptr_t)(pch_path_segment_end - pch_path))) {
            /* recurse on subdirectory */
            if (pch_sector[ui_entry + 0x0B] & 0x10 && !b_is_file) {
              ui_cluster = *ui_file_cluster;
              goto nextsegment;
            } else if (!(pch_sector[ui_entry + 0x0B] & 0x10) && b_is_file) {
              /*
                 file found

                 TODO: return a reference to the directory entry for
                 write capability
              */
              return 0;
            }
            /* file not found (file/directory does not agree) */
            return 0xF0;
          }

          /* indicate lfn is now invalid (as dir entry has been processed) */
          ui_lfn_idx = 0xFFFF;
        }
      }
    }
    r = fat32_chain_next(&chain);
  }

  /* if iterated over directory and not found file, abort */
  if (b_end_of_directory || r == 0xF9) {
    print_P("File not found\n");
    return 0xF0;
  }

  return r;
}

/*
  Opens a file identified by pch_path, and populates p_sdfile with
  needed data.

  Assumes a unix style path, i.e. /path/to/file.ext
 */
uint8_t fat32_file_open(SSDFATCard* const p_sdfatcard,
                        SSDFAT_File* const p_sdfile,
                        const char* pch_path) {
  uint8_t r;
  uint32_t ui_cluster;

  p_sdfile->pch_filename = pch_path;
  p_sdfile->ui_position = 0;

  r = fat32_file_locate(p_sdfatcard,
                        pch_path,
                        &ui_cluster,
                        &(p_sdfile->ui_file_size));
  if (r != 0) {
    print_P("File not found\n");
    return r;
  }

  r = fat32_chain_init(&(p_sdfile->s_chain),
                       p_sdfatcard,
                       ui_cluster);
  if (r != 0) {
    print_P("Could not init cluster\n");
  }

  return r;
}
