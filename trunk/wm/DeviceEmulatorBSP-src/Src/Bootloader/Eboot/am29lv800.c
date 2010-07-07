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

// AMD AM29LV800BB flash manufacturer and device IDs.
//
#define AMD_MFG_CODE			0x01
#define AMD_DEV_CODE			0x225B	// We only support the AMD29LV800BB.

// AMD flash commands.
//
#define AMD_CMD_RESET			0xF0F0
#define AMD_CMD_AUTOSEL			0x9090
#define AMD_CMD_PROGRAM			0xA0A0
#define AMD_CMD_UNLOCK1			0xAAAA
#define AMD_CMD_UNLOCK2			0x5555
#define AMD_CMD_SECTERASE		0x8080
#define AMD_CMD_SECTERASE_CONFM 0x3030

// AMD flash access macros.
//
#define AMD_WRITE_USHORT(addr, data)	*((volatile unsigned short *)(addr)) = (data)
#define AMD_READ_USHORT(addr)           *((volatile unsigned short *)(addr))

#define AMD_CMD_ADDR					(AMD_FLASH_START + (0x5555 << 1))
#define AMD_UNLOCK_ADDR1				(AMD_FLASH_START + (0x5555 << 1))
#define AMD_UNLOCK_ADDR2				(AMD_FLASH_START + (0x2AAA << 1))

#define AMD_UNLOCK_CHIP()				AMD_WRITE_USHORT(AMD_UNLOCK_ADDR1, AMD_CMD_UNLOCK1); \
										AMD_WRITE_USHORT(AMD_UNLOCK_ADDR2, AMD_CMD_UNLOCK2)

#define AMD_WRITE_CMD(cmd)      		AMD_WRITE_USHORT(AMD_CMD_ADDR, cmd)


/*
    @func   BOOL | AMD28LV800_Wait | Checks for BIT6 to toggle (indicates flash has completed command).
    @rdesc  TRUE = Bit6 toggled, FASE = Bit6 didn't toggle.
    @comm    
    @xref   
*/
static BOOL AM29LV800_Wait(void) //Check if the bit6 toggle ends.
{
	volatile USHORT Status, OldStatus;

	OldStatus = *((volatile unsigned short *)AMD_FLASH_START);

	while(1)
	{
		Status = *((volatile unsigned short *)AMD_FLASH_START);
		if ((OldStatus & 0x40) == (Status & 0x40))
			break;
		if(Status & 0x20)
		{
			OldStatus = *((volatile unsigned short *)AMD_FLASH_START);
			Status    = *((volatile unsigned short *)AMD_FLASH_START);
			if((OldStatus & 0x40) == (Status & 0x40))
				return(FALSE);
			else
				return(TRUE);
		}
		OldStatus = Status;
	}
	return(TRUE);
}


/*
    @func   BOOL | AM29LV800_Init | Initialize the AMD AM29LV800BB flash.
    @rdesc  TRUE = Success, FALSE = Failure.
    @comm    
    @xref   
*/
BOOL AM29LV800_Init(UINT32 dwAddr)
{
    UINT16 MfgCode = 0;
    UINT16 DevCode = 0;
    BOOLEAN bResult = FALSE;

    OALMSG(OAL_FUNC, (TEXT("+AM29LV800_Init.\r\n")));

    // Put part in autoselect mode so we can identify it.
    //
    AMD_WRITE_CMD(AMD_CMD_RESET);
    AMD_UNLOCK_CHIP();
    AMD_WRITE_CMD(AMD_CMD_AUTOSEL);

    // Read the manufacturer and device codes.
    //
    MfgCode = AMD_READ_USHORT(dwAddr);
    DevCode = AMD_READ_USHORT(dwAddr + 2);

    // Is this the expected AMD flash part?
    //
    if (MfgCode != AMD_MFG_CODE || DevCode != AMD_DEV_CODE)
    {
        OALMSG(OAL_ERROR, (TEXT("ERROR: AM29LV800_Init: Bad manufacturer or device code: Mfg=0x%x, Dev=0x%x.\r\n"), MfgCode, DevCode));
        goto CleanUp;
    }

    // Reset the part into normal operating mode.
    //
    AMD_WRITE_CMD(AMD_CMD_RESET);

    bResult = TRUE;

CleanUp:

    OALMSG(OAL_FUNC, (TEXT("-AM29LV800_Init.\r\n")));
    return(bResult);

}


/*
    @func   UINT32 | GetSectorSize | Returns the size of the specified sector.  
	                                Note that the AMD AM29LV800BB is a "bottom" boot block non-uniform sector size part.
    @rdesc  Sector size for the specified sector.
    @comm    
    @xref   
*/
static UINT32 GetSectorSize(UINT8 SectorNumber)
{
    switch(SectorNumber)
    {
    case 0:
        return(16 * 1024);
    case 1:
        return(8 * 1024);
    case 2:
        return(8 * 1024);
    case 3:
        return(32 * 1024);
    default:
        return(64 * 1024);
    }

    return(0);
}


