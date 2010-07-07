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
#include <pcireg.h>
#include <ethdbg.h>
#include <fmd.h>
#include "loader.h"


char *inet_ntoa(DWORD dwIP);
DWORD inet_addr( char *pszDottedD );
BOOL EbootInitEtherTransport (EDBG_ADDR *pEdbgAddr, LPDWORD pdwSubnetMask,
                              BOOL *pfJumpImg,
                              DWORD *pdwDHCPLeaseTime,
                              UCHAR VersionMajor, UCHAR VersionMinor,
                              char *szPlatformString, char *szDeviceName,
                              UCHAR CPUId, DWORD dwBootFlags);
BOOL EbootEtherReadData (DWORD cbData, LPBYTE pbData);
EDBG_OS_CONFIG_DATA *EbootWaitForHostConnect (EDBG_ADDR *pDevAddr, EDBG_ADDR *pHostAddr);
// End   ***************************************


// Globals
//
DWORD EdbgDebugZone;
static BOOLEAN g_bWaitForConnect = TRUE;

IMAGE_TYPE     g_ImageType       = IMAGE_OS_BIN;
BOOT_CFG       g_BootConfig;

EDBG_ADDR g_DeviceAddr; // NOTE: global used so it remains in scope throughout download process
                        // since eboot library code keeps a global pointer to the variable provided.

// External definitions.
//
extern const BYTE ScreenBitmap[];
extern BOOL g_bSmartMediaExist;



/*
    @func   void | SpinForever | Halts execution (used in error conditions).
    @rdesc  
    @comm    
    @xref   
*/
static void SpinForever(void)
{
    EdbgOutputDebugString("SpinForever...\r\n");

    while(1)
    {
        ;
    }
}


/*
    @func   void | main | Samsung bootloader C routine entry point.
    @rdesc  N/A.
    @comm    
    @xref   
*/
void main(void)
{

    // Clear LEDs.
    //   
    OEMWriteDebugLED(0, 0xF);

    // Common boot loader (blcommon) main routine.
    //    
    BootloaderMain();

    // Should never get here.
    // 
    SpinForever();
}


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
                           (BppMode     <<  1) |       /* bpp Mode                                       */
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

    // Display a bitmap image on the LCD...
    //
    // Display a bitmap image on the LCD...
    //
    if (BppMode == 12)
    {
        if (240 == XSize)
        {
            memcpy((void *)IMAGE_FRAMEBUFFER_UA_BASE, ScreenBitmap, LCD_ARRAY_SIZE_TFT_16BIT);
        }
        else
        {
            unsigned int Line;

            for (Line=0; Line<min(YSize,320); ++Line)
            {
                memcpy((void*)(IMAGE_FRAMEBUFFER_UA_BASE+Line*XSize*2),
                       ScreenBitmap+240*Line*2,
                       min(XSize*2,320*2));
            }
        }
    }
    else
    {
        // Not 16bpp, so just blank the screen
        memset((void*)IMAGE_FRAMEBUFFER_UA_BASE, 0, XSize*YSize*(BppMode-10));
    }

}

static void InitRealTimeClock(void)
{
    SYSTEMTIME defaultTime = {1998,1,0,1,12,0,0,0};
    OEMSetRealTime(&defaultTime);
}

/*
    @func   BOOL | WriteBootConfig | Write bootloader settings to flash.
    @rdesc  TRUE = Success, FALSE = Failure.
    @comm    
    @xref   
*/
static BOOL WriteBootConfig(PBOOT_CFG pBootCfg)
{
    BOOL bResult = FALSE;

    OALMSG(OAL_FUNC, (TEXT("+WriteBootConfig.\r\n")));

    if (!pBootCfg)
    {
        goto CleanUp;
    }

    // First, erase the eboot settings area in flash...
    //
    if (!AM29LV800_EraseFlash(EBOOT_CONFIG_OFFSET, sizeof(BOOT_CFG)))
    {
        OALMSG(OAL_ERROR, (TEXT("ERROR: WriteEbootConfig: Flash erase failed.\r\n")));
        goto CleanUp;
    }

    // Write settings to flash...
    //
    if (!AM29LV800_WriteFlash(EBOOT_CONFIG_OFFSET, (PBYTE)pBootCfg, sizeof(BOOT_CFG)))
    {
        OALMSG(OAL_ERROR, (TEXT("ERROR: WriteEbootConfig: Flash write failed.\r\n")));
        goto CleanUp;
    }

    bResult = TRUE;

CleanUp:
    OALMSG(OAL_FUNC, (TEXT("-WriteBootConfig.\r\n")));
    return(bResult);
}


/*
    @func   void | ResetBootConfig | Reset the bootloader settings to their default values.
    @rdesc  N/A.
    @comm    
    @xref   
*/
static void ResetBootConfig(PBOOT_CFG pBootCfg)
{
    // Default eboot configuration values (leave the MAC address field alone)...
    //
    pBootCfg->Signature        = CONFIG_SIGNATURE;
    pBootCfg->VerMajor         = EBOOT_VERSION_MAJOR;
    pBootCfg->VerMinor         = EBOOT_VERSION_MINOR;
    pBootCfg->BootDelay        = CONFIG_AUTOBOOT_DEFAULT_COUNT;
    pBootCfg->ConfigFlags      = CONFIG_FLAGS_DHCP;
    pBootCfg->IPAddr           = 0;
    pBootCfg->SubnetMask       = 0;
    pBootCfg->LoadDeviceOrder  = 0;

    return;
}


