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
//  File:  init.c
//
//  Samsung SMDK2410X board initialization code.
//
#include <bsp.h>

#include <romxip.h>

// global variables declared in ioctl.c
extern const volatile DWORD g_dwExtensionRAMFMDSize;
extern PVOID g_pvExtensionRAMFMDBaseAddr;

/*
    @func   void | InitDisplay | Initializes the LCD controller and displays a splashscreen image.
    @rdesc  N/A.
    @comm    
    @xref   
*/
static void InitDisplay(void)
{
    volatile S3C2410X_IOPORT_REG *s2410IOP = (S3C2410X_IOPORT_REG *)OALPAtoVA(S3C2410X_BASE_REG_PA_IOPORT, FALSE);
    volatile S3C2410X_LCD_REG    *s2410LCD = (S3C2410X_LCD_REG *)OALPAtoVA(S3C2410X_BASE_REG_PA_LCD, FALSE);
    unsigned int XSize;
    unsigned int YSize;
    unsigned int BppMode;
    BSP_ARGS *pBSPArgs = ((BSP_ARGS *) IMAGE_SHARE_ARGS_UA_START);                

    if (pBSPArgs->ScreenSignature == BSP_SCREEN_SIGNATURE)
    {
        YSize = pBSPArgs->ScreenHeight;
        XSize = pBSPArgs->ScreenWidth;
        switch (pBSPArgs->ScreenBitsPerPixel) {
        case 16:
        default:            // assume 16bpp if the value is unsupported
            BppMode = 12;
            break;
        case 24:
            BppMode = 13;
            break;
        case 32:
            BppMode = 14;
            break;
        }
        OALMSG(OAL_WARN, (TEXT("Using emulator-specified video parameters: %dx%dx%d\r\n"), XSize, YSize, pBSPArgs->ScreenBitsPerPixel));
    }
    else
    {
        YSize = LCD_YSIZE_TFT;
        XSize = LCD_XSIZE_TFT;
        BppMode = 12;       // 16bpp
    }

    // Set up the LCD controller registers to display a power-on bitmap image.
    //
    s2410IOP->GPCUP     = 0xFFFFFFFF;
    s2410IOP->GPCCON    = 0xAAAAAAAA;
                                    
    s2410IOP->GPDUP     = 0xFFFFFFFF;
    s2410IOP->GPDCON    = 0xAAAAAAAA; 

    s2410LCD->LCDCON1   =  (6           <<  8) |       /* VCLK = HCLK / ((CLKVAL + 1) * 2) -> About 7 Mhz  */
                           (LCD_MVAL   <<  7)  |       /* 0 : Each Frame                                   */
                           (3           <<  5) |       /* TFT LCD Pannel                                   */
                           (BppMode     <<  1) |       /* bpp Mode                                         */
                           (0           <<  0) ;       /* Disable LCD Output                               */

    s2410LCD->LCDCON2   =  (LCD_VBPD        << 24) |   /* VBPD          :   1                              */
                           ((YSize-1)       << 14) |   /* Vertical Size : 320 - 1                          */
                           (LCD_VFPD        <<  6) |   /* VFPD          :   2                              */
                           (LCD_VSPW        <<  0) ;   /* VSPW          :   1                              */

    s2410LCD->LCDCON3   =  (LCD_HBPD        << 19) |   /* HBPD          :   6                              */
                           ((XSize-1)       <<  8) |   /* HOZVAL_TFT    : 240 - 1                          */
                           (LCD_HFPD        <<  0) ;   /* HFPD          :   2                              */


    s2410LCD->LCDCON4   =  (LCD_MVAL        <<  8) |   /* MVAL          :  13                              */
                           (LCD_HSPW        <<  0) ;   /* HSPW          :   4                              */

    s2410LCD->LCDCON5   =  (0           << 12) |       /* BPP24BL       : LSB valid                        */
                           (1           << 11) |       /* FRM565 MODE   : 5:6:5 Format                     */
                           (0           << 10) |       /* INVVCLK       : VCLK Falling Edge                */
                           (1           <<  9) |       /* INVVLINE      : Inverted Polarity                */
                           (1           <<  8) |       /* INVVFRAME     : Inverted Polarity                */
                           (0           <<  7) |       /* INVVD         : Normal                           */
                           (0           <<  6) |       /* INVVDEN       : Normal                           */
                           (0           <<  5) |       /* INVPWREN      : Normal                           */
                           (0           <<  4) |       /* INVENDLINE    : Normal                           */
                           (0           <<  3) |       /* PWREN         : Disable PWREN                    */
                           (0           <<  2) |       /* ENLEND        : Disable LEND signal              */
                           (0           <<  1) |       /* BSWP          : Swap Disable                     */
                           (1           <<  0) ;       /* HWSWP         : Swap Enable                      */

    s2410LCD->LCDSADDR1 = ((IMAGE_FRAMEBUFFER_DMA_BASE >> 22)     << 21) | 
                          ((M5D(IMAGE_FRAMEBUFFER_DMA_BASE >> 1)) <<  0);

    s2410LCD->LCDSADDR2 = M5D((IMAGE_FRAMEBUFFER_DMA_BASE + (XSize * YSize * 2)) >> 1);

    s2410LCD->LCDSADDR3 = (((XSize - XSize) / 1) << 11) | (XSize / 1);        

    s2410LCD->LPCSEL   |= 0x3;

    s2410LCD->TPAL      = 0x0;        
    s2410LCD->LCDCON1  |= 1;

    // Display a blank image on the LCD...
    //
    memset((void*)IMAGE_FRAMEBUFFER_UA_BASE, 0, XSize*YSize*(BppMode-10));

}
 
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define	EXTENDED_RAM_BASE	0x94000000
#define	EXTENDED_RAM_MEGS	192		// 192 Meg's

