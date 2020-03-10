//
// Created by bgklug on 8/13/19.
//

#ifndef PCI_DIO_120_H
#define PCI_DIO_120_H

#include <stdint.h>

/*
 * PCI-DIO-120 device IDs
 */
#define VENDOR_ID 0x494f
#define DEVICE_ID 0x0c78

/*
 * PCI-DIO-120 register offsets as defined in https://www.accesio.com/MANUALS/PCI-DIO-120.pdf
 */
#define CNTRL_GRP_0  3
#define PA_GRP_0     0
#define PB_GRP_0     1
#define PC_GRP_0     2
#define CNTRL_GRP_1  7
#define PA_GRP_1     4
#define PB_GRP_1     5
#define PC_GRP_1     6
#define CNTRL_GRP_2 11
#define PA_GRP_2     8
#define PB_GRP_2     9
#define PC_GRP_2    10
#define CNTRL_GRP_3 15
#define PA_GRP_3    12
#define PB_GRP_3    13
#define PC_GRP_3    14
#define CNTRL_GRP_4 19
#define PA_GRP_4    16
#define PB_GRP_4    17
#define PC_GRP_4    18

/*
 * PCI-DIO Control Register Bits
 */
#define CONTROL_PORT_C_LO_INPUT  0x01
#define CONTROL_PORT_B_INPUT  0x02
#define CONTROL_PORT_C_HI_INPUT  0x08
#define CONTROL_PORT_A_INPUT  0x10
#define CONTROL_ALL_PORTS_INPUT  (CONTROL_PORT_A_INPUT & CONTROL_PORT_B_INPUT & CONTROL_PORT_C_HI_INPUT & CONTROL_PORT_C_LO_INPUT)
#define CONTROL_ACTIVE  0x80


void init_pci_dio_120();
void write_pci_dio_120(int reg, uint8_t data);
uint8_t read_pci_dio_120(int reg);

#endif //PCI_DIO_120_H