/*
    @func   BOOL | ReadBootConfig | Read bootloader settings from flash.
    @rdesc  TRUE = Success, FALSE = Failure.
    @comm    
    @xref   
*/
static BOOL ReadBootConfig(PBOOT_CFG pBootCfg)
{
    BOOLEAN bResult = FALSE;

    OALMSG(OAL_FUNC, (TEXT("+ReadBootConfig.\r\n")));

    // Valid caller buffer?
    if (!pBootCfg)
    {
        OALMSG(OAL_ERROR, (TEXT("ERROR: Bad caller buffer.\r\n")));
        goto CleanUp;
    }

    // Read settings from flash...
    //
    if (!AM29LV800_ReadFlash(EBOOT_CONFIG_OFFSET, (PBYTE)pBootCfg, sizeof(BOOT_CFG)))
    {
        OALMSG(OAL_ERROR, (TEXT("ERROR: Flash read failed.\r\n")));
        goto CleanUp;
    }

    // Check configuration signature...
    //
    if (pBootCfg->Signature != CONFIG_SIGNATURE)
    {
        OALMSG(OAL_WARN, (TEXT("WARNING: Boot configuration signature invalid - choosing defaults...\r\n")));
        ResetBootConfig(pBootCfg);
        WriteBootConfig(pBootCfg);
    }

#if 1
    // override the emulator's setting and enable auto-ip
    pBootCfg->ConfigFlags      = 0;
    pBootCfg->IPAddr           = inet_addr("169.254.25.130");;
    pBootCfg->SubnetMask       = inet_addr("255.255.0.0");
#endif

    bResult = TRUE;

CleanUp:

    OALMSG(OAL_FUNC, (TEXT("-ReadBootConfig.\r\n")));
    return(TRUE);

}


/*
    @func   void | SetIP | Accepts IP address from user input.
    @rdesc  N/A.
    @comm    
    @xref   
*/

static void SetIP(PBOOT_CFG pBootCfg)
{
    CHAR   szDottedD[16];   // The string used to collect the dotted decimal IP address.
    USHORT cwNumChars = 0;
    USHORT InChar = 0;

    EdbgOutputDebugString("\r\nEnter new IP address: ");

    while(!((InChar == 0x0d) || (InChar == 0x0a)))
    {
        InChar = OEMReadDebugByte();
        if (InChar != OEM_DEBUG_COM_ERROR && InChar != OEM_DEBUG_READ_NODATA) 
        {
            // If it's a number or a period, add it to the string.
            //
            if (InChar == '.' || (InChar >= '0' && InChar <= '9')) 
            {
                if (cwNumChars < 16) 
                {
                    szDottedD[cwNumChars++] = (char)InChar;
                    OEMWriteDebugByte((BYTE)InChar);
                }
            }
            // If it's a backspace, back up.
            //
            else if (InChar == 8) 
            {
                if (cwNumChars > 0) 
                {
                    cwNumChars--;
                    OEMWriteDebugByte((BYTE)InChar);
                }
            }
        }
    }

    // If it's a carriage return with an empty string, don't change anything.
    //
    if (cwNumChars) 
    {
        szDottedD[cwNumChars] = '\0';
        pBootCfg->IPAddr = inet_addr(szDottedD);
    }
}


/*
    @func   void | SetMask | Accepts subnet mask from user input.
    @rdesc  N/A.
    @comm    
    @xref   
*/
static void SetMask(PBOOT_CFG pBootCfg)
{
    CHAR szDottedD[16]; // The string used to collect the dotted masks.
    USHORT cwNumChars = 0;
    USHORT InChar = 0;

    EdbgOutputDebugString("\r\nEnter new subnet mask: ");

    while(!((InChar == 0x0d) || (InChar == 0x0a)))
    {
        InChar = OEMReadDebugByte();
        if (InChar != OEM_DEBUG_COM_ERROR && InChar != OEM_DEBUG_READ_NODATA) 
        {
            // If it's a number or a period, add it to the string.
            //
            if (InChar == '.' || (InChar >= '0' && InChar <= '9')) 
            {
                if (cwNumChars < 16) 
                {
                    szDottedD[cwNumChars++] = (char)InChar;
                    OEMWriteDebugByte((BYTE)InChar);
                }
            }
            // If it's a backspace, back up.
            //
            else if (InChar == 8) 
            {
                if (cwNumChars > 0) 
                {
                    cwNumChars--;
                    OEMWriteDebugByte((BYTE)InChar);
                }
            }
        }
    }

    // If it's a carriage return with an empty string, don't change anything.
    //
    if (cwNumChars) 
    {
        szDottedD[cwNumChars] = '\0';
        pBootCfg->SubnetMask = inet_addr(szDottedD);
    }
}