BOOL 
OEMGetExtensionDRAM(
    LPDWORD lpMemStart, 
    LPDWORD lpMemLen
    ) 
{
    typedef volatile unsigned long MegOfExtendedRam_t[1024 * 1024 / sizeof (unsigned long)];
    MegOfExtendedRam_t *MegsOfRam = (MegOfExtendedRam_t *)EXTENDED_RAM_BASE;
    DWORD each_meg;
    const DWORD dwPageSize = 4096;	// PAGE_SIZE in ceddk isn't valid until later in boot

    OALMSG(OAL_FUNC, (L"++OEMGetExtensionDRAM\r\n"));

    // Employ a simple memory test to see that all N meg's are there.
    // NB: Because an empty memory buss can "float" data for several
    // cycles and appear as valid memory, discharge the bus before
    // verifying the data.
    try {
        for (each_meg = 0; each_meg < EXTENDED_RAM_MEGS; ++each_meg)
        {
            MegsOfRam[each_meg][0] = 0x55555555UL;	// Write pattern
            MegsOfRam[each_meg][1] = ~0x55555555UL;	// Discharge
            if (MegsOfRam[each_meg][0] != 0x55555555UL)	// Verify patern
                break;
            MegsOfRam[each_meg][0] = ~0x55555555UL;	// Write pattern-not
            MegsOfRam[each_meg][1] = 0x55555555UL;	// Discharge
            if (MegsOfRam[each_meg][0] != ~0x55555555UL)
                break;
        }
    } except (EXCEPTION_EXECUTE_HANDLER) {
        OALMSG(OAL_FUNC, (L"--OEMGetExtensionDRAM\r\n"));
        return FALSE; // no extension DRAM
    }

    *lpMemStart = EXTENDED_RAM_BASE;
    *lpMemLen = each_meg * sizeof (MegOfExtendedRam_t);
    OALMSG(OAL_LOG_INFO, (L"OEMGetExtensionDRAM: found 0x%08x bytes of ram at 0x%08x\r\n",
        *lpMemLen, *lpMemStart));

    // adjust by RAM FMD amount
    if(g_dwExtensionRAMFMDSize != 0) {
        if((g_dwExtensionRAMFMDSize & (dwPageSize - 1)) != 0) {
            OALMSG(OAL_LOG_WARN, 
                (L"OEMGetExtensionDRAM: g_dwExtensionRAMFMDSize 0x%08x not a multiple of %u\r\n",
                g_dwExtensionRAMFMDSize, dwPageSize));
        } else if(*lpMemLen < g_dwExtensionRAMFMDSize) {
            OALMSG(OAL_LOG_WARN, 
                (L"OEMGetExtensionDRAM: 0x%08x bytes of extension ram not enough to satisfy FMD request for 0x%0x bytes\r\n",
                *lpMemLen, g_dwExtensionRAMFMDSize));
        } else {
            g_pvExtensionRAMFMDBaseAddr = (PVOID) EXTENDED_RAM_BASE;
            *lpMemStart = EXTENDED_RAM_BASE + g_dwExtensionRAMFMDSize;
            *lpMemLen -= g_dwExtensionRAMFMDSize;
                OALMSG(OAL_LOG_INFO, (L"OEMGetExtensionDRAM: reserving 0x%08x bytes of ram at 0x%08x for RAMFMD\r\n",
                    g_dwExtensionRAMFMDSize, g_pvExtensionRAMFMDBaseAddr));
        }
    }

    OALMSG(OAL_LOG_INFO, (L"OEMGetExtensionDRAM: returning 0x%08x bytes of ram at 0x%08x\r\n",
        *lpMemLen, *lpMemStart));

    OALMSG(OAL_FUNC, (L"--OEMGetExtensionDRAM\r\n"));

    return TRUE;
}


