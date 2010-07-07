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
#include <fmd.h>
#include <s3c2410x_nand.h>
#include "nand.h"
#include <ethdbg.h>

#define NAND_BASE 0xB0E00000

static volatile S3C2410X_NAND_REG *s2410NAND = (S3C2410X_NAND_REG *)NAND_BASE;

extern "C" void RdPage512(unsigned char *bufPt);
extern "C" void RdPage512Unalign(unsigned char *bufPt);
extern "C" void WrPage512(unsigned char *bufPt); 
extern "C" void WrPage512Unalign(unsigned char *bufPt); 
extern "C" void WrPageInfo(PBYTE pBuff);
extern "C" void RdPageInfo(PBYTE pBuff);

/*
    @func   DWORD | ReadFlashID | Reads the flash manufacturer and device codes.
    @rdesc  Manufacturer and device codes.
    @comm    
    @xref   
*/
static DWORD ReadFlashID(void)
{
    BYTE Mfg, Dev;

    NF_CMD(CMD_READID);        // Send flash ID read command.
    NF_ADDR(0);                //
    NF_WAITRB();            // Wait for flash to complete command.
    Mfg    = NF_RDDATA();    // 
    Dev    = NF_RDDATA();    // 

    //RETAILMSG(1,(TEXT("FMD: ReadID (Mfg=%x, Dev=%x)\r\n"), Mfg, Dev));

    return ((DWORD)Mfg*0x100+Dev);
}

/*
    @func   PVOID | FMD_Init | Initializes the Smart Media NAND flash controller.
    @rdesc  Pointer to S3C2410 NAND controller registers.
    @comm    
    @xref   
*/
PVOID FMD_Init(LPCTSTR lpActiveReg, PPCI_REG_INFO pRegIn, PPCI_REG_INFO pRegOut)
{

    // Caller should have specified NAND controller address.
    //

    BOOL bLastMode = SetKMode(TRUE);

    if (pRegIn && pRegIn->MemBase.Num && pRegIn->MemBase.Reg[0])
        s2410NAND = (S3C2410X_NAND_REG *)(pRegIn->MemBase.Reg[0]);
    else
        s2410NAND = (S3C2410X_NAND_REG *)NAND_BASE;


    // Set up initial flash controller configuration.
    //
    s2410NAND->NFCONF =  (1      << 15) | /* Enable/Disable                    */ 
                          (1      << 14) | /* Page Size : 512Bytes              */
                          (1      << 13) | /* 4 Step Address                    */
                          (1      << 12) | /* Initialize ECC                    */
                          (1      << 11) | /* nFCE control nFCE = HIGH          */
                          (TACLS  <<  8) | /* CLE & ALE = HCLK * (TACLS  + 1)   */
                          (TWRPH0 <<  4) | /* TWRPH0    = HCLK * (TWRPH0 + 1)   */
                          (TWRPH1 <<  0);  /* TWRPH1    = HCLK * (TWRPH1 + 1)   */

    NF_nFCE_L();            // Select the flash chip.
    NF_CMD(CMD_RESET);        // Send reset command.
    NF_WAITRB();            // Wait for flash to complete command.
    // Get manufacturer and device codes.
    if (ReadFlashID() != 0xEC76)
    {
        SetKMode (bLastMode);
        return(NULL);
    }
    NF_nFCE_H();            // Deselect the flash chip.

    SetKMode (bLastMode);
    return((PVOID)s2410NAND);
}