/*
    @func   void | SetDelay | Accepts an autoboot delay value from user input.
    @rdesc  N/A.
    @comm    
    @xref   
*/
static void SetDelay(PBOOT_CFG pBootCfg)
{
    CHAR szCount[16];
    USHORT cwNumChars = 0;
    USHORT InChar = 0;

    EdbgOutputDebugString("\r\nEnter maximum number of seconds to delay [1-255]: ");

    while(!((InChar == 0x0d) || (InChar == 0x0a)))
    {
        InChar = OEMReadDebugByte();
        if (InChar != OEM_DEBUG_COM_ERROR && InChar != OEM_DEBUG_READ_NODATA) 
        {
            // If it's a number or a period, add it to the string.
            //
            if ((InChar >= '0' && InChar <= '9')) 
            {
                if (cwNumChars < 16) 
                {
                    szCount[cwNumChars++] = (char)InChar;
                    OEMWriteDebugByte((BYTE)InChar);
                }
            }
            // If it's a backspace, back up.
            //
            else if (InChar == 8) 
            {
                if (cwNumChars > 0) 
                {
                    cwNumChars--;
                    OEMWriteDebugByte((BYTE)InChar);
                }
            }
        }
    }

    // If it's a carriage return with an empty string, don't change anything.
    //
    if (cwNumChars) 
    {
        szCount[cwNumChars] = '\0';
        pBootCfg->BootDelay = atoi(szCount);
        if (pBootCfg->BootDelay > 255)
        {
            pBootCfg->BootDelay = 255;
        } 
        else if (pBootCfg->BootDelay < 1)
        {
            pBootCfg->BootDelay = 1;
        }
    }
}


static ULONG mystrtoul(PUCHAR pStr, UCHAR nBase)
{
    UCHAR nPos=0;
    BYTE c;
    ULONG nVal = 0;
    UCHAR nCnt=0;
    ULONG n=0;

    // fulllibc doesn't implement isctype or iswctype, which are needed by
    // strtoul, rather than including coredll code, here's our own simple strtoul.

    if (pStr == NULL)
        return(0);

    for (nPos=0 ; nPos < strlen(pStr) ; nPos++)
    {
        c = tolower(*(pStr + strlen(pStr) - 1 - nPos));
        if (c >= '0' && c <= '9')
            c -= '0';
        else if (c >= 'a' && c <= 'f')
        {
            c -= 'a';
            c  = (0xa + c);
        }

        for (nCnt = 0, n = 1 ; nCnt < nPos ; nCnt++)
        {
            n *= nBase;
        }
        nVal += (n * c);
    }

    return(nVal);
}


static void CvtMAC(USHORT MacAddr[3], char *pszDottedD ) 
{
    DWORD cBytes;
    char *pszLastNum;
    int atoi (const char *s);
    int i=0;    
    BYTE *p = (BYTE *)MacAddr;

    // Replace the dots with NULL terminators
    pszLastNum = pszDottedD;
    for(cBytes = 0 ; cBytes < 6 ; cBytes++)
    {
        while(*pszDottedD != '.' && *pszDottedD != '\0')
        {
            pszDottedD++;
        }
        if (pszDottedD == '\0' && cBytes != 5)
        {
            // zero out the rest of MAC address
            while(i++ < 6)
            {
                *p++ = 0;
            }
            break;
        }
        *pszDottedD = '\0';
        *p++ = (BYTE)(mystrtoul(pszLastNum, 16) & 0xFF);
        i++;
        pszLastNum = ++pszDottedD;
    }
}


static void SetCS8900MACAddress(PBOOT_CFG pBootCfg)
{
    CHAR szDottedD[24];
    USHORT cwNumChars = 0;
    USHORT InChar = 0;

    memset(szDottedD, '0', 24);

    EdbgOutputDebugString ( "\r\nEnter new MAC address in hexadecimal (hh.hh.hh.hh.hh.hh): ");

    while(!((InChar == 0x0d) || (InChar == 0x0a)))
    {
        InChar = OEMReadDebugByte();
        InChar = tolower(InChar);
        if (InChar != OEM_DEBUG_COM_ERROR && InChar != OEM_DEBUG_READ_NODATA) 
        {
            // If it's a hex number or a period, add it to the string.
            //
            if (InChar == '.' || (InChar >= '0' && InChar <= '9') || (InChar >= 'a' && InChar <= 'f')) 
            {
                if (cwNumChars < 17) 
                {
                    szDottedD[cwNumChars++] = (char)InChar;
                    OEMWriteDebugByte((BYTE)InChar);
                }
            }
            else if (InChar == 8)       // If it's a backspace, back up.
            {
                if (cwNumChars > 0) 
                {
                    cwNumChars--;
                    OEMWriteDebugByte((BYTE)InChar);
                }
            }
        }
    }

    EdbgOutputDebugString ( "\r\n");

    // If it's a carriage return with an empty string, don't change anything.
    //
    if (cwNumChars) 
    {
        szDottedD[cwNumChars] = '\0';
        CvtMAC(pBootCfg->CS8900MAC, szDottedD);

        EdbgOutputDebugString("INFO: MAC address set to: %x:%x:%x:%x:%x:%x\r\n",
                  pBootCfg->CS8900MAC[0] & 0x00FF, pBootCfg->CS8900MAC[0] >> 8,
                  pBootCfg->CS8900MAC[1] & 0x00FF, pBootCfg->CS8900MAC[1] >> 8,
                  pBootCfg->CS8900MAC[2] & 0x00FF, pBootCfg->CS8900MAC[2] >> 8);
    }
    else
    {
        EdbgOutputDebugString("WARNING: SetCS8900MACAddress: Invalid MAC address.\r\n");
    }
}


