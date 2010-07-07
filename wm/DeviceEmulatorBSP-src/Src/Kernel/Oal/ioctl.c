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
//------------------------------------------------------------------------------
//
//  File: ioctl.c           
//
//  This file implements the OEM's IO Control (IOCTL) functions and declares
//  global variables used by the IOCTL component.
//
#include <bsp.h>

#ifdef XIPIOCTL
#include "romxip.h"
#include "msmddoal.h"
#include "deviceid.h"
#endif

// callstack 
#define IOCTL_GET_CALLSTACK        CTL_CODE(FILE_DEVICE_HAL,1099, METHOD_BUFFERED, FILE_ANY_ACCESS)
extern ULONG SC_GetThreadCallStack (HANDLE hThrd, ULONG dwMaxFrames, CallSnapshot lpFrames[], DWORD dwFlags, DWORD dwSkip);
extern CPUPowerReset();

//------------------------------------------------------------------------------
//
//  Global: g_oalIoctlPlatformType/OEM    
//
//  Platform Type/OEM
//
LPCWSTR g_oalIoCtlPlatformType = IOCTL_PLATFORM_TYPE;
LPCWSTR g_oalIoCtlPlatformOEM  = IOCTL_PLATFORM_OEM;

//------------------------------------------------------------------------------
//
//  Global: g_oalIoctlProcessorVendor/Name/Core
//
//  Processor information
//
LPCWSTR g_oalIoCtlProcessorVendor = IOCTL_PROCESSOR_VENDOR;
LPCWSTR g_oalIoCtlProcessorName   = IOCTL_PROCESSOR_NAME;
LPCWSTR g_oalIoCtlProcessorCore   = IOCTL_PROCESSOR_CORE;

UINT32 g_oalIoCtlInstructionSet = IOCTL_PROCESSOR_INSTRUCTION_SET;
UINT32 g_oalIoCtlClockSpeed = IOCTL_PROCESSOR_CLOCK_SPEED;


//------------------------------------------------------------------------------
//
//  Global: g_dwExtensionRAMFMDSize and g_pvExtensionRAMFMDBaseAddr
//
//  RAMFMD initializers used by OEMGetExtensionDRAM
//
//  g_dwExtensionRAMFMDSize updated via FIXUPVAR in config.bib, must 
//  be a multiple of 4K.
//
//  g_pvExtensionRAMFMDBaseAddr filled in by OEMGetExtensionDRAM()
//  if sufficient extension memory is available.
const volatile DWORD g_dwExtensionRAMFMDSize = 0;
PVOID g_pvExtensionRAMFMDBaseAddr;

#ifdef DIFF_UPDATE
static BOOL
OALIoCtlDiffUpdate(
    UINT32  code,
    VOID   *pInBuffer,
    UINT32  inSize,
    VOID   *pOutBuffer,
    UINT32  outSize,
    UINT32 *pOutSize
) {
    BOOL rc;

    (void)IsBinDiffIoControl(code, pInBuffer, inSize, pOutBuffer, outSize, pOutSize, &rc);

    return rc;
}
#endif

#ifdef XIPIOCTL
static BOOL
OALIoCtlXipUpdate(
    UINT32  code,
    VOID   *pInBuffer,
    UINT32  inSize,
    VOID   *pOutBuffer,
    UINT32  outSize,
    UINT32 *pOutSize
) {
    BOOL rc;

    (void)IsXIPUpdateIoControl(code, pInBuffer, inSize, pOutBuffer, outSize, pOutSize, &rc);

    return rc;
}

static BOOL
OALIoCtlMsMdd(
    UINT32  code,
    VOID   *pInBuffer,
    UINT32  inSize,
    VOID   *pOutBuffer,
    UINT32  outSize,
    UINT32 *pOutSize
) {
    BOOL rc;

    (void)IsMSMDDIoControl(code, pInBuffer, inSize, pOutBuffer, outSize, pOutSize, &rc);

    return rc;
}

#endif

static BOOL
OALIoInitRtc(
    UINT32  code,
    VOID   *pInBuffer,
    UINT32  inSize,
    VOID   *pOutBuffer,
    UINT32  outSize,
    UINT32 *pOutSize
) {
    // Do nothing for this on the emulator.
    // We leverage the host sides RTC which is always init'ed.
    *pOutSize = 0;
    return TRUE;
}

// Make sure that this is defined in configurations that may not include filesys.  In the event
// that filesys isn't present this routine will never be called since nobody invokes
// the IOCTL_HAL_INITREGISTRY kernel IOCTL.
#ifndef HKEY_LOCAL_MACHINE
#define HKEY_LOCAL_MACHINE          (( HKEY ) (ULONG_PTR)0x80000002 )
#endif  // HKEY_LOCAL_MACHINE