/*
    @func   BOOL | FMD_ReadSector | Reads the specified sector(s) from NAND flash.
    @rdesc  TRUE = Success, FALSE = Failure.
    @comm    
    @xref   
*/
BOOL FMD_ReadSector(SECTOR_ADDR startSectorAddr, LPBYTE pSectorBuff, PSectorInfo pSectorInfoBuff, DWORD dwNumSectors)
{
    ULONG SectorAddr = (ULONG)startSectorAddr;

    BOOL bLastMode = SetKMode(TRUE);

    NF_RSTECC();                            // Initialize ECC.
    NF_nFCE_L();                            // Select the flash chip.
    NF_CMD(CMD_RESET);                        // Send reset command.
    
    while (dwNumSectors--)
    {
        ULONG blockPage = (((SectorAddr / NAND_PAGE_CNT) * NAND_PAGE_CNT) | (SectorAddr % NAND_PAGE_CNT));

        NF_WAITRB();                            // Wait for flash to complete command.

        if (pSectorBuff)
        {
            NF_CMD(CMD_READ);                    // Send read command.
            NF_ADDR(0);                            // Column = 0.
            NF_ADDR(blockPage         & 0xff);    // Page address.
            NF_ADDR((blockPage >>  8) & 0xff);
            NF_ADDR((blockPage >> 16) & 0xff);  
            NF_WAITRB();                        // Wait for command to complete.

            //  Handle unaligned buffer pointer
            if( ((DWORD) pSectorBuff) & 0x3) 
            {
                RdPage512Unalign (pSectorBuff);
            }
            else 
            {
                RdPage512(pSectorBuff);                // Read page/sector data.
            }
            
            NF_RDDATA();                        // Read/clear status.
            NF_RDDATA();                        //
        }

        if (pSectorInfoBuff)
        {
            NF_CMD(CMD_READ2);                    // Send read confirm command.
            NF_ADDR(0);                            // Column = 0.
            NF_ADDR(blockPage         & 0xff);    // Page address.
            NF_ADDR((blockPage >>  8) & 0xff);
            NF_ADDR((blockPage >> 16) & 0xff);  
            NF_WAITRB();                        // Wait for command to complete.

            RdPageInfo((PBYTE)pSectorInfoBuff);    // Read page/sector information.

            NF_RDDATA();                        // Read/clear status.
            NF_RDDATA();                        //

            pSectorInfoBuff++;
        }

        ++SectorAddr;
        pSectorBuff += NAND_PAGE_SIZE;
    }

    NF_nFCE_H();                             // Deselect the flash chip.

    SetKMode (bLastMode);
    return(TRUE);
}


/*
    @func   BOOL | FMD_EraseBlock | Erases the specified flash block.
    @rdesc  TRUE = Success, FALSE = Failure.
    @comm    
    @xref   
*/
BOOL FMD_EraseBlock(BLOCK_ID blockID)
{
    BYTE Status;
    ULONG blockPage = (blockID * NAND_PAGE_CNT);    // Convert block address to page address.

    BOOL bLastMode = SetKMode(TRUE);

    NF_nFCE_L();                        // Select the flash chip.
    NF_CMD(CMD_ERASE);                    // Send block erase command.
    NF_ADDR(blockPage         & 0xff);    /* The mark of bad block is in 0 page   */
    NF_ADDR((blockPage >>  8) & 0xff);  /* For block number A[24:17]            */
    NF_ADDR((blockPage >> 16) & 0xff);  /* For block number A[25]               */
    NF_CMD(CMD_ERASE2);                    // Send block erase confirm command.
    NF_WAITRB();                        // Wait for flash to complete command.
    
    Status = NF_RDDATA();                // Read command status.
    NF_nFCE_H();                        // Deselect the flash chip.

    SetKMode (bLastMode);
    return((Status & 1) ? FALSE : TRUE);
}


