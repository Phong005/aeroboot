//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.
Copyright (c) 2002. Samsung Electronics, co. ltd  All rights reserved.

Module Name:  

Abstract:

Notes: 
--*/

#ifndef __PD6710_H__
#define __PD6710_H__

#define B6710_Tacs	(0x3)	// 4clk
#define B6710_Tcos	(0x3)	// 4clk
#define B6710_Tacc	(0x7)	// 14clk
#define B6710_Tcoh	(0x3)	// 1clk
#define B6710_Tah	(0x3)	// 4clk
#define B6710_Tacp	(0x3)	// 6clk
#define B6710_PMC	(0x3)	// normal(1data)

#define PD6710_MEM_BASE_ADDRESS     (0xA4000000)	//(0x04000000)//nGCS2 0x04000000
#define PD6710_IO_BASE_ADDRESS      (0xA5000000)	//(0x05000000)//      0x05000000

//
// PCIC register offsets
//
#define REG_INTERFACE_STATUS                 0x01
#define REG_POWER_CONTROL                    0x02
#define REG_INTERRUPT_AND_GENERAL_CONTROL    0x03
#define REG_CARD_STATUS_CHANGE               0x04
#define REG_STATUS_CHANGE_INT_CONFIG         0x05
#define REG_WINDOW_ENABLE                    0x06
#define REG_IO_WINDOW_CONTROL                0x07
#define REG_IO_MAP0_START_ADDR_LO            0x08
#define REG_IO_MAP0_START_ADDR_HI            0x09
#define REG_IO_MAP0_END_ADDR_LO              0x0A
#define REG_IO_MAP0_END_ADDR_HI              0x0B
#define REG_IO_MAP1_START_ADDR_LO            0x0C
#define REG_IO_MAP1_START_ADDR_HI            0x0D
#define REG_IO_MAP1_END_ADDR_LO              0x0E
#define REG_IO_MAP1_END_ADDR_HI              0x0F
#define REG_MEM_MAP0_START_ADDR_LO           0x10
#define REG_MEM_MAP0_START_ADDR_HI           0x11
#define REG_MEM_MAP0_END_ADDR_LO             0x12
#define REG_MEM_MAP0_END_ADDR_HI             0x13
#define REG_MEM_MAP0_ADDR_OFFSET_LO          0x14
#define REG_MEM_MAP0_ADDR_OFFSET_HI          0x15
#define REG_GENERAL_CONTROL                  0x16
#define REG_FIFO_CTRL						 0x17

#define REG_MEM_MAP1_START_ADDR_LO           0x18
#define REG_MEM_MAP1_START_ADDR_HI           0x19
#define REG_MEM_MAP1_END_ADDR_LO             0x1A
#define REG_MEM_MAP1_END_ADDR_HI             0x1B
#define REG_MEM_MAP1_ADDR_OFFSET_LO          0x1C
#define REG_MEM_MAP1_ADDR_OFFSET_HI          0x1D
#define REG_GLOBAL_CONTROL                   0x1E
#define REG_CHIP_INFO                        0x1F
#define REG_MEM_MAP2_START_ADDR_LO           0x20
#define REG_MEM_MAP2_START_ADDR_HI           0x21
#define REG_MEM_MAP2_END_ADDR_LO             0x22
#define REG_MEM_MAP2_END_ADDR_HI             0x23
#define REG_MEM_MAP2_ADDR_OFFSET_LO          0x24
#define REG_MEM_MAP2_ADDR_OFFSET_HI          0x25
//
//
#define REG_MEM_MAP3_START_ADDR_LO           0x28
#define REG_MEM_MAP3_START_ADDR_HI           0x29
#define REG_MEM_MAP3_END_ADDR_LO             0x2A
#define REG_MEM_MAP3_END_ADDR_HI             0x2B
#define REG_MEM_MAP3_ADDR_OFFSET_LO          0x2C
#define REG_MEM_MAP3_ADDR_OFFSET_HI          0x2D
//
//
#define REG_MEM_MAP4_START_ADDR_LO           0x30
#define REG_MEM_MAP4_START_ADDR_HI           0x31
#define REG_MEM_MAP4_END_ADDR_LO             0x32
#define REG_MEM_MAP4_END_ADDR_HI             0x33
#define REG_MEM_MAP4_ADDR_OFFSET_LO          0x34
#define REG_MEM_MAP4_ADDR_OFFSET_HI          0x35
#define REG_IO_MAP0_OFFSET_ADDR_LO           0x36
#define REG_IO_MAP0_OFFSET_ADDR_HI           0x37
#define REG_IO_MAP1_OFFSET_ADDR_LO           0x38
#define REG_IO_MAP1_OFFSET_ADDR_HI           0x39
#define REG_MEM_MAP0_WINDOW_PAGE             0x40
#define REG_MEM_MAP1_WINDOW_PAGE             0x41
#define REG_MEM_MAP2_WINDOW_PAGE             0x42
#define REG_MEM_MAP3_WINDOW_PAGE             0x43
#define REG_MEM_MAP4_WINDOW_PAGE             0x44

