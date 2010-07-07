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
#include <ceddk.h>
#include <nkintr.h>

#undef ZONE_INIT

#include <keybddbg.h>
#include <keybddr.h>
#include <keybdpdd.h>
#include <keybdist.h>
#include "s3c2410kbd.hpp"
#include <s3c2410x.h>

// Pointer to device control registers
volatile S3C2410X_IOPORT_REG *v_pIOPregs;
volatile S3C2410X_SPI_REG *v_pSPIregs;

DWORD g_dwSysIntr_Keybd = SYSINTR_UNDEFINED;

// Scan code consts
static const UINT8 scE0Extended	= 0xe0;
static const UINT8 scE1Extended	= 0xe1;
static const UINT8 scKeyUpMask	= 0x80;

UINT32
ScanCodeToVKeyEx(
        UINT32                  ScanCode,
        KEY_STATE_FLAGS KeyStateFlags,
        UINT32                  VKeyBuf[16],
        UINT32                  ScanCodeBuf[16],
        KEY_STATE_FLAGS KeyStateFlagsBuf[16]
        );

//	There is really only one physical keyboard supported by the system.
Ps2Keybd *v_pp2k;

extern void ReadRegDWORD( LPCWSTR szKeyName, LPCWSTR szValueName, LPDWORD pdwValue );

void WINAPI KeybdPdd_PowerHandler(BOOL bOff)
{
	if (!bOff) { 
	   v_pp2k->KeybdPowerOn();
	}
	else {
	   v_pp2k->KeybdPowerOff();
	}
	return;
}

#define ONEBIT    0x1

int	putcToKBCTL(UCHAR c)
{
	UINT	i;

  	v_pIOPregs->GPBDAT &= ~(ONEBIT << 6);       //Set _SS signal to low (Slave Select)

	while((v_pSPIregs->SPSTA1 & ONEBIT)==0);	// wait while busy

	v_pSPIregs->SPTDAT1 = c;	                // write left justified data

	while((v_pSPIregs->SPSTA1 & ONEBIT)==0);	// wait while busy
   	
   	v_pIOPregs->GPBDAT |= (ONEBIT << 6);        //Set _SS signal to high (Slave Select)

	i = v_pSPIregs->SPRDAT1;

	return(i);
}


void getsFromKBCTL(UINT8 *m, int cnt) {
	int	i, j;
	volatile tmp = 1;

	for(j = 0; j < 3; j++)
		tmp += tmp;
	for(j = 0; j < 250 * 30; j++)
		tmp += tmp;

	for(i = 0; i < cnt; i++) {
		m[i] = putcToKBCTL(0xFF);

		for(j = 0; j < 400; j++)
			tmp+= tmp;
	}
}

void putsToKBCTL(UINT8 *m,  int cnt)
{
	int	i, j, x;
	volatile tmp = 1;
	
	for(j = 0; j < 3; j++)
		x = j;
	for(j = 0; j < 3; j++)
		tmp += tmp;
	for(j = 0; j < 250 * 30; j++)
		tmp += tmp;

	for(i = 0; i < cnt; i++) {

		j = putcToKBCTL(m[i]);

		for(j = 0; j < 400; j++)
			tmp+= tmp;
		for(j = 0; j < 400; j++)
			x = j;
    }
}

char lrc(UINT8 *buffer, int count)
{
    char lrc;
    int n;

    lrc = buffer[0] ^ buffer[1];

    for (n = 2; n < count; n++)
    {
        lrc ^= buffer[n];
    }

    if (lrc & 0x80)
        lrc ^= 0xC0;

    return lrc;
}

int USAR_WriteRegister(int reg, int data)
{
    UINT8 cmd_buffer[4];

    cmd_buffer[0] = 0x1b; //USAR_PH_WR;
    cmd_buffer[1] = (unsigned char)reg;
    cmd_buffer[2] = (unsigned char)data;

    cmd_buffer[3] = lrc((UINT8 *)cmd_buffer,3);
    putsToKBCTL((UINT8 *)cmd_buffer,4);

    return TRUE;
}


