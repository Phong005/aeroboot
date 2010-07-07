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
#include <bsp.h>

//------------------------------------------------------------------------------
#define AMD_FLASH_START             (UINT32)OALPAtoVA(BSP_BASE_REG_PA_AM29LV800, FALSE)
#define EBOOT_CONFIG_OFFSET         0x000F0000                                      // Physical flash address byte offset 

VOID CreateName(CHAR *pPrefix, UINT16 mac[3], CHAR *pBuffer);

/*
    @func   BOOL | ReadBootConfigFromFlash | Read bootloader settings from flash.
    @rdesc  TRUE = Success, FALSE = Failure.
    @comm    
    @xref   
*/
static BOOL ReadBootConfigFromFlash(PBOOT_CFG pBootCfg)
{
    PBYTE pReadPtr = (PBYTE)(AMD_FLASH_START + EBOOT_CONFIG_OFFSET);
    BOOLEAN bResult = FALSE;

    OALMSG(OAL_FUNC, (TEXT("+ReadBootConfig.\r\n")));

    // Valid caller buffer?
    if (!pBootCfg)
    {
        OALMSG(OAL_ERROR, (TEXT("ERROR: Bad caller buffer.\r\n")));
        goto CleanUp;
    }


    // Read settings from flash...
    memcpy(pBootCfg, pReadPtr, sizeof(BOOT_CFG));

    // Check configuration signature...
    //
    if (pBootCfg->Signature != CONFIG_SIGNATURE)
    {
        OALMSG(OAL_WARN, (TEXT("WARNING: Boot configuration signature invalid.\r\n")));
        goto CleanUp;
    }

    bResult = TRUE;

CleanUp:

    OALMSG(OAL_FUNC, (TEXT("-ReadBootConfig.\r\n")));
    return(bResult);

}

//------------------------------------------------------------------------------
//
//  Function:  OALArgsQuery
//
//  This function is called from other OAL modules to return boot arguments.
//  Boot arguments are typically placed in fixed memory location and they are
//  filled by boot loader. In case that boot arguments can't be located
//  the function should return NULL. The OAL module then must use default
//  values.
//
VOID* OALArgsQuery(UINT32 type)
{
    VOID *pData = NULL;
    BSP_ARGS *pArgs;

    OALMSG(OAL_ARGS&&OAL_FUNC, (L"+OALArgsQuery(%d)\r\n", type));

    // Get pointer to expected boot args location
    pArgs = (BSP_ARGS*)IMAGE_SHARE_ARGS_UA_START;

    // Check if there is expected signature
    if (
        pArgs->header.signature  != OAL_ARGS_SIGNATURE ||
        pArgs->header.oalVersion != OAL_ARGS_VERSION   ||
        pArgs->header.bspVersion != BSP_ARGS_VERSION
    ) {
        BOOT_CFG bootCfg;
        UINT32 uiFlags;
        
        // Look in NOR flash for KITL arguments
        if(ReadBootConfigFromFlash(&bootCfg)) {
            // go ahead and create a fake one -- the Emulator writes
            // some arguments to the display section and fills in the soft reset
            // flag without generating a valid header
            OALMSG(OAL_INFO, (L"INFO: Constructing KITL arguments based on flash config.\r\n"));
            pArgs->header.signature  = OAL_ARGS_SIGNATURE;
            pArgs->header.oalVersion = OAL_ARGS_VERSION ;
            pArgs->header.bspVersion = BSP_ARGS_VERSION;

            uiFlags = pArgs->kitl.flags;
            pArgs->kitl.flags = OAL_KITL_FLAGS_ENABLED | OAL_KITL_FLAGS_DHCP | OAL_KITL_FLAGS_VMINI;
            if(uiFlags & OAL_KITL_FLAGS_PASSIVE) {
                OALMSG(OAL_INFO, (L"INFO: enabling passive KITL.\r\n"));
                pArgs->kitl.flags |= OAL_KITL_FLAGS_PASSIVE;
            }
            pArgs->kitl.devLoc.IfcType = Internal;
            pArgs->kitl.devLoc.BusNumber = 0;
            pArgs->kitl.devLoc.LogicalLoc = BSP_BASE_REG_PA_CS8900A_IOBASE;
            memcpy(pArgs->kitl.mac, &bootCfg.CS8900MAC, sizeof(pArgs->kitl.mac));
            pArgs->kitl.ipAddress = 0;

            // format a device name
            CreateName(BSP_DEVICE_PREFIX, pArgs->kitl.mac, pArgs->deviceId);

        } else {
            OALMSG(OAL_INFO, (L"INFO: OALArgsQuery can't find args structure in memory.\r\n"));
            goto cleanUp;
        }
    }

    // Depending on required args
    switch (type) {
    case OAL_ARGS_QUERY_DEVID:
        pData = &pArgs->deviceId;
        break;
    case OAL_ARGS_QUERY_KITL:
        pData = &pArgs->kitl;
        break;
    }

cleanUp:
    OALMSG(OAL_ARGS&&OAL_FUNC, (L"-OALArgsQuery(pData = 0x%08x)\r\n", pData));
    return pData;
}

//------------------------------------------------------------------------------

//
//  Function:  CreateName
//
//  This function create device name from prefix and mac address (usually last
//  two bytes of MAC address used for download).
//
VOID CreateName(CHAR *pPrefix, UINT16 mac[3], CHAR *pBuffer)
{
    UINT32 count;
    UINT16 base, code;

    count = 0;
    code = (mac[2] >> 8) | ((mac[2] & 0x00ff) << 8);
    while (count < OAL_KITL_ID_SIZE - 6 && pPrefix[count] != '\0') {
        pBuffer[count] = pPrefix[count];
        count++;
    }        
    base = 10000;
    while (base > code && base != 0) base /= 10;
    if (base == 0) { 
        pBuffer[count++] = '0';
    } else {
        while (base > 0) {
            pBuffer[count++] = code/base + '0';
            code %= base;
            base /= 10;
        }
    }        
    pBuffer[count] = '\0';
}

//------------------------------------------------------------------------------