//------------------------------------------------------------------------------
//
//  Function:  OEMInit
//
//  This is Windows CE OAL initialization function. It is called from kernel
//  after basic initialization is made.
//
void OEMInit()
{
    BSP_ARGS *pBSPArgs = ((BSP_ARGS *) IMAGE_SHARE_ARGS_UA_START);                
    OALMSG(OAL_FUNC, (L"+OEMInit\r\n"));

    // Set memory size for DrWatson kernel support
    dwNKDrWatsonSize = 128 * 1024;

    // Initialize cache globals
    OALCacheGlobalsInit();

    OALLogSerial(
        L"DCache: %d sets, %d ways, %d line size, %d size\r\n", 
        g_oalCacheInfo.L1DSetsPerWay, g_oalCacheInfo.L1DNumWays,
        g_oalCacheInfo.L1DLineSize, g_oalCacheInfo.L1DSize
    );
    OALLogSerial(
        L"ICache: %d sets, %d ways, %d line size, %d size\r\n", 
        g_oalCacheInfo.L1ISetsPerWay, g_oalCacheInfo.L1INumWays,
        g_oalCacheInfo.L1ILineSize, g_oalCacheInfo.L1ISize
    );

    // Check whether we should force a clean boot
    if(!pBSPArgs->EmulatorFlags.bit.fSoftReset) {
        OALMSG(OAL_INFO, (L"OEMInit: soft reset flag not set, forcing clean boot\r\n"));
        NKForceCleanBoot();
        pBSPArgs->EmulatorFlags.bit.fSoftReset = 1;        // assume future resets are soft unless told otherwise
        pBSPArgs->pvExtensionRAMFMDBaseAddr = NULL;
        pBSPArgs->dwExtensionRAMFMDSize = 0;
    }
    
    // Initialize the display, in case the emulator launched the kernel binary without eboot
    InitDisplay();

    // Initialize interrupts
    if (!OALIntrInit()) {
        OALMSG(OAL_ERROR, (
            L"ERROR: OEMInit: failed to initialize interrupts\r\n"
        ));
    }

    // Initialize system clock
    OALTimerInit(1, S3C2410X_PCLK/2000, 0);

    // Initialize the KITL connection if required
    OALKitlStart();

    OALMSG(OAL_FUNC, (L"-OEMInit\r\n"));
}

//------------------------------------------------------------------------------

#ifdef XIPIOCTL

#define NOT_FIXEDUP		   (DWORD*)-1
extern  ROMChain_t         *OEMRomChain;
DWORD *pdwXIPLoc = NOT_FIXEDUP;
DWORD dwDontUseChain = (DWORD)NOT_FIXEDUP;


/*
    @func   void | InitRomChain | Collects chain information for all image regions for the kernel.
    @rdesc  N/A.
    @comm    
    @xref   
*/
void InitRomChain(void)
{
	static		ROMChain_t	s_pNextRom[MAX_ROM] = {0};
	DWORD		dwRomCount = 0;
    DWORD       dwChainCount = 0;
    DWORD		*pdwCurXIP;
    DWORD       dwNumXIPs;
    PXIPCHAIN_ENTRY pChainEntry = NULL;

    if(pdwXIPLoc == NOT_FIXEDUP)
	{
        return;  // no chain or not fixed up properly
    }

    // set the top bit to mark it as a virtual address
    pdwCurXIP = (DWORD*)(((DWORD)pdwXIPLoc) | 0x80000000);

    // first DWORD is number of XIPs
    dwNumXIPs = (*pdwCurXIP);

    if(dwNumXIPs > MAX_ROM)
	{
      lpWriteDebugStringFunc(TEXT("ERROR: Number of XIPs exceeds MAX\n"));
      return;
    }

    pChainEntry = (PXIPCHAIN_ENTRY)(pdwCurXIP + 1);

    while(dwChainCount < dwNumXIPs)
    {
        if ((pChainEntry->usFlags & ROMXIP_OK_TO_LOAD) &&  // flags indicates valid XIP
            *(LPDWORD)(((DWORD)(pChainEntry->pvAddr)) + ROM_SIGNATURE_OFFSET) == ROM_SIGNATURE)
        {
            s_pNextRom[dwRomCount].pTOC = *(ROMHDR **)(((DWORD)(pChainEntry->pvAddr)) + ROM_SIGNATURE_OFFSET + 4);
            s_pNextRom[dwRomCount].pNext = NULL;

            if (dwRomCount != 0)
            {
                s_pNextRom[dwRomCount-1].pNext = &s_pNextRom[dwRomCount];
            }
            else
            {
                OEMRomChain = s_pNextRom;
            }
            dwRomCount++;
        }
        else
        {
            lpWriteDebugStringFunc( _T("Invalid XIP found\n") );
        }

        ++pChainEntry;
		dwChainCount++;
	}
}

#endif