/*
    @func   UINT32 | GetSectorAddress | Returns the starting address (flash byte offset) for the specified sector number.
	                                   Note that the AMD AM29LV800BB is a "bottom" boot block non-uniform sector size part.
    @rdesc  Starting address of the specified sector.
    @comm    
    @xref   
*/
static UINT32 GetSectorAddress(UINT8 SectorNumber)
{
	UINT32 dwSectorAddr = 0;

    switch(SectorNumber)
    {
    case 0:
        dwSectorAddr = 0;
		break;
    case 1:
        dwSectorAddr = 0x4000;
		break;
    case 2:
        dwSectorAddr = 0x6000;
		break;
    case 3:
        dwSectorAddr = 0x8000;
		break;
    default:
        dwSectorAddr = ((SectorNumber - 3) * 0x10000);
		break;
    }

    return(AMD_FLASH_START + dwSectorAddr);
}


/*
    @func   UINT8 | GetSectorNumber | Returns the sector number corresponding to the flash address specified.
	                                   Note that the AMD AM29LV800BB is a "bottom" boot block non-uniform sector size part.
    @rdesc  Sector number.
    @comm    
    @xref   
*/
static UINT8 GetSectorNumber(UINT32 dwOffset)
{
	UINT8 SectorNumber = 0;

	if (dwOffset < 0x4000)
	{
		SectorNumber = 0;
	}
	else if (dwOffset < 0x6000)
	{
		SectorNumber = 1;
	}
	else if (dwOffset < 0x8000)
	{
		SectorNumber = 2;
	}
	else if (dwOffset < 0x10000)
	{
		SectorNumber = 3;
	}
	else
	{
		SectorNumber = 4 + (UCHAR)((dwOffset - 0x10000) / (64 * 1024));
	}

    return(SectorNumber);
}


/*
    @func   BOOL | EraseSector | Erases the specified flash sector.
    @rdesc  TRUE = Success, FALSE = Failure.
    @comm    
    @xref   
*/
static BOOL EraseSector(UINT8 dwSect)
{
    AMD_UNLOCK_CHIP();
    AMD_WRITE_CMD(AMD_CMD_SECTERASE);
    AMD_UNLOCK_CHIP();
    AMD_WRITE_USHORT(GetSectorAddress(dwSect), AMD_CMD_SECTERASE_CONFM);
	AM29LV800_Wait();

	return(TRUE);
}


/*
    @func   BOOL | AM29LV800_WriteFlash | Writes the specified data to AMD AM29LV800 flash starting at the sector offset specified.
    @rdesc  TRUE = Success, FALSE = Failure.
    @comm    
    @xref   
*/
BOOL AM29LV800_WriteFlash(UINT32 dwOffset, PBYTE pData, UINT32 dwLength)
{
    UINT32 dwByteOffset;
    
	RETAILMSG(1, (TEXT("AM29LV800_WriteFlash: Writing 0x%x bytes to flash (offset 0x%x bytes)...\r\n"), dwLength, dwOffset));

    for (dwByteOffset = 0 ; dwByteOffset < dwLength ; dwByteOffset += sizeof(USHORT))
    {
		AMD_UNLOCK_CHIP();
		AMD_WRITE_CMD(AMD_CMD_PROGRAM);
        AMD_WRITE_USHORT((AMD_FLASH_START + dwOffset + dwByteOffset), *(PUSHORT)(pData + dwByteOffset));
		AM29LV800_Wait();
    }

	return(TRUE);
}


/*
    @func   BOOL | AM29LV800_ReadFlash | Reads the specified number of bytes from flash starting at the specified offset.
    @rdesc  TRUE = Success, FALSE = Failure.
    @comm    
    @xref   
*/
BOOL AM29LV800_ReadFlash(UINT32 dwOffset, PBYTE pData, UINT32 dwLength)
{
	PBYTE pReadPtr = (PBYTE)(AMD_FLASH_START + dwOffset);
    
	//RETAILMSG(1, (TEXT("AM29LV800_ReadFlash: Reading 0x%x bytes from flash (offset 0x%x bytes)...\r\n"), dwLength, dwOffset));

	memcpy(pData, pReadPtr, dwLength);

	return(TRUE);
}


/*
    @func   BOOL | AM29LV800_EraseFlash | Erases AMD AM29LV800 flash starting at the sector offset specified.
    @rdesc  TRUE = Success, FALSE = Failure.
    @comm    
    @xref   
*/
BOOL AM29LV800_EraseFlash(UINT32 dwOffset, UINT32 dwLength)
{
    UINT8 NumSects = 0;
    UINT32 dwEraseLength = 0;
	   UINT8 StartSector = GetSectorNumber(dwOffset);

    while(dwEraseLength < dwLength)
    {
        dwEraseLength += GetSectorSize(StartSector + NumSects);
        ++NumSects;
    }

	if (!NumSects)
	{
		NumSects = 1;
        dwEraseLength = GetSectorSize(StartSector);
	}

    RETAILMSG(1, (TEXT("AM29LV800_EraseFlash: Requested 0x%x bytes, Erasing 0x%x sectors (0x%x bytes) (offset 0x%x bytes)...\r\n"), dwLength, NumSects, dwEraseLength, dwOffset)); 

    AMD_WRITE_CMD(AMD_CMD_RESET);

    while(NumSects--)
    {
        if (!EraseSector(StartSector + NumSects))
        {
            RETAILMSG(1, (TEXT("ERROR: AM29LV800_EraseFlash: EraseSector failed on sector 0x%x!\r\n"), NumSects));
            return(FALSE);
        }
    }
    AMD_WRITE_CMD(AMD_CMD_RESET);

	return(TRUE);
}