static BOOL 
OALIoCtlDeviceEmulatorHalInitRegistry( 
    UINT32 code, VOID *pInpBuffer, UINT32 inpSize, VOID *pOutBuffer, 
    UINT32 outSize, UINT32 *pOutSize
){
#ifdef DEBUG
    LPCWSTR pszFname = L"OALIoCtlDeviceEmulatorHalInitRegistry";
#endif  // DEBUG
    BOOL fOk = TRUE;
    BSP_ARGS *pBSPArgs = ((BSP_ARGS *) IMAGE_SHARE_ARGS_UA_START);
    {
        HKEY    hKey;
        DWORD   dwRotation = 0;
        DWORD   dwStatus;
        LPCWSTR pszKeyPath = L"System\\GDI\\Rotation";
        DWORD   dwDesposition = 0;

        switch (pBSPArgs->ScreenOrientation) {
          default:case 0:
            dwRotation = 0;
            break;
          case 1:
            dwRotation= 90;
            break;
          case 2:
            dwRotation = 180;
            break;
          case 3:
            dwRotation = 270 ;
            break;
        }
        DEBUGMSG(TRUE, (L"%s: Set Orientation to %d pBSPArgs->ScreenOrientation = %d \r\n", pszFname,dwRotation,pBSPArgs->ScreenOrientation));
        dwStatus = NKRegCreateKeyExW(HKEY_LOCAL_MACHINE, pszKeyPath, 0, NULL, 0, 0, NULL, &hKey,&dwDesposition);
        
        if(dwStatus != ERROR_SUCCESS) {
            DEBUGMSG(TRUE, (L"%s: NKRegOpenKeyExW('%s') failed %u - 0x%08x rotation is unused\r\n", 
                pszFname, pszKeyPath, dwStatus, dwRotation));
        } else {
            dwStatus = NKRegSetValueEx(hKey, L"Angle", 0, REG_DWORD, (LPBYTE) &dwRotation, sizeof(DWORD));
            if(dwStatus != ERROR_SUCCESS) {
                DEBUGMSG(TRUE, (L"%s: NKRegSetValueEx('Angle') failed %u\r\n", pszFname, dwStatus));
            }
            NKRegCloseKey(hKey);
        }
    }

    // determine the FMD size
    if(g_pvExtensionRAMFMDBaseAddr != NULL) {
        pBSPArgs->pvExtensionRAMFMDBaseAddr = g_pvExtensionRAMFMDBaseAddr;
        pBSPArgs->dwExtensionRAMFMDSize = g_dwExtensionRAMFMDSize;
    }

    // none of the RAMFMD issues are fatal, so we don't set fOk to FALSE even
    // if there's a problem
    DEBUGMSG(TRUE, (L"%s: FMD reserve size 0x%08x; base addr 0x%08x\r\n", pszFname,
        pBSPArgs->dwExtensionRAMFMDSize, pBSPArgs->pvExtensionRAMFMDBaseAddr));
    if(pBSPArgs->dwExtensionRAMFMDSize != 0) {
        if(pBSPArgs->pvExtensionRAMFMDBaseAddr == NULL) {
            DEBUGMSG(TRUE, (L"%s: couldn't reserve 0x%08x bytes; not modifying FMD registry\r\n",
                pszFname, pBSPArgs->dwExtensionRAMFMDSize));
        } else {
            HKEY hKey;
            LPCWSTR pszKeyPath = L"Drivers\\BlockDevice\\RAMFMD";
            DWORD dwStatus = NKRegOpenKeyExW(HKEY_LOCAL_MACHINE, pszKeyPath, 0, 0, &hKey);
            if(dwStatus != ERROR_SUCCESS) {
                DEBUGMSG(TRUE, (L"%s: NKRegOpenKeyExW('%s') failed %u - 0x%08x bytes extension ram unused\r\n", 
                    pszFname, pszKeyPath, dwStatus, pBSPArgs->dwExtensionRAMFMDSize));
            } else {
                dwStatus = NKRegSetValueEx(hKey, L"Address", 0, REG_DWORD, 
                    (LPBYTE) &pBSPArgs->pvExtensionRAMFMDBaseAddr, sizeof(DWORD));
                if(dwStatus != ERROR_SUCCESS) {
                    DEBUGMSG(TRUE, (L"%s: NKRegSetValueEx('Address') failed %u\r\n", 
                        pszFname, dwStatus));
                }
                dwStatus = NKRegSetValueEx(hKey, L"Size", 0, REG_DWORD, 
                    (LPBYTE) &pBSPArgs->dwExtensionRAMFMDSize, sizeof(DWORD));
                if(dwStatus != ERROR_SUCCESS) {
                    DEBUGMSG(TRUE, (L"%s: NKRegSetValueEx('Size') failed %u\r\n", 
                        pszFname, dwStatus));
                }
                NKRegCloseKey(hKey);
            }
        }
    }
    if(fOk) {
        fOk = OALIoCtlHalInitRegistry(code, pInpBuffer, inpSize, pOutBuffer,
            outSize, pOutSize);
    }
    return fOk;
}

static BOOL 
OALIoCtlHalReboot( 
    UINT32 code, VOID *pInpBuffer, UINT32 inpSize, VOID *pOutBuffer, 
    UINT32 outSize, UINT32 *pOutSize
){
    RETAILMSG(1,(TEXT("Calling CPUPowerReset\r\n")));
    CPUPowerReset();
    return TRUE;
}

//------------------------------------------------------------------------------
//
//  Global: g_oalIoCtlTable[]    
//
//  IOCTL handler table. This table includes the IOCTL code/handler pairs  
//  defined in the IOCTL configuration file. This global array is exported 
//  via oal_ioctl.h and is used by the OAL IOCTL component.
//
const OAL_IOCTL_HANDLER g_oalIoCtlTable[] = {
#include "ioctl_tab.h"
};

//------------------------------------------------------------------------------