BOOL
KeybdDriverInitializeAddresses(
	void
	)
{
	bool RetValue = TRUE;
	DWORD dwIOBase;
	DWORD dwSSPBase;

	DEBUGMSG(1,(TEXT("++KeybdDriverInitializeAddresses\r\n")));

	ReadRegDWORD(TEXT("HARDWARE\\DEVICEMAP\\KEYBD"), _T("IOBase"), &dwIOBase );
	if(dwIOBase == 0) {
		DEBUGMSG(1, (TEXT("Can't fount registry entry : HARDWARE\\DEVICEMAP\\KEYBD\\IOBase\r\n")));
		goto error_return;
	}
	DEBUGMSG(1, (TEXT("HARDWARE\\DEVICEMAP\\KEYBD\\IOBase:%x\r\n"), dwIOBase));

	ReadRegDWORD(TEXT("HARDWARE\\DEVICEMAP\\KEYBD"), _T("SSPBase"), &dwSSPBase );
	if(dwSSPBase == 0) {
		DEBUGMSG(1, (TEXT("Can't fount registry entry : HARDWARE\\DEVICEMAP\\KEYBD\\SSPBase\r\n")));
		goto error_return;
	}
	DEBUGMSG(1, (TEXT("HARDWARE\\DEVICEMAP\\KEYBD\\SSPBase:%x\r\n"), dwSSPBase));

	v_pIOPregs = (volatile S3C2410X_IOPORT_REG *)VirtualAlloc(0, sizeof(S3C2410X_IOPORT_REG), MEM_RESERVE, PAGE_NOACCESS);
	if(v_pIOPregs == NULL) {
		DEBUGMSG(1,(TEXT("[KBD] v_pIOPregs : VirtualAlloc failed!\r\n")));
		goto error_return;
	}
	else {
		if(!VirtualCopy((PVOID)v_pIOPregs, (PVOID)(dwIOBase), sizeof(S3C2410X_IOPORT_REG), PAGE_READWRITE|PAGE_NOCACHE )) 
		{
			DEBUGMSG(1,(TEXT("[KBD] v_pIOPregs : VirtualCopy failed!\r\n")));
			goto error_return;
		}
	}
	DEBUGMSG(1, (TEXT("[KBD] v_pIOPregs mapped at %x\r\n"), v_pIOPregs));
	
	v_pSPIregs = (volatile S3C2410X_SPI_REG *)VirtualAlloc(0, sizeof(S3C2410X_SPI_REG), MEM_RESERVE, PAGE_NOACCESS);
	if (v_pSPIregs == NULL) {
		DEBUGMSG(1, (TEXT("[KBD] v_pSPIregs : VirtualAlloc failed!\r\n")));
		goto error_return;
	}
	else {
		if (!VirtualCopy((PVOID)v_pSPIregs, (PVOID)(dwSSPBase), sizeof(S3C2410X_SPI_REG), PAGE_READWRITE | PAGE_NOCACHE)) 
		{
		    	DEBUGMSG(1, (TEXT("[KBD] v_pSPIregs : VirtualCopy failed!\r\n")));
				goto error_return;
		}
	}

	DEBUGMSG(1, (TEXT("[KBD] v_pSPIregs mapped at %x\r\n"), v_pSPIregs));
	
	DEBUGMSG(1,(TEXT("--KeybdDriverInitializeAddresses\r\n")));
	
	return TRUE;

error_return:
	if ( v_pIOPregs )
		VirtualFree((PVOID)v_pIOPregs, 0, MEM_RELEASE);
	if ( v_pSPIregs )
		VirtualFree((PVOID)v_pSPIregs, 0, MEM_RELEASE);
	v_pIOPregs = 0;
	v_pSPIregs = 0;

	DEBUGMSG(1,(TEXT("--KeybdDriverInitializeAddresses[FAILED!!!]\r\n")));

	return FALSE;
}

static UINT KeybdPdd_GetEventEx2(UINT uiPddId, UINT32 rguiScanCode[16], BOOL rgfKeyUp[16])
{
    SETFNAME(_T("KeybdPdd_GetEventEx2"));

    UINT32   scInProgress = 0;
    static UINT32   scPrevious;

    BOOL            fKeyUp;
    UINT8           ui8ScanCode;
    UINT            cEvents = 0;

    DEBUGCHK(rguiScanCode != NULL);
    DEBUGCHK(rgfKeyUp != NULL);

	getsFromKBCTL(&ui8ScanCode, 1);

    DEBUGMSG(ZONE_SCANCODES, 
        (_T("%s: scan code 0x%08x, code in progress 0x%08x, previous 0x%08x\r\n"),
        pszFname, ui8ScanCode, scInProgress, scPrevious));

    scInProgress = ui8ScanCode;
    if (scInProgress == scPrevious) {
        //	mdd handles auto-repeat so ignore auto-repeats from keybd
    } else {
        // Not a repeated key.  This is the real thing.
        scPrevious = scInProgress;
        
        if (ui8ScanCode & scKeyUpMask) {
            fKeyUp = TRUE;
            scInProgress &= ~scKeyUpMask;
        } else {
            fKeyUp = FALSE;
        }
        
        rguiScanCode[cEvents] = scInProgress;
		rgfKeyUp[cEvents] = fKeyUp;
        ++cEvents;
    }

    return cEvents;
}


