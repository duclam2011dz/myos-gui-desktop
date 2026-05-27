#include "pci.h"

#include "io.h"
#include "serial.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC
#define PCI_MAX_DEVICES 32

struct pci_device {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint8_t class_code;
    uint8_t subclass;
    uint16_t vendor;
    uint16_t device;
};

static struct pci_device devices[PCI_MAX_DEVICES];
static uint32_t device_count;

static uint32_t inl(uint16_t port)
{
    uint32_t value;
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void outl(uint16_t port, uint32_t value)
{
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset)
{
    uint32_t address = 0x80000000U | ((uint32_t) bus << 16) | ((uint32_t) slot << 11) |
                       ((uint32_t) function << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static void add_device(uint8_t bus, uint8_t slot, uint8_t function)
{
    if (device_count >= PCI_MAX_DEVICES) {
        return;
    }
    uint32_t id = pci_read32(bus, slot, function, 0x00);
    if ((id & 0xFFFF) == 0xFFFF) {
        return;
    }
    uint32_t class_reg = pci_read32(bus, slot, function, 0x08);
    devices[device_count].bus = bus;
    devices[device_count].slot = slot;
    devices[device_count].function = function;
    devices[device_count].vendor = (uint16_t) (id & 0xFFFF);
    devices[device_count].device = (uint16_t) ((id >> 16) & 0xFFFF);
    devices[device_count].class_code = (uint8_t) (class_reg >> 24);
    devices[device_count].subclass = (uint8_t) (class_reg >> 16);
    device_count++;
}

void pci_initialize(void)
{
    device_count = 0;
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t id = pci_read32((uint8_t) bus, slot, 0, 0x00);
            if ((id & 0xFFFF) == 0xFFFF) {
                continue;
            }
            add_device((uint8_t) bus, slot, 0);
            uint8_t header = (uint8_t) (pci_read32((uint8_t) bus, slot, 0, 0x0C) >> 16);
            if ((header & 0x80) != 0) {
                for (uint8_t function = 1; function < 8; function++) {
                    add_device((uint8_t) bus, slot, function);
                }
            }
        }
    }
    serial_writestring("MyOS PCI: bus scan complete.\n");
}

uint32_t pci_device_count(void) { return device_count; }
uint16_t pci_device_vendor(uint32_t index) { return index < device_count ? devices[index].vendor : 0xFFFF; }
uint16_t pci_device_id(uint32_t index) { return index < device_count ? devices[index].device : 0xFFFF; }
uint8_t pci_device_bus(uint32_t index) { return index < device_count ? devices[index].bus : 0; }
uint8_t pci_device_slot(uint32_t index) { return index < device_count ? devices[index].slot : 0; }
uint8_t pci_device_function(uint32_t index) { return index < device_count ? devices[index].function : 0; }
uint8_t pci_device_class(uint32_t index) { return index < device_count ? devices[index].class_code : 0; }
uint8_t pci_device_subclass(uint32_t index) { return index < device_count ? devices[index].subclass : 0; }
