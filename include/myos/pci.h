#ifndef MYOS_PCI_H
#define MYOS_PCI_H

#include <stdint.h>

void pci_initialize(void);
uint32_t pci_device_count(void);
uint16_t pci_device_vendor(uint32_t index);
uint16_t pci_device_id(uint32_t index);
uint8_t pci_device_bus(uint32_t index);
uint8_t pci_device_slot(uint32_t index);
uint8_t pci_device_function(uint32_t index);
uint8_t pci_device_class(uint32_t index);
uint8_t pci_device_subclass(uint32_t index);

#endif