void WINAPI KeybdPdd_ToggleKeyNotification(KEY_STATE_FLAGS	KeyStateFlags)
{
	unsigned int	fLights;

	DEBUGMSG(1, (TEXT("KeybdPdd_ToggleKeyNotification\r\n")));
	fLights = 0;
	if (KeyStateFlags & KeyShiftCapitalFlag) {
		fLights |= 0x04;
		}
	if (KeyStateFlags & KeyShiftNumLockFlag) {
		fLights |= 0x2;
		}
	/*
	Keyboard lights is disabled once driver is installed because the
	PS2 controller sends back a response which goes to the IST and corrupts
	the interface.  When we figure out how to disable the PS2 response we
	can re-enable the lights routine below
	*/

	return;
}


BOOL Ps2Keybd::IsrThreadProc()
{
	DWORD dwPriority;
    DWORD dwIrq_Keybd = 0;

	// look for our priority in the registry -- this routine sets it to zero if
	// it can't find it.
	ReadRegDWORD( TEXT("HARDWARE\\DEVICEMAP\\KEYBD"), _T("Priority256"), &dwPriority );
	if(dwPriority == 0) {
		dwPriority = 240;	// default value is 145
	}

    DEBUGMSG(1, (TEXT("IsrThreadProc:\r\n")));
    m_hevInterrupt = CreateEvent(NULL,FALSE,FALSE,NULL);
    if (m_hevInterrupt == NULL) {
        DEBUGMSG(1, (TEXT("IsrThreadProc: InterruptInitialize\r\n")));
		goto leave;
	}

	ReadRegDWORD( TEXT("HARDWARE\\DEVICEMAP\\KEYBD"), _T("Irq"), &dwIrq_Keybd );

    // Call the OAL to translate the IRQ into a SysIntr value.
    //
    if (!KernelIoControl(IOCTL_HAL_REQUEST_SYSINTR, &dwIrq_Keybd, sizeof(DWORD), &g_dwSysIntr_Keybd, sizeof(DWORD), NULL))
    {
        RETAILMSG(1, (TEXT("ERROR: Failed to obtain sysintr value for keyboard interrupt.\r\n")));
        g_dwSysIntr_Keybd = SYSINTR_UNDEFINED;
        goto leave;
    }

	if (!InterruptInitialize(g_dwSysIntr_Keybd,m_hevInterrupt,NULL,0)) {
		DEBUGMSG(1, (TEXT("IsrThreadProc: KeybdInterruptEnable\r\n")));
		goto leave;
	}

	// update the IST priority
	CeSetThreadPriority(GetCurrentThread(), (int)dwPriority);

	extern UINT v_uiPddId;
	extern PFN_KEYBD_EVENT v_pfnKeybdEvent;

	KEYBD_IST keybdIst;
	keybdIst.hevInterrupt = m_hevInterrupt;
	keybdIst.dwSysIntr_Keybd = g_dwSysIntr_Keybd;
	keybdIst.uiPddId = v_uiPddId;
	keybdIst.pfnGetKeybdEvent = KeybdPdd_GetEventEx2;
	keybdIst.pfnKeybdEvent = v_pfnKeybdEvent;
		
	KeybdIstLoop(&keybdIst);

leave:
    return 0;
}

DWORD Ps2KeybdIsrThread(Ps2Keybd *pp2k)
{
	DEBUGMSG(1,(TEXT("Ps2KeybdIsrThread:\r\n")));
	pp2k->IsrThreadProc();
	return 0;
}

BOOL Ps2Keybd::IsrThreadStart()
{
	HANDLE	hthrd;

	DEBUGMSG(1,(TEXT("IsrThreadStart:\r\n")));
	hthrd = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)Ps2KeybdIsrThread,this,0,NULL);
	//	Since we don't need the handle, close it now.
	CloseHandle(hthrd);
	return TRUE;
}

