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
#include <bsp.h>

//------------------------------------------------------------------------------
//
//  Function:  HalTranslateBusAddress
//
//  This function translates a bus-relative physical address into
//  the corresponding system physical address. To avoid code duplication
//  HAL is called to do translation.
//
BOOLEAN HalTranslateBusAddress(
   INTERFACE_TYPE ifcType, ULONG busNumber, PHYSICAL_ADDRESS busAddress,
   ULONG *pAddressSpace, PHYSICAL_ADDRESS *pSystemAddress
) {
    BOOLEAN rc = FALSE;
    OAL_DDK_PARAMS params;
    UINT32 outSize;

    // Check input parameters
    if (pAddressSpace == NULL || pSystemAddress == NULL) return FALSE;

    memset(&params, 0, sizeof(params));
    params.function = IOCTL_OAL_TRANSBUSADDRESS;
    params.transAddress.ifcType = ifcType;
    params.transAddress.busNumber = busNumber;
    params.transAddress.space = *pAddressSpace;
    params.transAddress.address = busAddress.QuadPart;

    if (!KernelIoControl(
        IOCTL_HAL_DDK_CALL, &params, sizeof(params), NULL, 0, &outSize
    )) goto cleanUp;

    rc = params.rc;
    *pAddressSpace = params.transAddress.space;
    pSystemAddress->QuadPart = params.transAddress.address;
    
cleanUp:
    DEBUGMSG(TRUE, (
        L"HalTranslateBusAddress: %d %d %08x%08x %d --> %08x%08x %d\r\n", 
        ifcType, busNumber, busAddress.HighPart, busAddress.LowPart,
        *pAddressSpace, pSystemAddress->HighPart, pSystemAddress->LowPart, rc
    ));      
    return rc;
}

//------------------------------------------------------------------------------
//
//  Function:  HalTranslateSystemAddress
//
//  This function translates a system physical address into
//  the corresponding bus relative physical address. To avoid code duplication
//  HAL is called to do translation.
//
BOOLEAN HalTranslateSystemAddress(
   INTERFACE_TYPE ifcType, ULONG busNumber, PHYSICAL_ADDRESS systemAddress, 
   PHYSICAL_ADDRESS *pBusAddress
) {
    BOOLEAN rc = FALSE;
    OAL_DDK_PARAMS params;
    UINT32 outSize;

    // Check input parameters
    if (pBusAddress == NULL) goto cleanUp;

    // Prepare input parameters for HAL call
    memset(&params, 0, sizeof(params));
    params.function = IOCTL_OAL_TRANSSYSADDRESS;
    params.transAddress.ifcType = ifcType;
    params.transAddress.busNumber = busNumber;
    params.transAddress.address = systemAddress.QuadPart;

    // Call to HAL
    if (!KernelIoControl(
        IOCTL_HAL_DDK_CALL, &params, sizeof(params), NULL, 0, &outSize
    )) goto cleanUp;

    // Get output parameters from HAL call
    pBusAddress->QuadPart = params.transAddress.address;

    // Done
    rc = TRUE;
    
cleanUp:
    return rc;
}

//------------------------------------------------------------------------------
