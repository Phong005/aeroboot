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

extern IMAGE_TYPE    g_ImageType;

/*
    @func   BOOL | OEMIsFlashAddr | Tests whether the address provided resides in the Samsung's flash.
    @rdesc  TRUE = Specified address resides in flash, FALSE = Specified address doesn't reside in flash.
    @comm    
    @xref   
*/
BOOL OEMIsFlashAddr(DWORD dwAddr)
{
    // Does the specified address fall in the AMD flash address range?
    //
    if ((dwAddr >= AMD_FLASH_START) && (dwAddr < (AMD_FLASH_START + AMD_FLASH_LENGTH)))
    {
        return(TRUE);
    }

    return(FALSE);
}


/*
    @func   LPBYTE | OEMMapMemAddr | Remaps a specified address to a file cache location.  The file cache is used as a temporary store for flash images before they're written to flash.
    @rdesc  Corresponding address within a file cache area.
    @comm    
    @xref   
*/
LPBYTE OEMMapMemAddr(DWORD dwImageStart, DWORD dwAddr)
{
    UINT32 MappedAddr = dwAddr;

    OALMSG(OAL_FUNC, (TEXT("+OEMMapMemAddr (Address=0x%x).\r\n"), dwAddr));

    // If it's a flash address, translate it by "rebasing" the address into the file cache area.
    //
    if (OEMIsFlashAddr(dwAddr) && (dwImageStart <= dwAddr))
    {
        MappedAddr = (FILE_CACHE_START + (dwAddr - dwImageStart));
    }
    else if ((g_ImageType == IMAGE_LOADER) && (dwAddr >= EBOOT_RAM_IMAGE_BASE))
    {
        // Special case for bootloader - it's destined for flash, cached in the file cache area, but fixed-up to run from yet another RAM area (the romheader is "located" here).
        //
        MappedAddr = (FILE_CACHE_START + (dwAddr - EBOOT_RAM_IMAGE_BASE));
    }

    OALMSG(OAL_FUNC, (TEXT("-OEMMapMemAddr (Address=0x%x.\r\n"), MappedAddr));

    return((LPBYTE)MappedAddr);
}


/*
    @func   BOOL | OEMStartEraseFlash | Called at the start of image download, this routine begins the flash erase process.
    @rdesc  TRUE = Success, FALSE = Failure.
    @comm    
    @xref   
*/
BOOL OEMStartEraseFlash(DWORD dwStartAddr, DWORD dwLength)
{
    // Nothing to do (erase done in OEMWriteFlash)...
    //
    return(TRUE);
}


/*
    @func   void | OEMContinueEraseFlash | Called frequenty during image download, this routine continues the flash erase process.
    @rdesc  N/A.
    @comm    
    @xref   
*/
void OEMContinueEraseFlash(void)
{
    // Nothing to do (erase done in OEMWriteFlash)...
    //
}


/*
    @func   BOOL | OEMFinishEraseFlash | Called following the image download, this routine completes the flash erase process.
    @rdesc  TRUE = Success, FALSE = Failure.
    @comm    
    @xref   
*/
BOOL OEMFinishEraseFlash(void)
{
    // Nothing to do (erase done in OEMWriteFlash)...
    //
    return(TRUE);
}


/*
    @func   BOOL | OEMWriteFlash | Writes data to flash (the source location is determined using OEMMapMemAddr).
    @rdesc  TRUE = Success, FALSE = Failure.
    @comm    
    @xref   
*/
BOOL OEMWriteFlash(DWORD dwStartAddr, DWORD dwLength)
{

    // Are we updating the bootloader in AMD flash?
    //
    if (g_ImageType == IMAGE_LOADER)
    {
        PBYTE pCachedData = OEMMapMemAddr(dwStartAddr, dwStartAddr);

        // Erase AMD flash.
        //
        if (!AM29LV800_EraseFlash(EBOOT_STORE_OFFSET, dwLength))
        {
            RETAILMSG(1, (TEXT("ERROR: OEMWriteFlash: Flash erase failed.\r\n")));
            return(FALSE);
        }

        // Write flash image.
        //
        if (!AM29LV800_WriteFlash(EBOOT_STORE_OFFSET, pCachedData, dwLength))
        {
            RETAILMSG(1, (TEXT("ERROR: OEMWriteFlash: Flash write failed.\r\n")));
            return(FALSE);
        }

        if (!memcmp((const void *)EBOOT_STORE_ADDRESS, (const void *)pCachedData, dwLength))
        {
            RETAILMSG(1, (TEXT("\r\n*** Bootloader updated - please reboot system. ***\r\n")));

            // Update LEDs to indicate that flash write is done.
            //
            OEMWriteDebugLED(0, 0);

            while (1);
        }
        else
        {
            RETAILMSG(1, (TEXT("ERROR: OEMWriteFlash: Flash verification failed.\r\n")));
            return(FALSE);
        }

    }

    RETAILMSG(1, (TEXT("ERROR: OEMWriteFlash: Unknown image type.\r\n")));

    return(FALSE);
}

