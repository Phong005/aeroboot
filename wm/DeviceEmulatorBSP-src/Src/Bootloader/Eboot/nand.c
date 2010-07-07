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
#include <windows.h>
#include <bsp.h>
#include "loader.h"
#include <fmd.h>

// Number of beginning SmartMedia (NAND) blocks to mark as reserved.
//
#define NAND_START_RESERVED_BLOCKS      3

BOOL g_bSmartMediaExist = FALSE;		// Is there a SmartMedia card on this device?


// Define a dummy SetKMode function to satisfy the NAND FMD.
//
DWORD SetKMode (DWORD fMode)
{
    return(1);
}


/*
    @func   BOOL | WriteImageToSmartMedia | Stores the image cached in RAM to the Smart Media card.  We assume that FMD_Init has already been called at this point.
    @rdesc  TRUE = Success, FALSE = Failure.
    @comm    
    @xref   
*/
BOOL WriteDiskImageToSmartMedia(DWORD dwImageStart, DWORD dwImageLength, BOOT_CFG *pBootCfg)
{
    UINT32 ulLogicalSector;
    FlashInfo FlashInfo;
    SectorInfo *pSectInfo;
    BYTE *pBuffer;
    UINT32 ulBadBlocks = 0;
    UINT32 ulImageLengthSectors = 0;
    UINT8 MACCount = 0;

    UINT16 uSectorSize = 0;
    UINT32 ulBlockSize  = 0;
    UINT32 ulSectorsPerBlock  = 0;

    // If there isn't a SmartMedia card in the system, it'll be kind of hard to write an image to it...
    //
    if (!g_bSmartMediaExist)
    {
        EdbgOutputDebugString("WARNING: Smart Media device doesn't exist - unable to store image.\r\n");
        return(FALSE);
    }

    // Format the SmartMedia card prior to writing.
    //
    EdbgOutputDebugString("INFO: Formatting SmartMedia (please wait)...\r\n");
    if (!FormatSmartMedia())
    {
        EdbgOutputDebugString("ERROR: Failed to format SmartMedia.\r\n");
        return(FALSE);
    }

    EdbgOutputDebugString("INFO: Writing download image to SmartMedia (please wait)...\r\n");

    // Get NAND flash info.
    //
    if (!FMD_GetInfo(&FlashInfo))
    {
        EdbgOutputDebugString("ERROR: Unable to get SmartMedia flash information.\r\n");
        return(FALSE);
    }
    uSectorSize = FlashInfo.wDataBytesPerSector;
    ulBlockSize  = FlashInfo.dwBytesPerBlock;
    ulSectorsPerBlock = (ulBlockSize / uSectorSize);

    // Write the image to NAND flash (starting at sector 0, skipping bad blocks).
    //
    // NOTE: we're going to assume that the image being written to SmartMedia has been generated
    // by the disk image utility.  As such, the first two flash blocks are reserved for the Steppingstone
    // loader and the IPL.  We'll mark these sectors reserved and start the logical sector numbering at
    // 0 on the following sector.  This shouldn't affect our ability to store/load a single NK solution
    // since the logical sector numbering and the reserved status won't matter to the ReadImageFromSmartMedia
    // routine (below).
    //
    ulImageLengthSectors = (dwImageLength / (uSectorSize + sizeof(SectorInfo)));

    for (ulLogicalSector = 0, ulBadBlocks = 0, MACCount = 0; ulLogicalSector < ulImageLengthSectors ; ulLogicalSector++)
    {
        // Compute the physical sector address (equal to logical sector address plus compensation for any bad blocks).
        UINT32 ulPhysicalSector = ((ulBadBlocks * ulSectorsPerBlock) + ulLogicalSector);
        UINT32 ulBlockNumber    = (ulPhysicalSector / ulSectorsPerBlock);

        // If we're writing the first sector of a new block and the block is marked bad, skip the whole block.
        while (!(ulPhysicalSector % ulSectorsPerBlock) && (FMD_GetBlockStatus(ulBlockNumber) == BLOCK_STATUS_BAD))
        {
            ulBadBlocks++;

            // Recompute the sector and block numbers based on bad blocks.
            ulPhysicalSector += ulSectorsPerBlock;
            ulBlockNumber++;
        }

        // Have we walked off the end of flash?
        if (ulPhysicalSector > (FlashInfo.dwNumBlocks * ulSectorsPerBlock))
        {
            EdbgOutputDebugString("ERROR: SmartMedia sector write would be out of bounds (physical sector = 0x%x  max sector = 0x%x).\r\n", ulPhysicalSector, (FlashInfo.dwNumBlocks * ulSectorsPerBlock));
            return(FALSE);
        }

        // update the buffer to the current sector
        pBuffer = (BYTE *)(dwImageStart + (ulLogicalSector * (uSectorSize + sizeof(SectorInfo))));

        // sector info immediately follows the current sector
        pSectInfo = (SectorInfo *) (pBuffer + uSectorSize);
        
        // For NAND-only boots, the NOR flash at nGCS0 isn't available for retrieving the CS8900A MAC
        // address (nor is there a EEPROM connected to the CS8900A where we could store this address).
        // We'll use the logical sector field for the first couple reserved block sectors to store
        // the address.
        //
        if ((MACCount < 3) && !(pSectInfo->bOEMReserved & OEM_BLOCK_RESERVED))
        {
            pSectInfo->dwReserved1 = pBootCfg->CS8900MAC[MACCount++];
        }

        // Write the sector to flash...
        //
        if (!FMD_WriteSector(ulPhysicalSector, pBuffer, pSectInfo, 1))
        {
            EdbgOutputDebugString("ERROR: Failed to write to SmartMedia (sector number 0x%x).\r\n", ulPhysicalSector);
            return(FALSE);
        }
    }

    EdbgOutputDebugString("INFO: Writing image to SmartMedia completed successfully.\r\n");
    return(TRUE);

}


/*
    @func   BOOLEAN | FormatSmartMedia | Performs a low-level format on the SmartMedia card.
    @rdesc  TRUE == Success and FALSE == Failure.
    @comm    
    @xref   
*/
BOOL FormatSmartMedia(void)
{
    UINT32 ulBlockNumber;
    FlashInfo FlashInfo;

    // Get NAND flash data bytes per sector.
    //
    if (!FMD_GetInfo(&FlashInfo))
    {
        EdbgOutputDebugString("ERROR: Unable to get SmartMedia flash information.\r\n");
        return(FALSE);
    }

    for (ulBlockNumber = 0 ; ulBlockNumber < FlashInfo.dwNumBlocks ; ulBlockNumber++)
    {
        // Is the block bad?
        //
        if (FMD_GetBlockStatus(ulBlockNumber) == BLOCK_STATUS_BAD)
        {
            EdbgOutputDebugString("INFO: Found bad SmartMedia block [0x%x].\r\n", ulBlockNumber);
            continue;
        }

        // Erase the block...
        //
        if (!FMD_EraseBlock(ulBlockNumber))
        {
            EdbgOutputDebugString("ERROR: Unable to erase SmartMedia block 0x%x.\r\n", ulBlockNumber);
            return(FALSE);
        }
    }

    return(TRUE);
}