/*
    @func   BOOL | MainMenu | Manages the Samsung bootloader main menu.
    @rdesc  TRUE == Success and FALSE == Failure.
    @comm    
    @xref   
*/

static BOOL MainMenu(PBOOT_CFG pBootCfg)
{
    BYTE KeySelect = 0;
    BOOL bConfigChanged = FALSE;

    while(TRUE)
    {
        KeySelect = 0;

        EdbgOutputDebugString ( "\r\nEthernet Boot Loader Configuration:\r\n\r\n");
        EdbgOutputDebugString ( "0) IP address: %s\r\n",inet_ntoa(pBootCfg->IPAddr));
        EdbgOutputDebugString ( "1) Subnet mask: %s\r\n", inet_ntoa(pBootCfg->SubnetMask));
        EdbgOutputDebugString ( "2) DHCP: %s\r\n", (pBootCfg->ConfigFlags & CONFIG_FLAGS_DHCP)?"Enabled":"Disabled");
        EdbgOutputDebugString ( "3) Boot delay: %d seconds\r\n", pBootCfg->BootDelay);
        EdbgOutputDebugString ( "4) Reset to factory default configuration\r\n");
        EdbgOutputDebugString ( "5) Program disk image into SmartMedia card: %s\r\n", (pBootCfg->ConfigFlags & CONFIG_FLAGS_SAVETOFLASH)?"Enabled":"Disabled");
        EdbgOutputDebugString ( "6) Program CS8900 MAC address\r\n");
        EdbgOutputDebugString ( "7) Low-level format the Smart Media card\r\n");
        EdbgOutputDebugString ( "D) Download image now\r\n");
        EdbgOutputDebugString ( "\r\nEnter your selection: ");

        while (! ( ( (KeySelect >= '0') && (KeySelect <= '7') ) ||
                   ( (KeySelect == 'D') || (KeySelect == 'd') ) ))
        {
            KeySelect = OEMReadDebugByte();
        }

        EdbgOutputDebugString ( "%c\r\n", KeySelect);

        switch(KeySelect)
        {
        case '0':           // Change IP address.
            SetIP(pBootCfg);
            bConfigChanged = TRUE;
            break;
        case '1':           // Change subnet mask.
            SetMask(pBootCfg);
            bConfigChanged = TRUE;
            break;
        case '2':           // Toggle static/DHCP mode.
            pBootCfg->ConfigFlags = (pBootCfg->ConfigFlags ^ CONFIG_FLAGS_DHCP);
            bConfigChanged = TRUE;
            break;
        case '3':           // Change autoboot delay.
            SetDelay(pBootCfg);
            bConfigChanged = TRUE;
            break;
        case '4':           // Reset the bootloader configuration to defaults.
            ResetBootConfig(pBootCfg);
            bConfigChanged = TRUE;
            break;
        case '5':           // Toggle image storage to Smart Media.
            pBootCfg->ConfigFlags = (pBootCfg->ConfigFlags ^ CONFIG_FLAGS_SAVETOFLASH);
            bConfigChanged = TRUE;
            break;
        case '6':           // Configure Crystal CS8900 MAC address.
            SetCS8900MACAddress(pBootCfg);
            bConfigChanged = TRUE;
            break;
        case '7':           // Format the Smart Media card.
            if (g_bSmartMediaExist && !FormatSmartMedia())
            {
                RETAILMSG(1, (TEXT("ERROR: Failed to perform low-level format of SmartMedia card.\r\n")));
            }
            break;
        case 'D':           // Download? Yes.
        case 'd':
            goto MENU_DONE;
            break;
        default:
            break;
        }
    }

MENU_DONE:

    // If eboot settings were changed by user, save them to flash.
    //
    if (bConfigChanged && !WriteBootConfig(pBootCfg))
    {
        OALMSG(OAL_WARN, (TEXT("WARNING: MainMenu: Failed to store updated bootloader configuration to flash.\r\n")));
    }

    return(TRUE);
}


