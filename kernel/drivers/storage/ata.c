#include "ata.h"

#include "io.h"

#define ATA_DATA 0x1F0
#define ATA_SECTOR_COUNT 0x1F2
#define ATA_LBA_LOW 0x1F3
#define ATA_LBA_MID 0x1F4
#define ATA_LBA_HIGH 0x1F5
#define ATA_DRIVE 0x1F6
#define ATA_COMMAND 0x1F7
#define ATA_STATUS 0x1F7

#define ATA_CMD_READ_SECTORS 0x20
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_STATUS_BSY 0x80
#define ATA_STATUS_DRQ 0x08
#define ATA_STATUS_ERR 0x01

static uint16_t inw(uint16_t port)
{
    uint16_t value;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void outw(uint16_t port, uint16_t value)
{
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static bool wait_for_drq(void)
{
    for (uint32_t i = 0; i < 100000; i++) {
        uint8_t status = inb(ATA_STATUS);
        if ((status & ATA_STATUS_ERR) != 0) {
            return false;
        }
        if ((status & ATA_STATUS_BSY) == 0 && (status & ATA_STATUS_DRQ) != 0) {
            return true;
        }
    }

    return false;
}

bool ata_read_sector(uint32_t lba, void *buffer)
{
    uint16_t *out = (uint16_t *) buffer;

    outb(ATA_DRIVE, (uint8_t) (0xE0 | ((lba >> 24) & 0x0F)));
    outb(ATA_SECTOR_COUNT, 1);
    outb(ATA_LBA_LOW, (uint8_t) (lba & 0xFF));
    outb(ATA_LBA_MID, (uint8_t) ((lba >> 8) & 0xFF));
    outb(ATA_LBA_HIGH, (uint8_t) ((lba >> 16) & 0xFF));
    outb(ATA_COMMAND, ATA_CMD_READ_SECTORS);

    if (!wait_for_drq()) {
        return false;
    }

    for (uint32_t word = 0; word < 256; word++) {
        out[word] = inw(ATA_DATA);
    }
    return true;
}

bool ata_write_sector(uint32_t lba, const void *buffer)
{
    const uint16_t *in = (const uint16_t *) buffer;

    outb(ATA_DRIVE, (uint8_t) (0xE0 | ((lba >> 24) & 0x0F)));
    outb(ATA_SECTOR_COUNT, 1);
    outb(ATA_LBA_LOW, (uint8_t) (lba & 0xFF));
    outb(ATA_LBA_MID, (uint8_t) ((lba >> 8) & 0xFF));
    outb(ATA_LBA_HIGH, (uint8_t) ((lba >> 16) & 0xFF));
    outb(ATA_COMMAND, ATA_CMD_WRITE_SECTORS);

    if (!wait_for_drq()) {
        return false;
    }

    for (uint32_t word = 0; word < 256; word++) {
        outw(ATA_DATA, in[word]);
    }
    outb(ATA_COMMAND, ATA_CMD_CACHE_FLUSH);

    for (uint32_t i = 0; i < 100000; i++) {
        uint8_t status = inb(ATA_STATUS);
        if ((status & ATA_STATUS_ERR) != 0) {
            return false;
        }
        if ((status & ATA_STATUS_BSY) == 0) {
            return true;
        }
    }

    return false;
}
