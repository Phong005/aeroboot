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

#ifndef _LOADER_H_
#define _LOADER_H_

#include <blcommon.h>

// Bootloader version.
//
#define EBOOT_VERSION_MAJOR     2
#define EBOOT_VERSION_MINOR     5


//------------------------------------------------------------------------------
//
//  Section Name:   Memory Configuration
//  Description.:   The constants defining memory layout below must match
//                  those defined in the .bib files.
//
//------------------------------------------------------------------------------

#define CACHED_TO_UNCACHED_OFFSET   0x20000000

#define OS_RAM_IMAGE_BASE           0x80040000
#define OS_RAM_IMAGE_SIZE           0x01E00000


#define EBOOT_RAM_IMAGE_BASE        0x8c038000
#define AMD_FLASH_START             (UINT32)OALPAtoVA(BSP_BASE_REG_PA_AM29LV800, FALSE)
#define AMD_FLASH_LENGTH            0x00100000                                      // Size of AMD flash part.
#define EBOOT_STORE_OFFSET          0                                               // Physical flash address byte offset 
#define EBOOT_STORE_ADDRESS         (AMD_FLASH_START + EBOOT_STORE_OFFSET)          // Physical flash address where eboot 
#define EBOOT_STORE_MAX_LENGTH      0x00040000                                      // Maximum size of eboot in flash.
#define EBOOT_CONFIG_OFFSET         0x000F0000                                      // Physical flash address byte offset 
#define EBOOT_CONFIG_ADDRESS        (AMD_FLASH_START + EBOOT_SETTINGS_OFFSET)       // Physical flash address where eboot 

//------------------------------------------------------------------------------
//
//  Define Name:    RAM_START/END
//  Description:    Defines the start/end address of cached RAM.
//
//------------------------------------------------------------------------------

#define RAM_START                   (0x80000000)
#define RAM_SIZE                    (32 * 1024 * 1024)              // 32MB
#define RAM_END                     (RAM_START + RAM_SIZE - 1)

//------------------------------------------------------------------------------
//
//  Define Name:    FLASH_START/END
//  Description:    Defines the start/end address of cached FLASH.
//
//------------------------------------------------------------------------------

#define FLASH_START                 (0x92000000)
#define FLASH_SIZE                  (AMD_FLASH_LENGTH)
#define FLASH_END                   (RAM_START + RAM_SIZE - 1)
                                                                                    // settings are stored.
//------------------------------------------------------------------------------
//
//  Define Name:    FLASH_EBOOT_START/END
//  Description:    Defines start/end address of EBOOT image. Required for 
//					setting the g_ImageType.
//
//------------------------------------------------------------------------------

#define FLASH_EBOOT_START           (UINT32)OALPAtoVA(BSP_BASE_REG_PA_AM29LV800, TRUE)
#define FLASH_EBOOT_END             (FLASH_EBOOT_START + EBOOT_STORE_MAX_LENGTH - 1)


#define FILE_CACHE_START            (0x8c200000 | CACHED_TO_UNCACHED_OFFSET)        // Start of file cache (temporary store 
                                                                                    // for flash images).

// Driver globals pointer (parameter sharing memory used by bootloader and OS).
//
#define pBSPArgs                    ((BSP_ARGS *) IMAGE_SHARE_ARGS_UA_START)


// Platform "BOOTME" root name.
//
#define PLATFORM_STRING         "DEVICEEM"

// Bootloader configuration parameters (stored in the bootloader parameter partition in NAND flash).
//
typedef struct _NAND_BOOTLOADER_PARAMS_
{
    ULONG  Signature;           // Config signature (used to validate).
    USHORT VerMajor;            // Config major version.
    USHORT VerMinor;            // Config minor version.
    ULONG  StoreOffset;         // Region storage location (BINFS partition offset).
    ULONG  RunAddress;          // Region RAM address (loader copies region to this address).
    ULONG  Length;              // Region length (in bytes).
    ULONG  LaunchAddress;       // Region launch address.

} NAND_BOOTLOADER_PARAMS, *PNAND_BOOTLOADER_PARAMS;

// Download image type (used to decide amongst storage options).
//
typedef enum _IMAGE_TYPE_
{
    IMAGE_LOADER,
    IMAGE_OS_BIN,
    IMAGE_OS_NB0
} IMAGE_TYPE, *PIMAGE_TYPE;

// Loader function prototypes.
//
BOOL InitEthDevice(PBOOT_CFG pBootCfg);
int OEMReadDebugByte(void);
BOOL OEMVerifyMemory(DWORD dwStartAddr, DWORD dwLength);
BOOL AM29LV800_WriteFlash(UINT32 dwOffset, PBYTE pData, UINT32 dwLength);
BOOL AM29LV800_ReadFlash(UINT32 dwOffset, PBYTE pData, UINT32 dwLength);
BOOL AM29LV800_EraseFlash(UINT32 dwOffset, UINT32 dwLength);
BOOL AM29LV800_Init(UINT32 dwAddr);
void Launch(DWORD dwLaunchAddr);
PVOID PCMCIA_Init(void);
BOOL WriteDiskImageToSmartMedia(DWORD dwImageStart, DWORD dwImageLength, BOOT_CFG *pBootCfg);
BOOL FormatSmartMedia(void);
void OEMWriteDebugLED(WORD wIndex, DWORD dwPattern);

#endif  // _LOADER_H_.