/*
    @func   BOOL | OEMPlatformInit | Initialize the Samsung SMD2410 platform hardware.
    @rdesc  TRUE = Success, FALSE = Failure.
    @comm    
    @xref   
*/
BOOL OEMPlatformInit(void)
{
    UINT8 BootDelay;
    UINT8 KeySelect;
    UINT32 dwStartTime, dwPrevTime, dwCurrTime;
    PCI_REG_INFO NANDInfo;
    BOOLEAN bResult = FALSE;

    OALMSG(OAL_FUNC, (TEXT("+OEMPlatformInit.\r\n")));

    EdbgOutputDebugString("Microsoft Windows CE Bootloader for the Microsoft DeviceEmulator Version %d.%d Built %s\r\n\r\n", 
                          EBOOT_VERSION_MAJOR, EBOOT_VERSION_MINOR, __DATE__);

    // Initialize the display.
    //
    InitDisplay();

    // Initialize the RealTimeClock
    InitRealTimeClock();

    // Initialize the BSP args structure.
    //
    memset(pBSPArgs, 0, sizeof(BSP_ARGS));
    pBSPArgs->header.signature       = OAL_ARGS_SIGNATURE;
    pBSPArgs->header.oalVersion      = OAL_ARGS_VERSION;
    pBSPArgs->header.bspVersion      = BSP_ARGS_VERSION;
    pBSPArgs->kitl.flags             = OAL_KITL_FLAGS_ENABLED | OAL_KITL_FLAGS_VMINI;
    pBSPArgs->kitl.devLoc.IfcType    = Internal;
    pBSPArgs->kitl.devLoc.BusNumber  = 0;
    pBSPArgs->kitl.devLoc.LogicalLoc = BSP_BASE_REG_PA_CS8900A_IOBASE;

    // Initialize the AMD AM29LV800 flash code.
    //
    if (!AM29LV800_Init((UINT32)AMD_FLASH_START))
    {
        OALMSG(OAL_ERROR, (TEXT("ERROR: OEMPlatformInit: Flash initialization failed.\r\n")));
        goto CleanUp;
    }

    // Initialize the Smart Media flash driver (and partitioning code).
    //
    memset(&NANDInfo, 0, sizeof(PCI_REG_INFO));
    NANDInfo.MemBase.Num    = 1;
    NANDInfo.MemBase.Reg[0] = (DWORD)OALPAtoVA(S3C2410X_BASE_REG_PA_NAND, FALSE);
    if (!FMD_Init(NULL, &NANDInfo, NULL))
    {
        OALMSG(OAL_WARN, (TEXT("WARNING: OEMPlatformInit: Failed to initialize Smart Media.\r\n")));
        g_bSmartMediaExist = FALSE;
    }
    else
    {
        g_bSmartMediaExist = TRUE;
    }

    // Retrieve eboot settings from AMD flash.
    //
    if (!ReadBootConfig(&g_BootConfig))
    {
        OALMSG(OAL_ERROR, (TEXT("ERROR: OEMPlatformInit: Failed to retrieve bootloader settings from flash.\r\n")));
        goto CleanUp;
    }

    // Display boot message - user can halt the autoboot by pressing any key on the serial terminal emulator.
    //
    BootDelay = g_BootConfig.BootDelay;

    EdbgOutputDebugString ( "Press [ENTER] to download now or [SPACE] to cancel.\r\n");
    EdbgOutputDebugString ( "\r\nInitiating image download in %d seconds. ", BootDelay--);

    dwStartTime = OEMEthGetSecs();
    dwPrevTime  = dwStartTime;
    dwCurrTime  = dwStartTime;
    KeySelect   = 0;

    // Allow the user to break into the bootloader menu.
    //
    while((dwCurrTime - dwStartTime) < g_BootConfig.BootDelay)
    {
        KeySelect = OEMReadDebugByte();
        if ((KeySelect == 0x20) || (KeySelect == 0x0d))
            break;
        dwCurrTime = OEMEthGetSecs();

        if (dwCurrTime > dwPrevTime)
        {
            int i, j;

            // 1 Second has elapsed - update the countdown timer.
            dwPrevTime = dwCurrTime;
            if (BootDelay < 9)
                i = 11;
            else if (BootDelay < 99)
                i = 12;
            else if (BootDelay < 999)
                i = 13;

            for(j = 0; j < i; j++)
                OEMWriteDebugByte((BYTE)0x08); // print back space
            EdbgOutputDebugString ( "%d seconds. ", BootDelay--);
        }
    }
    EdbgOutputDebugString ( "\r\n");

    // Boot or enter bootloader menu.
    //
    switch(KeySelect)
    {
    case 0x20: // Boot menu.
        MainMenu(&g_BootConfig);
        break;
    case 0x00: // Fall through if no keys were pressed -or-
    case 0x0d: // the user cancelled the countdown.
    default:
        EdbgOutputDebugString ( "\r\nStarting auto-download ... \r\n");
        break;
    }

    // Configure Ethernet controller.
    //
    if (!InitEthDevice(&g_BootConfig))
    {
        OALMSG(OAL_ERROR, (TEXT("ERROR: OEMPlatformInit: Failed to initialize Ethernet controller.\r\n")));
        goto CleanUp;
    }

    bResult = TRUE;

CleanUp:

    OALMSG(OAL_FUNC, (TEXT("_OEMPlatformInit.\r\n")));
    return(bResult);
}


