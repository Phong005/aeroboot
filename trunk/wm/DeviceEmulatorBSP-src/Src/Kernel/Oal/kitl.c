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
//  File:  kitl.c
//
//  Support routines for KITL. 
//
//  Note: this routines are stubbed out in the kern image.
//
//------------------------------------------------------------------------------

#include <bsp.h>
#include <kitl_cfg.h>
#include <fmd.h>

//------------------------------------------------------------------------------

BOOL OALKitlStart()
{
    BOOL rc;
    OAL_KITL_ARGS *pArgs, args;
    CHAR *szDeviceId, buffer[OAL_KITL_ID_SIZE];

    OALMSG(OAL_KITL&&OAL_FUNC, (L"+OALKitlStart\r\n"));

    // Look for bootargs left by the bootloader or left over from an earlier boot.
    //
    pArgs      = (OAL_KITL_ARGS*)OALArgsQuery(OAL_ARGS_QUERY_KITL);
    szDeviceId = (CHAR*)OALArgsQuery(OAL_ARGS_QUERY_DEVID);

    // If we don't have bootargs in RAM, look first in NOR flash for the information
    // otherwise look on the SmartMedia NAND card (in case we're performing a NAND-only) boot.
    //
    if (pArgs == NULL)
    {
        SectorInfo si;
        UINT8 maccount = 0;

        // Get MAC address from NAND flash...
        //
        if (FMD_Init(NULL, NULL, NULL) == NULL)
        {
            OALMSG(OAL_ERROR, (L"ERROR: Failed to initialize NAND flash controller.\r\n"));
            return(FALSE);
        }

        // If block 0 isn't reserved, we can't trust that the values we read for the MAC address are
        // correct.  They may actually be valid logical sector numbers (we're overloading the use
        // of the logical sector number field).
        //
        if (!(FMD_GetBlockStatus(0) & BLOCK_STATUS_RESERVED))
        {
            OALMSG(OAL_ERROR, (L"ERROR: Block 0 isn't reserved - can't trust MAC address values stored in NAND.\r\n"));
            return(FALSE);
        }

        OALMSG(OAL_INFO, (L"INFO: Using KITL arguments stored on SmartMedia.\r\n"));
        memset(&args, 0, sizeof(args));
        args.flags = OAL_KITL_FLAGS_ENABLED | OAL_KITL_FLAGS_DHCP | OAL_KITL_FLAGS_VMINI;
        args.devLoc.IfcType = Internal;
        args.devLoc.BusNumber = 0;
        args.devLoc.LogicalLoc = BSP_BASE_REG_PA_CS8900A_IOBASE;
        args.ipAddress = 0;

        // We know the first block of NAND flash must be good, so we needn't worry about bad blocks when reading.
        //
        maccount = 0;
        do
        {
            if (!FMD_ReadSector(maccount, NULL, &si, 1))
            {
                OALMSG(OAL_ERROR, (L"ERROR: NAND flash read error (sector = 0x%x).\r\n", maccount));
                return(FALSE);
            }

            args.mac[maccount] = (UINT16)(si.dwReserved1 & 0xFFFF);

        } while(++maccount < 3);

        pArgs = &args;
    }        

    // If there isn't device id from bootloader create some.
    //
    if (szDeviceId == NULL)
    {
        OALKitlCreateName(BSP_DEVICE_PREFIX, args.mac, buffer);
        szDeviceId = buffer;
    }

    // Finally call KITL library.
    //
    rc = OALKitlInit(szDeviceId, pArgs, g_kitlDevices);

    OALMSG(OAL_KITL&&OAL_FUNC, (L"-OALKitlStart(rc = %d)\r\n", rc));
    return(rc);
}

DWORD OEMEthGetSecs (void)
{
    SYSTEMTIME st;
    DWORD dwRet;
    static DWORD dwBias;
    static DWORD dwLastTime;

    OEMGetRealTime( &st );
    dwRet = ((60UL * (60UL * (24UL * (31UL * st.wMonth + st.wDay) + st.wHour) + st.wMinute)) + st.wSecond);
    dwBias = dwRet;

    if (dwRet < dwLastTime) {
        KITLOutputDebugString("! Time went backwards (or wrapped): cur: %u, last %u\n",
                              dwRet,dwLastTime);
    }
    dwLastTime = dwRet;
    return (dwRet);
}


//------------------------------------------------------------------------------
//
//  Function:  OALGetTickCount
//
//  This function is called by some KITL libraries to obtain relative time
//  since device boot. It is mostly used to implement timeout in network
//  protocol.
//

UINT32 OALGetTickCount()
{
    return OEMEthGetSecs () * 1000;
}

// Define a dummy SetKMode function to satisfy the NAND FMD.
//
DWORD SetKMode (DWORD fMode)
{
    return(1);
}
//------------------------------------------------------------------------------

