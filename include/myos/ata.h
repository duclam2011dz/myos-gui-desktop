#ifndef MYOS_ATA_H
#define MYOS_ATA_H

#include <stdbool.h>
#include <stdint.h>

bool ata_read_sector(uint32_t lba, void *buffer);
bool ata_write_sector(uint32_t lba, const void *buffer);

#endif