/*
    @func   DWORD | OEMPreDownload | Complete pre-download tasks - get IP address, initialize TFTP, etc.
    @rdesc  BL_DOWNLOAD = Platform Builder is asking us to download an image, BL_JUMP = Platform Builder is requesting we jump to an existing image, BL_ERROR = Failure.
    @comm    
    @xref   
*/
DWORD OEMPreDownload(void)
{
    BOOL  bGotJump = FALSE;
    DWORD dwDHCPLeaseTime = 0;
    PDWORD pdwDHCPLeaseTime = &dwDHCPLeaseTime;
    DWORD dwBootFlags = 0;


    // Create device name based on Ethernet address (this is how Platform Builder identifies this device).
    //
    OALKitlCreateName(BSP_DEVICE_PREFIX, pBSPArgs->kitl.mac, pBSPArgs->deviceId);
    OALMSG(TRUE, (L"INFO: *** Device Name '%hs' ***\r\n", pBSPArgs->deviceId));

    // If the user wants to use a static IP address, don't request an address 
    // from a DHCP server.  This is done by passing in a NULL for the DHCP 
    // lease time variable.  If user specified a static IP address, use it (don't use DHCP).
    //
    if (!(g_BootConfig.ConfigFlags & CONFIG_FLAGS_DHCP))
    {
        // Static IP address.
        pBSPArgs->kitl.ipAddress  = g_BootConfig.IPAddr;
        pBSPArgs->kitl.ipMask     = g_BootConfig.SubnetMask;
        pBSPArgs->kitl.flags     &= ~OAL_KITL_FLAGS_DHCP;
        pdwDHCPLeaseTime = NULL;
        OALMSG(TRUE, (TEXT("INFO: Using static IP address %s.\r\n"), inet_ntoa(pBSPArgs->kitl.ipAddress))); 
        OALMSG(TRUE, (TEXT("INFO: Using subnet mask %s.\r\n"),       inet_ntoa(pBSPArgs->kitl.ipMask))); 
    }
    else
    {
        pBSPArgs->kitl.ipAddress = 0;
        pBSPArgs->kitl.ipMask    = 0;
    }

    // Initialize the the TFTP transport.
    //
    g_DeviceAddr.dwIP = pBSPArgs->kitl.ipAddress;
    memcpy(g_DeviceAddr.wMAC, pBSPArgs->kitl.mac, (3 * sizeof(UINT16)));
    g_DeviceAddr.wPort = 0;

    if (!EbootInitEtherTransport(&g_DeviceAddr,
                                 &pBSPArgs->kitl.ipMask,
                                 &bGotJump,
                                 pdwDHCPLeaseTime,
                                 EBOOT_VERSION_MAJOR,
                                 EBOOT_VERSION_MINOR,
                                 BSP_DEVICE_PREFIX,
                                 pBSPArgs->deviceId,
                                 EDBG_CPU_ARM720,
                                 dwBootFlags))
    {
        OALMSG(OAL_ERROR, (TEXT("ERROR: OEMPreDownload: Failed to initialize Ethernet connection.\r\n")));
        return(BL_ERROR);
    }


    // If the user wanted a DHCP address, we presumably have it now - save it for the OS to use.
    //
    if (g_BootConfig.ConfigFlags & CONFIG_FLAGS_DHCP)
    {
        // DHCP address.
        pBSPArgs->kitl.ipAddress  = g_DeviceAddr.dwIP;
        pBSPArgs->kitl.flags     |= OAL_KITL_FLAGS_DHCP;
    }

    return(bGotJump ? BL_JUMP : BL_DOWNLOAD);
}


/*
    @func   BOOL | OEMReadData | Generically read download data (abstracts actual transport read call).
    @rdesc  TRUE = Success, FALSE = Failure.
    @comm    
    @xref   
*/
BOOL OEMReadData(DWORD dwData, PUCHAR pData)
{
    return(EbootEtherReadData(dwData, pData));
}


/*
    @func   void | OEMShowProgress | Displays download progress for the user.
    @rdesc  N/A.
    @comm    
    @xref   
*/
void OEMShowProgress(DWORD dwPacketNum)
{
}


/*
    @func   void | OEMLaunch | Executes the stored/downloaded image.
    @rdesc  N/A.
    @comm    
    @xref   
*/

