//
//  Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
//  Use of this source code is subject to the terms of the Microsoft end-user
//  license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
//  If you did not accept the terms of the EULA, you are not authorized to use
//  this source code. For a copy of the EULA, please see the LICENSE.RTF on your
//  install media.
//
#include <windows.h>
#include "oal_kitl.h"

//------------------------------------------------------------------------------
//
//  Function:  OALKitlStart
//
//  This function is called from OEMInit to start KITL. It usualy set KITL
//  configuration and it calls OALKitlInit
//

BOOL OALKitlStart()
{
    return TRUE;
}

//------------------------------------------------------------------------------
//
//  Function:  OALKitlInitRegistry
//
//  This function is called as part of IOCTL_HAL_INITREGISTRY to update
//  registry with information about KITL device. This must be done to avoid
//  loading Windows CE driver in case that it is part of image. On system
//  without KITL is this function replaced with empty stub.
//
VOID OALKitlInitRegistry()
{
}

//------------------------------------------------------------------------------
//
//  Function:  OALKitlPowerOff
//
//  This function is called as part of OEMPowerOff implementation. It should
//  save all information about KITL device and put it to power off mode.
//
VOID OALKitlPowerOff()
{
}

//------------------------------------------------------------------------------
//
//  Function:  OALKitlPowerOn
//
//  This function is called as part of OEMPowerOff implementation. It should
//  restore KITL device back to working state.
//
VOID OALKitlPowerOn()
{
}

//------------------------------------------------------------------------------
//
//  Function:  OALIoCtlVBridge
//
//  This function implement OEMIoControl calls for VMINI related IOCTL codes.
//
BOOL OALIoCtlVBridge(
    UINT32 code, VOID *pInpBuffer, UINT32 inpSize, VOID *pOutBuffer,
    UINT32 outSize, UINT32 *pOutSize
) {
    return FALSE;
}

//------------------------------------------------------------------------------
