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
#ifndef __ARGS_H
#define __ARGS_H

//------------------------------------------------------------------------------
//
// File:        args.h
//
// Description: This header file defines device structures and constant related
//              to boot configuration. BOOT_CFG structure defines layout of
//              persistent device information. It is used to control boot
//              process. BSP_ARGS structure defines information passed from
//              boot loader to kernel HAL/OAL. Each structure has version
//              field which should be updated each time when structure layout
//              change.
//
//------------------------------------------------------------------------------

#include "oal_args.h"
#include "oal_kitl.h"


//------------------------------------------------------------------------------
// Bootloader configuration parameters (stored in NOR flash).
//
#define CONFIG_SIGNATURE                    0x12345678

typedef struct
{
    ULONG  Signature;           // Config signature (used to validate).
    USHORT VerMajor;            // Config major version.
    USHORT VerMinor;            // Config minor version.
    ULONG  ConfigFlags;         // General bootloader flags.
    ULONG  IPAddr;              // Device static IP address.
    ULONG  SubnetMask;          // Device subnet mask.
    BYTE   BootDelay;           // Bootloader count-down delay.
    BYTE   LoadDeviceOrder;     // Search order for download devices.
    USHORT CS8900MAC[3];        // Crystal CS8900A MAC address.
    ULONG  LaunchAddress;       // Kernel region launch address.

} BOOT_CFG, *PBOOT_CFG;

#define CONFIG_AUTOBOOT_DEFAULT_COUNT       5
#define CONFIG_FLAGS_AUTOBOOT               (1 << 0)
#define CONFIG_FLAGS_DHCP                   (1 << 1)
#define CONFIG_FLAGS_SAVETOFLASH            (1 << 2)

//------------------------------------------------------------------------------

#define BSP_ARGS_VERSION    1

#define BSP_SCREEN_SIGNATURE 0xde12de34

typedef struct {
    OAL_ARGS_HEADER header;

    UINT8 deviceId[16];                 // Device identification
    OAL_KITL_ARGS kitl;

    // CAUTION:  The DeviceEmulator contains hard-coded knowledge of the addresses and contents of these
    //           three fields.
    UINT32 ScreenSignature;		// Set to BSP_SCREEN_SIGNATURE if the DeviceEmulator specifies screen size
    UINT16 ScreenWidth;                 // May be set by the DeviceEmulator, or zero
    UINT16 ScreenHeight;                // May be set by the DeviceEmulator, or zero
    UINT16 ScreenBitsPerPixel;          // May be set by the DeviceEmulator, or zero
    union {
        struct {
            UINT16 fSoftReset:1;
        }bit;
        UINT16 u16Value;
    } EmulatorFlags;               // May be set by the DeviceEmulator, or zero
    UINT8  ScreenOrientation;           // May be set by the DeviceEmulator, or zero
    UINT8  Pad[15];                     // May be set by the DeviceEmulator, or zero

    BOOL   fUpdateMode;                 // Is the device in update mode?

    // record the size and location of the RAM FMD if present
    DWORD dwExtensionRAMFMDSize;
    PVOID pvExtensionRAMFMDBaseAddr;
} BSP_ARGS;

//------------------------------------------------------------------------------

#endif
