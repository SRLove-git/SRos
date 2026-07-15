#ifndef PCI_H
#define PCI_H

#include "types.h"

/* PCI 配置空间 I/O 端口 */
#define PCI_CONFIG_ADDRESS  0x0CF8
#define PCI_CONFIG_DATA     0x0CFC

/* PCI 标准 vendor/device ID */
#define PCI_VENDOR_REALTEK  0x10EC
#define PCI_DEVICE_RTL8139  0x8139

/* PCI 类代码 */
#define PCI_CLASS_NETWORK       0x02
#define PCI_SUBCLASS_ETHERNET   0x00

/* PCI 配置空间偏移 */
#define PCI_VENDOR_ID       0x00
#define PCI_DEVICE_ID       0x02
#define PCI_COMMAND         0x04
#define PCI_STATUS          0x06
#define PCI_REVISION        0x08
#define PCI_CLASS_SUBCLASS  0x08
#define PCI_CLASS_CODE      0x0B
#define PCI_SUBCLASS        0x0A
#define PCI_HEADER_TYPE     0x0E
#define PCI_BAR0            0x10
#define PCI_BAR1            0x14
#define PCI_BAR2            0x18
#define PCI_BAR3            0x1C
#define PCI_BAR4            0x20
#define PCI_INTERRUPT_LINE  0x3C

/* PCI 命令寄存器标志 */
#define PCI_CMD_IO_SPACE     0x0001
#define PCI_CMD_MEM_SPACE    0x0002
#define PCI_CMD_BUS_MASTER   0x0004

/* PCI 设备信息 */
typedef struct {
    u8  bus;
    u8  dev;
    u8  func;
    u16 vendor_id;
    u16 device_id;
    u8  class_code;
    u8  subclass;
    u8  prog_if;
    u8  irq_line;
    u32 bar[6];
} pci_device_t;

/* 读取 PCI 配置空间 */
u32 pci_config_read(u8 bus, u8 dev, u8 func, u8 offset);
void pci_config_write(u8 bus, u8 dev, u8 func, u8 offset, u32 value);

/* 扫描 PCI 总线 */
int pci_find_device(u16 vendor_id, u16 device_id, pci_device_t *dev);
int pci_find_by_class(u8 class_code, u8 subclass, pci_device_t *dev);
void pci_enable_bus_master(pci_device_t *dev);

#endif /* PCI_H */