//
// Relative offsets for memory window control registers
//
#define REG_MEM_MAP_START_ADDR_LO            0x00
#define REG_MEM_MAP_START_ADDR_HI            0x01
#define REG_MEM_MAP_END_ADDR_LO              0x02
#define REG_MEM_MAP_END_ADDR_HI              0x03
#define REG_MEM_MAP_ADDR_OFFSET_LO           0x04
#define REG_MEM_MAP_ADDR_OFFSET_HI           0x05

#define REG_MEM_MAP_STRIDE                   0x08

//
// Interface status register 0x01
//
#define STS_BVD2                             0x01
#define STS_BVD1                             0x02
#define STS_CD1                              0x04
#define STS_CD2                              0x08
#define STS_WRITE_PROTECT                    0x10
#define STS_CARD_READY                       0x20
#define STS_CARD_POWER_ON                    0x40
#define STS_GPI                              0x80

//
// Power and RESETDRV control register 0x02
//
#define PWR_VPP1_BIT0							0x01
#define PWR_VPP1_BIT1							0x02
#define PWR_VPP2_BIT0							0x04
#define PWR_VPP2_BIT1							0x08
#define PWR_VCC_POWER							0x10
#define PWR_AUTO_POWER 							0x20
#define PWR_RESUME_RESET						0x40
#define PWR_OUTPUT_ENABLE 						0x80

//
// Interrupt and general control register 0x03
//
#define INT_IRQ_BIT0                         0x01
#define INT_IRQ_BIT1                         0x02
#define INT_IRQ_BIT2                         0x04
#define INT_IRQ_BIT3                         0x08
#define INT_ENABLE_MANAGE_INT                0x10
#define INT_CARD_IS_IO                       0x20
#define INT_CARD_NOT_RESET                   0x40
#define INT_RING_INDICATE_ENABLE             0x80

//
// Card Status change register 0x04
//
#define CSC_BATTERY_DEAD_OR_STS_CHG          0x01
#define CSC_BATTERY_WARNING                  0x02
#define CSC_READY_CHANGE                     0x04
#define CSC_DETECT_CHANGE                    0x08
#define CSC_GPI_CHANGE                       0x10

//
// Card Status change interrupt configuration register 0x05
//
#define CFG_BATTERY_DEAD_ENABLE              0x01
#define CFG_BATTERY_WARNING_ENABLE           0x02
#define CFG_READY_ENABLE                     0x04
#define CFG_CARD_DETECT_ENABLE               0x08
#define CFG_MANAGEMENT_IRQ_BIT0              0x10
#define CFG_MANAGEMENT_IRQ_BIT1              0x20
#define CFG_MANAGEMENT_IRQ_BIT2              0x40
#define CFG_MANAGEMENT_IRQ_BIT3              0x80