void OEMLaunch( DWORD dwImageStart, DWORD dwImageLength, DWORD dwLaunchAddr, const ROMHDR *pRomHdr )
{
    DWORD dwPhysLaunchAddr;
    EDBG_OS_CONFIG_DATA *pCfgData;    
    EDBG_ADDR EshellHostAddr;
    EDBG_ADDR DeviceAddr;

    
    // If the user requested that a disk image (stored in RAM now) be written to the SmartMedia card, so it now.
    //
    if (g_BootConfig.ConfigFlags & CONFIG_FLAGS_SAVETOFLASH)
    {
        // Since this platform only supports RAM images, the image cache address is the same as the image RAM address.
        //
        if (!WriteDiskImageToSmartMedia(dwImageStart, dwImageLength, &g_BootConfig))
        {
            OALMSG(OAL_ERROR, (TEXT("ERROR: OEMLaunch: Failed to store image to Smart Media.\r\n")));
            goto CleanUp;
        }

        // Store the bootloader settings to flash.
        //
        // TODO: minimize flash writes.
        //
        if (!WriteBootConfig(&g_BootConfig))
        {
            OALMSG(OAL_ERROR, (TEXT("ERROR: OEMLaunch: Failed to store bootloader settings to flash.\r\n")));
            goto CleanUp;
        }

        OALMSG(TRUE, (TEXT("INFO: Disk image stored to Smart Media.  Please Reboot.  Halting...\r\n")));
        while(1)
        {
            // Wait...
        }
    }

    // Wait for Platform Builder to connect after the download and send us IP and port settings for service
    // connections - also sends us KITL flags.  This information is used later by the OS (KITL).
    //
    if (g_bWaitForConnect)
    {
        memset(&EshellHostAddr, 0, sizeof(EDBG_ADDR));

        DeviceAddr.dwIP  = pBSPArgs->kitl.ipAddress;
        memcpy(DeviceAddr.wMAC, pBSPArgs->kitl.mac, (3 * sizeof(UINT16)));
        DeviceAddr.wPort = 0;

        if (!(pCfgData = EbootWaitForHostConnect(&DeviceAddr, &EshellHostAddr)))
        {
            OALMSG(OAL_ERROR, (TEXT("ERROR: OEMLaunch: EbootWaitForHostConnect failed.\r\n")));
            goto CleanUp;
        }

        // If the user selected "passive" KITL (i.e., don't connect to the target at boot time), set the
        // flag in the args structure so the OS image can honor it when it boots.
        //
        if (pCfgData->KitlTransport & KTS_PASSIVE_MODE)
        {
            pBSPArgs->kitl.flags |= OAL_KITL_FLAGS_PASSIVE;
        }
    }

    // If a launch address was provided, we must have downloaded the image, save the address in case we
    // want to jump to this image next time.  If no launch address was provided, retrieve the last one.
    //
    if (dwLaunchAddr)
    {
        g_BootConfig.LaunchAddress = dwLaunchAddr;
    }
    else
    {
        dwLaunchAddr = g_BootConfig.LaunchAddress;
    }

    // Save bootloader settings in flash.
    //
    // TODO: minimize flash writes.
    //
    if (!WriteBootConfig(&g_BootConfig))
    {
        OALMSG(OAL_ERROR, (TEXT("ERROR: OEMLaunch: Failed to store bootloader settings in flash.\r\n")));
        goto CleanUp;
    }

    // Jump to downloaded image (use the physical address since we'll be turning the MMU off)...
    //
    dwPhysLaunchAddr = (DWORD)OALVAtoPA((void *)dwLaunchAddr);
    OALMSG(TRUE, (TEXT("INFO: OEMLaunch: Jumping to Physical Address 0x%Xh (Virtual Address 0x%Xh)...\r\n\r\n\r\n"), dwPhysLaunchAddr, dwLaunchAddr));

    // Jump...
    //
    Launch(dwPhysLaunchAddr);


CleanUp:

    OALMSG(TRUE, (TEXT("ERROR: OEMLaunch: Halting...\r\n")));
    SpinForever();
}


//------------------------------------------------------------------------------
//
//  Function Name:  OEMVerifyMemory( DWORD dwStartAddr, DWORD dwLength )
//  Description..:  This function verifies the passed address range lies
//                  within a valid region of memory. Additionally this function
//                  sets the g_ImageType if the image is a boot loader. 
//  Inputs.......:  DWORD           Memory start address
//                  DWORD           Memory length
//  Outputs......:  BOOL - true if verified, false otherwise
//
//------------------------------------------------------------------------------

BOOL OEMVerifyMemory( DWORD dwStartAddr, DWORD dwLength )
{
    BOOL    rc;                 // return code
    DWORD   Addr1;              // starting address
    DWORD   Addr2;              // ending   address

    // Convert address to a cached address.
    //
    dwStartAddr &= ~CACHED_TO_UNCACHED_OFFSET;

    // Setup address range for comparison
    //
    Addr1 = dwStartAddr;
    Addr2 = Addr1 + (dwLength - 1);

    EdbgOutputDebugString( "****** OEMVerifyMemory Checking Range [ 0x%x ==> 0x%x ]\r\n", Addr1, Addr2 );

    // Validate the bootloader range to set the g_ImageType. The bootloader address
    // range is a subset of the whole flash range. This is required as the original
    // function set the image type based on this. Therefore this check is done before
    // the whole FLASH range is checked.

    if( (Addr1 >= FLASH_EBOOT_START) && (Addr2 <= FLASH_EBOOT_END) )
    {
        EdbgOutputDebugString("****** bootloader address ****** \r\n\r\n");

        // Set the image type 

        g_ImageType = IMAGE_LOADER;
        rc = TRUE;
    }
    else if( (Addr1 >= FLASH_START) && (Addr1 <= FLASH_END) )
    {
        EdbgOutputDebugString("****** FLASH address ****** \r\n\r\n");

        rc = TRUE;
    }
    else if( (Addr1 >= RAM_START) && (Addr2 <= RAM_END ) )
    {
        EdbgOutputDebugString("****** RAM address ****** \r\n\r\n");

        rc = TRUE;
    }
    else
    {
        EdbgOutputDebugString("****** OEMVerifyMemory FAILED - Invalid Memory Area ****** \r\n\r\n");

        rc = FALSE;
    }

    // Indicate status

    return( rc );
}