/*
    @func   BOOL | FMD_WriteSector | Writes the specified data to the specified NAND flash sector/page.
    @rdesc  TRUE = Success, FALSE = Failure.
    @comm    
    @xref   
*/
BOOL FMD_WriteSector(SECTOR_ADDR startSectorAddr, LPBYTE pSectorBuff, PSectorInfo pSectorInfoBuff, DWORD dwNumSectors)
{
    BYTE Status;
    ULONG SectorAddr = (ULONG)startSectorAddr;

    BOOL bLastMode = SetKMode(TRUE);

    if (!pSectorBuff && !pSectorInfoBuff)
        return(FALSE);

    NF_RSTECC();                            // Initialize ECC.
    NF_nFCE_L();                             // Select the flash chip.

    while (dwNumSectors--)
    {
        ULONG blockPage = (((SectorAddr / NAND_PAGE_CNT) * NAND_PAGE_CNT) | (SectorAddr % NAND_PAGE_CNT));

        if (!pSectorBuff)
        {
            // If we are asked just to write the SectorInfo, we will do that separately
            NF_CMD(CMD_READ2);                     // Send read command.
            NF_CMD(CMD_WRITE);                    // Send write command.
            NF_ADDR(0);                            // Column = 0.
            NF_ADDR(blockPage         & 0xff);    // Page address.
            NF_ADDR((blockPage >>  8) & 0xff);
            NF_ADDR((blockPage >> 16) & 0xff);  
            WrPageInfo((PBYTE)pSectorInfoBuff);
            pSectorInfoBuff++;
        }
        else 
        {
            NF_CMD(CMD_READ);                     // Send read command.
            NF_CMD(CMD_WRITE);                    // Send write command.
            NF_ADDR(0);                            // Column = 0.
            NF_ADDR(blockPage         & 0xff);    // Page address.
            NF_ADDR((blockPage >>  8) & 0xff);
            NF_ADDR((blockPage >> 16) & 0xff);  

            //  Special case to handle un-aligned buffer pointer.
            if( ((DWORD) pSectorBuff) & 0x3) 
            {
                WrPage512Unalign (pSectorBuff);
            }
            else
            {
                WrPage512(pSectorBuff);                // Write page/sector data.
            }

            // Write the SectorInfo data to the media.
            //
            if(pSectorInfoBuff)
            {
                WrPageInfo((PBYTE)pSectorInfoBuff);
                pSectorInfoBuff++;

            }
            else    // Make sure we advance the Flash's write pointer (even though we aren't writing the SectorInfo data)
            {
                BYTE TempInfo[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
                WrPageInfo(TempInfo);
            }
            pSectorBuff += NAND_PAGE_SIZE;
        }
            
        NF_CMD(CMD_WRITE2);                    // Send write confirm command.
        NF_WAITRB();                        // Wait for command to complete.

        Status = NF_RDDATA();
        if (Status & 1)
        {
            SetKMode (bLastMode);
            // EdbgOutputDebugString("ERROR: FMD_WriteSector: failed sector write.\r\n");
            return(FALSE);
        }

        ++SectorAddr;
    }

    NF_nFCE_H();                            // Deselect the flash chip.

    SetKMode (bLastMode);
    return(TRUE);
}


VOID FMD_PowerUp(VOID)
{
}


VOID FMD_PowerDown(VOID)
{
}

BOOL FMD_OEMIoControl(DWORD dwIoControlCode, PBYTE pInBuf, DWORD nInBufSize, PBYTE pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned)
{
    return(TRUE);
}

BOOL FMD_Deinit(PVOID hFMD)
{
    return(TRUE);
}


/*
    @func   BOOL | FMD_GetInfo | Provides information on the NAND flash.
    @rdesc  TRUE = Success, FALSE = Failure.
    @comm    
    @xref   
*/
BOOL FMD_GetInfo(PFlashInfo pFlashInfo)
{

    if (!pFlashInfo)
        return(FALSE);

    pFlashInfo->flashType            = NAND;
    pFlashInfo->wDataBytesPerSector = NAND_PAGE_SIZE;
    pFlashInfo->dwNumBlocks         = NAND_BLOCK_CNT;
    pFlashInfo->wSectorsPerBlock    = NAND_PAGE_CNT;
    pFlashInfo->dwBytesPerBlock        = (pFlashInfo->wSectorsPerBlock * pFlashInfo->wDataBytesPerSector);

    return(TRUE);
}


static BOOL IsBlockBad(BLOCK_ID blockID)
{
    BYTE Data;
    SECTOR_ADDR blockPage = (blockID * NAND_PAGE_CNT);

    BOOL bLastMode = SetKMode(TRUE);

    NF_nFCE_L();            // Select the flash chip.
    NF_CMD(CMD_RESET);        // Send reset command.
    NF_WAITRB();            // Wait for flash to complete command.
    NF_CMD(CMD_READ2);        // Send read confirm command.
    NF_ADDR(0);                            // Column = 0.
    NF_ADDR(blockPage         & 0xff);    /* The mark of bad block is in 0 page   */
    NF_ADDR((blockPage >>  8) & 0xff);  /* For block number A[24:17]            */
    NF_ADDR((blockPage >> 16) & 0xff);  /* For block number A[25]               */
    NF_WAITRB();            // Wait for flash to complete command.

    // TODO
    Data = NF_RDDATA();        // Read command status.
    Data = NF_RDDATA();        // Read command status.
    Data = NF_RDDATA();        // Read command status.
    Data = NF_RDDATA();        // Read command status.
    Data = NF_RDDATA();        // Read command status.
    Data = NF_RDDATA();        // Read command status.

    if(0xff != Data)
    {
        SetKMode (bLastMode);
        return(TRUE);
    }

    NF_nFCE_H();            // Deassert the flash chip.

    SetKMode (bLastMode);
   
    return(FALSE);
}


/*
    @func   DWORD | FMD_GetBlockStatus | Returns the status of the specified block.
    @rdesc  Block status (see fmd.h).
    @comm    
    @xref   
*/
DWORD FMD_GetBlockStatus(BLOCK_ID blockID)
{
    SECTOR_ADDR Sector = (blockID * NAND_PAGE_CNT);
    SectorInfo SI;
    DWORD dwResult = 0;
    
    if (IsBlockBad(blockID))
        return BLOCK_STATUS_BAD;

    if (!FMD_ReadSector(Sector, NULL, &SI, 1)) 
        return BLOCK_STATUS_UNKNOWN;

    if (!(SI.bOEMReserved & OEM_BLOCK_READONLY))  
        dwResult |= BLOCK_STATUS_READONLY;

    if (!(SI.bOEMReserved & OEM_BLOCK_RESERVED))  
        dwResult |= BLOCK_STATUS_RESERVED;

    return(dwResult);
}


/*
    @func   BOOL | MarkBlockBad | Marks the specified block as bad.
    @rdesc  TRUE = Success, FALSE = Failure.
    @comm    
    @xref   
*/
static BOOL MarkBlockBad(BLOCK_ID blockID)
{
    BYTE Status;
    ULONG blockPage = (blockID * NAND_PAGE_CNT);    // Convert block address to page address.

    BOOL bLastMode = SetKMode(TRUE);

    NF_nFCE_L();            // Select the flash chip.
    NF_CMD(CMD_RESET);        // Send reset command.
    NF_WAITRB();            // Wait for flash to complete command.
    NF_CMD(CMD_READ2);        // Send read confirm command.
    NF_CMD(CMD_WRITE);        // Send write command.
    NF_ADDR(0);                            // Column = 0.
    NF_ADDR(blockPage         & 0xff);    /* The mark of bad block is in 0 page   */
    NF_ADDR((blockPage >>  8) & 0xff);  /* For block number A[24:17]            */
    NF_ADDR((blockPage >> 16) & 0xff);  /* For block number A[25]               */

    // TODO
    NF_WRDATA(0xFF);            // Write bad block marker.
    NF_WRDATA(0xFF);            // Write bad block marker.
    NF_WRDATA(0xFF);            // Write bad block marker.
    NF_WRDATA(0xFF);            // Write bad block marker.
    NF_WRDATA(0xFF);            // Write bad block marker.

    NF_WRDATA(0);            // Write bad block marker.
    NF_CMD(CMD_WRITE2);        // Send write confirm command.
    NF_WAITRB();            // Wait for flash to complete command.

    Status = NF_RDDATA();    // Read command status.
    NF_nFCE_H();            // Deassert the flash chip.

    SetKMode (bLastMode);
    return((Status & 1) ? FALSE : TRUE);
}

/*
    @func   BOOL | FMD_SetBlockStatus | Marks the block with the specified block status.
    @rdesc  TRUE = Success, FALSE = Failure.
    @comm    
    @xref   
*/
BOOL FMD_SetBlockStatus(BLOCK_ID blockID, DWORD dwStatus)
{
    if (dwStatus & BLOCK_STATUS_BAD)
    {
        if (!MarkBlockBad(blockID))
            return(FALSE);
    }

    if (dwStatus & (BLOCK_STATUS_READONLY | BLOCK_STATUS_RESERVED)) {
        
        SECTOR_ADDR Sector = blockID * NAND_PAGE_CNT;
        SectorInfo SI;

        if (!FMD_ReadSector(Sector, NULL, &SI, 1)) {
            return FALSE;
        }

        if (dwStatus & BLOCK_STATUS_READONLY) {
            SI.bOEMReserved &= ~OEM_BLOCK_READONLY;
        }
        
        if (dwStatus & BLOCK_STATUS_RESERVED) {
            SI.bOEMReserved &= ~OEM_BLOCK_RESERVED;
        }

        if (!FMD_WriteSector (Sector, NULL, &SI, 1)) {
            return FALSE;
        }

    }    
    
    return(TRUE);
}