//
// Address window enable register 0x06
//
#define WIN_MEM_MAP0_ENABLE                  0x01
#define WIN_MEM_MAP1_ENABLE                  0x02
#define WIN_MEM_MAP2_ENABLE                  0x04
#define WIN_MEM_MAP3_ENABLE                  0x08
#define WIN_MEM_MAP4_ENABLE                  0x10
#define WIN_MEMCS16_DECODE                   0x20
#define WIN_IO_MAP0_ENABLE                   0x40
#define WIN_IO_MAP1_ENABLE                   0x80

//
// I/O control register 0x07
//
#define ICR_0_IO_16BIT                       0x01
#define ICR_0_IOCS16                         0x02
#define ICR_0_ZERO_WAIT_STATE                0x04
#define ICR_0_WAIT_STATE                     0x08
#define ICR_1_IO_16BIT                       0x10
#define ICR_1_IOCS16                         0x20
#define ICR_1_ZERO_WAIT_STATE                0x40
#define ICR_1_WAIT_STATE                     0x80

//
// MISC Control 1 register 0x16
//
#define MISC1_5V_DETECT							0x01
#define MISC1_VCC_33							0x02
#define MISC1_PM_IRQ							0x04
#define MISC1_PS_IRQ							0x08
#define MISC1_SPK_ENABLE						0x10
#define MISC1_INPACK_ENABLE						0x80

//
// FIFO Control register 0x17
//
#define FIFO_EMPTY_WRITE						0x80

//
// MISC Control 2 register  0x1E
//
#define MISC2_BFS								0x01
#define MISC2_LOW_POWER_MODE					0x02
#define MISC2_SUSPEND							0x04
#define MISC2_5V_CORE							0x08
#define MISC2_LED_ENABLE						0x10
#define MISC2_3STATE_BIT7						0x20
#define MISC2_IRQ15_RIOUT						0x80

//
// System memory address mapping start high byte register 0x11
// (also 0x19, 0x21, 0x29 and 0x31)
//
#define MSH_ZERO_WAIT_STATE                  0x40
#define MSH_MEM_16BIT                        0x80

//
// System memory address mapping stop high byte register 0x13
// (also 0x1B, 0x23, 0x2B and 0x33)
//
#define MTH_WAIT_STATE_BIT0                  0x40
#define MTH_WAIT_STATE_BIT1                  0x80

//
// Card memory offset address high byte register 0x13
// (also 0x1B, 0x23, 0x2B and 0x33)
//
#define MOH_REG_ACTIVE                       0x40
#define MOH_WRITE_PROTECT                    0x80

// more PD6710 specific flags
#define REG_CARD_IO_MAP0_OFFSET_L 				0x36
#define REG_CARD_IO_MAP0_OFFSET_H 				0x37
#define REG_CARD_IO_MAP1_OFFSET_L 				0x38
#define REG_CARD_IO_MAP1_OFFSET_H 				0x39
#define REG_SETUP_TIMING0						0x3a
#define REG_CMD_TIMING0							0x3b
#define REG_RECOVERY_TIMING0					0x3c
#define REG_SETUP_TIMING1						0x3d
#define REG_CMD_TIMING1							0x3e
#define REG_RECOVERY_TIMING1					0x3f
#define REG_LAST_INDEX 							REG_RECOVERY_TIMING1

#define EVENT_MASK_WRITE_PROTECT    0x0001 // write protect change
#define EVENT_MASK_CARD_LOCK        0x0002 // card lock change
#define EVENT_MASK_EJECT_REQ        0x0004 // ejection request
#define EVENT_MASK_INSERT_REQ       0x0008 // insertion request
#define EVENT_MASK_BATTERY_DEAD     0x0010 // battery dead
#define EVENT_MASK_BATTERY_LOW      0x0020 // battery low
#define EVENT_MASK_CARD_READY       0x0040 // ready change
#define EVENT_MASK_CARD_DETECT      0x0080 // card detect change
#define EVENT_MASK_POWER_MGMT       0x0100 // power management change
#define EVENT_MASK_RESET            0x0200 // card resets
#define EVENT_MASK_STATUS_CHANGE    0x0400 // card generated status change interrupts


#endif /*__PD6710_H__*/