/////////////////////// START - Stubbed functions - START //////////////////////////////
/*
    @func   void | SC_WriteDebugLED | Write to debug LED.
    @rdesc  N/A.
    @comm    
    @xref   
*/

void SC_WriteDebugLED(USHORT wIndex, ULONG dwPattern)
{
    // Stub - needed by NE2000 EDBG driver...
    //
}


ULONG HalSetBusDataByOffset(IN BUS_DATA_TYPE BusDataType,
                            IN ULONG BusNumber,
                            IN ULONG SlotNumber,
                            IN PVOID Buffer,
                            IN ULONG Offset,
                            IN ULONG Length)
{
    return(0);
}


ULONG
HalGetBusDataByOffset(IN BUS_DATA_TYPE BusDataType,
                      IN ULONG BusNumber,
                      IN ULONG SlotNumber,
                      IN PVOID Buffer,
                      IN ULONG Offset,
                      IN ULONG Length)
{
    return(0);
}


BOOLEAN HalTranslateBusAddress(IN INTERFACE_TYPE  InterfaceType,
                               IN ULONG BusNumber,
                               IN PHYSICAL_ADDRESS BusAddress,
                               IN OUT PULONG AddressSpace,
                               OUT PPHYSICAL_ADDRESS TranslatedAddress)
{

    // All accesses on this platform are memory accesses...
    //
    if (AddressSpace)
        *AddressSpace = 0;
 
    // 1:1 mapping...
    //
    if (TranslatedAddress)
    {
        *TranslatedAddress = BusAddress;
        return(TRUE);
    }

    return(FALSE);
}


PVOID MmMapIoSpace(IN PHYSICAL_ADDRESS PhysicalAddress,
                   IN ULONG NumberOfBytes,
                   IN BOOLEAN CacheEnable)
{
    DWORD dwAddr = PhysicalAddress.LowPart;

    if (CacheEnable)
        dwAddr &= ~CACHED_TO_UNCACHED_OFFSET; 
    else
        dwAddr |= CACHED_TO_UNCACHED_OFFSET; 

    return((PVOID)dwAddr);
}


VOID MmUnmapIoSpace(IN PVOID BaseAddress,
                    IN ULONG NumberOfBytes)
{
}

VOID WINAPI SetLastError(DWORD dwErrCode)
{
}
/////////////////////// END - Stubbed functions - END //////////////////////////////


/*
    @func   void | OEMDownloadFileNotify | Receives/processes download file manifest.
    @rdesc  None.
    @comm    
    @xref   
*/
void OEMDownloadFileNotify(PDownloadManifest pInfo)
{
    DWORD dwCount;
    DWORD dwNumNB0Files = 0;

    if (!pInfo) return;

    EdbgOutputDebugString("\r\nDownload file information:\r\n");
    EdbgOutputDebugString("-------------------------------------------------------------------------------\r\n");

    for (dwCount = 0 ; dwCount < pInfo->dwNumRegions ; dwCount++)
    {

        EdbgOutputDebugString("[%d]: Address=0x%x  Length=0x%x  Name=%s\r\n", dwCount, 
                                                                              pInfo->Region[dwCount].dwRegionStart, 
                                                                              pInfo->Region[dwCount].dwRegionLength, 
                                                                              pInfo->Region[dwCount].szFileName);

        // .nb0 files will have a start address of 0 because Platform Builder
        // won't know where to place them.  We'll support only one .nb0 file
        // download (this is an Image Update disk image).  If we need to
        // support more than one .nb0 file download in the future, we'll need
        // to differentiate them by filename.
        //
        if (pInfo->Region[dwCount].dwRegionStart == 0)
        {
            // We only support one raw binary file (disk image).
            if (dwNumNB0Files++)
            {
                EdbgOutputDebugString("ERROR: This bootloader doesn't support downloading a second .nb0 binary image.\r\n");
                SpinForever();
            }

            // The disk image .nb0 file should be placed immediately after the
            // bootloader image in flash.
            pInfo->Region[dwCount].dwRegionStart = 0x80001000;

            EdbgOutputDebugString("INFO: Changed start address for %s to 0x%x.\r\n", pInfo->Region[dwCount].szFileName, 
                                                                                     pInfo->Region[dwCount].dwRegionStart);

        }
    }

    EdbgOutputDebugString("\r\n");

    return;
}


/*
    @func   BOOL | OEMDebugInit | Initializes the serial port for debug output message.
    @rdesc  TRUE == Success and FALSE == Failure.
    @comm    
    @xref   
*/
BOOL OEMDebugInit(void)
{

    // Set up function callbacks used by blcommon.
    //
    g_pOEMVerifyMemory   = OEMVerifyMemory;      // Verify RAM.
    g_pOEMMultiBINNotify = OEMDownloadFileNotify;

    // Call serial initialization routine (shared with the OAL).
    //
    OEMInitDebugSerial();

    return(TRUE);
}