BOOL Ps2Keybd::Initialize()
{
	DEBUGMSG(1,(TEXT("Ps2Keybd::Initialize\r\n")));

	DEBUGMSG(1,(TEXT("Ps2Keybd::Initialize Done\r\n")));
	return TRUE;
}


BOOL Ps2Keybd::KeybdPowerOn()
{
    UINT8 msg[5];
    int t;
    char dummy = (char)0xff;

	DEBUGMSG(1,(TEXT("++Ps2Keybd::KeybdPowerOn\r\n")));

	// Setup IO port for SPI interface & Keyboard
	
	// Setup EINT1 (KBDINT)
    v_pIOPregs->GPFCON &= ~(0x3 << 2); 	// Clear GPF1 
    v_pIOPregs->GPFCON |= (0x2 << 2);  	// Set GPF1 to EINT1 for Keyboard interrupt

    v_pIOPregs->EXTINT0 &= ~(0x7 << 4);    // Clear EINT1
    v_pIOPregs->EXTINT0 |= (0x2 << 4);     // fallig edge triggered for EINT1

	// setup SPI interface
	// GPG5 : SPIMISO (KBDSPIMISO)
	// GPG6 : SPIMOSI (KBDSPIMOSI)
	// GPG7 : SPICLK  (KBDSPICLK)
    v_pIOPregs->GPGCON &= ~((0x3 << 10) | (0x3 << 12) | (0x3 << 14));   // Clear GPG5,6,7
    v_pIOPregs->GPGCON |= ((0x3 << 10) | (0x3 << 12) | (0x3 << 14));    
     
	// setup _SS signal(nSS_KBD)
    v_pIOPregs->GPBCON &= ~(0x3 << 12);         // Clear GPB6
    v_pIOPregs->GPBCON |= (ONEBIT << 12);        // Set Port GPB6 to output for nSS signal

	// setup _PWR_OK signal (KEYBOARD)
    v_pIOPregs->GPBCON &= ~(0x3 << 0);         // Clear GPB0 
    v_pIOPregs->GPBCON |= (ONEBIT << 0);       // Set Port GPB0 to output for _PWR_OK signal

    v_pIOPregs->GPDDAT &=~(ONEBIT << 0);        // set _PWR_OK to 0
    
	// Setup SPI registers
    // Interrupt mode, prescaler enable, master mode, active high clock, format B, normal mode
    v_pSPIregs->SPCON1 = (ONEBIT<<5)|(ONEBIT<<4)|(ONEBIT<<3)|(0x0<<2)|(ONEBIT<<1);
    
	// Developer MUST change the value of prescaler properly whenever value of PCLK is changed.
    v_pSPIregs->SPPRE1 = 255;// 99.121K = 203M/4/2/(255+1) PCLK=50.75Mhz FCLK=203Mhz SPICLK=99.121Khz
         
    for(t=0;t<20000; t++); // delay
	    msg[0] = (char)0x1b; msg[1] = (char)0xa0; msg[2] = (char)0x7b; msg[3] = (char)0; // Initialize USAR
    	for(t=0; t < 10; t++) {
    	dummy = putcToKBCTL(0xff);
    }
    
    for(t=0; t<10; t++) { // wait for a while
        putsToKBCTL(msg,3);
        for(t=0;t<20000; t++);
    }
    t = 100;
    while(t--) {
        if((v_pIOPregs->GPFDAT & 0x2)==0) { // Read _ATN
            break;
        }
    }	//check _ATN
    if(t != 0) {
        getsFromKBCTL(msg,3);
    }    
    t=100000;
    while(t--); // delay
	msg[0] = (char)0x1b; msg[1] = (char)0xa1; msg[2] = (char)0x7a; msg[3] = (char)0; //Initialization complete
	putsToKBCTL(msg,3);

	DEBUGMSG(1,(TEXT("--Ps2Keybd::KeybdPowerOn\r\n")));
	return(TRUE);
}

BOOL Ps2Keybd::KeybdPowerOff()
{
	DEBUGMSG(1,(TEXT("++Ps2Keybd::KeybdPowerOff\r\n")));

	DEBUGMSG(1,(TEXT("--Ps2Keybd::KeybdPowerOff\r\n")));
	return(TRUE);
}
