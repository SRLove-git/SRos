#include "pci.h"
#include "io.h"
#include "serial.h"

u32 pci_config_read(u8 bus, u8 dev, u8 func, u8 offset)
{
    u32 address = 0x80000000
                | ((u32)bus << 16)
                | ((u32)dev << 11)
                | ((u32)func << 8)
                | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_config_write(u8 bus, u8 dev, u8 func, u8 offset, u32 value)
{
    u32 address = 0x80000000
                | ((u32)bus << 16)
                | ((u32)dev << 11)
                | ((u32)func << 8)
                | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

/* 读取设备的 BAR0-BAR5 */
static void pci_read_bars(u8 bus, u8 dev, u8 func, u32 *bars)
{
    for (int i = 0; i < 6; i++) {
        bars[i] = pci_config_read(bus, dev, func, PCI_BAR0 + i * 4);
    }
}

/* 按 vendor/device ID 查找设备 */
int pci_find_device(u16 vendor_id, u16 device_id, pci_device_t *dev)
{
    for (u32 bus = 0; bus < 256; bus++) {
        for (u32 d = 0; d < 32; d++) {
            for (u32 func = 0; func < 8; func++) {
                u16 vendor = (u16)(pci_config_read((u8)bus, (u8)d, (u8)func, PCI_VENDOR_ID) & 0xFFFF);
                if (vendor == 0xFFFF || vendor == 0x0000) {
                    /* 如果 function 0 不存在，跳过整个 device */
                    if (func == 0) break;
                    continue;
                }
                u16 device = (u16)(pci_config_read((u8)bus, (u8)d, (u8)func, PCI_DEVICE_ID) >> 16);

                if (vendor == vendor_id && device == device_id) {
                    u32 class_reg = pci_config_read((u8)bus, (u8)d, (u8)func, PCI_CLASS_SUBCLASS);
                    u8 irq = (u8)(pci_config_read((u8)bus, (u8)d, (u8)func, PCI_INTERRUPT_LINE) & 0xFF);

                    dev->bus = (u8)bus;
                    dev->dev = (u8)d;
                    dev->func = (u8)func;
                    dev->vendor_id = vendor;
                    dev->device_id = device;
                    dev->class_code = (class_reg >> 24) & 0xFF;
                    dev->subclass = (class_reg >> 16) & 0xFF;
                    dev->prog_if = (class_reg >> 8) & 0xFF;
                    dev->irq_line = irq;
                    pci_read_bars((u8)bus, (u8)d, (u8)func, dev->bar);
                    return 0;
                }

                /* 如果不是多功能设备，跳过其余 function */
                if (func == 0) {
                    u8 header_type = (u8)((pci_config_read((u8)bus, (u8)d, 0, PCI_HEADER_TYPE) >> 16) & 0xFF);
                    if (!(header_type & 0x80)) break;
                }
            }
        }
    }
    return -1; /* 未找到 */
}

/* 按类代码查找 */
int pci_find_by_class(u8 class_code, u8 subclass, pci_device_t *dev)
{
    for (u32 bus = 0; bus < 256; bus++) {
        for (u32 d = 0; d < 32; d++) {
            for (u32 func = 0; func < 8; func++) {
                u16 vendor = (u16)(pci_config_read((u8)bus, (u8)d, (u8)func, PCI_VENDOR_ID) & 0xFFFF);
                if (vendor == 0xFFFF || vendor == 0x0000) {
                    if (func == 0) break;
                    continue;
                }

                u32 class_reg = pci_config_read((u8)bus, (u8)d, (u8)func, PCI_CLASS_SUBCLASS);
                u8 cc = (class_reg >> 24) & 0xFF;
                u8 sc = (class_reg >> 16) & 0xFF;

                if (cc == class_code && sc == subclass) {
                    u16 device = (u16)(pci_config_read((u8)bus, (u8)d, (u8)func, PCI_DEVICE_ID) >> 16);
                    u8 irq = (u8)(pci_config_read((u8)bus, (u8)d, (u8)func, PCI_INTERRUPT_LINE) & 0xFF);

                    dev->bus = (u8)bus;
                    dev->dev = (u8)d;
                    dev->func = (u8)func;
                    dev->vendor_id = vendor;
                    dev->device_id = device;
                    dev->class_code = cc;
                    dev->subclass = sc;
                    dev->prog_if = (class_reg >> 8) & 0xFF;
                    dev->irq_line = irq;
                    pci_read_bars((u8)bus, (u8)d, (u8)func, dev->bar);
                    return 0;
                }

                if (func == 0) {
                    u8 header_type = (u8)((pci_config_read((u8)bus, (u8)d, 0, PCI_HEADER_TYPE) >> 16) & 0xFF);
                    if (!(header_type & 0x80)) break;
                }
            }
        }
    }
    return -1;
}

/* 启用 bus master（DMA 所需） */
void pci_enable_bus_master(pci_device_t *dev)
{
    u16 cmd = (u16)(pci_config_read(dev->bus, dev->dev, dev->func, PCI_COMMAND) & 0xFFFF);
    cmd |= PCI_CMD_IO_SPACE | PCI_CMD_MEM_SPACE | PCI_CMD_BUS_MASTER;
    pci_config_write(dev->bus, dev->dev, dev->func, PCI_COMMAND, cmd);
}
