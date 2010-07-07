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

/* 
 * This driver uses EINT0 button as POWER ON/OFF Button.
 * 
 */

#include <windows.h>
#include <types.h>
#include <excpt.h>
#include <tchar.h>
#include <cardserv.h>
#include <cardapi.h>
#include <tuple.h>
#include <devload.h>
#include <diskio.h>
#include <nkintr.h>
#include <windev.h>

#include <s3c2410x.h>
#include "pwrbtn2410.h"

#define PRIVATE			static
#define PUBLIC

typedef DWORD (*PFN_SetSystemPowerState)(LPCWSTR, DWORD, DWORD);
PFN_SetSystemPowerState gpfnSetSystemPowerState;

PRIVATE HANDLE gPwrButtonIntrEvent;
PRIVATE HANDLE gPwrButtonIntrThread;
PRIVATE BOOL   gOffFlag;

UINT32 g_PwrButtonIrq = IRQ_EINT2;	// Determined by SMDK2410 board layout.
UINT32 g_PwrButtonSysIntr = SYSINTR_UNDEFINED;

PRIVATE volatile S3C2410X_IOPORT_REG * v_pIOPregs;


PRIVATE VOID
PBT_EnableInterrupt(VOID)
{
	v_pIOPregs->GPFCON  &= ~(0x3 << 0);		/* Set EINT0(GPF0) as EINT0							*/
	v_pIOPregs->GPFCON  |=  (0x2 << 0);

    v_pIOPregs->EXTINT0 &= ~(0x7 << 0);		/* Configure EINT0 as Falling Edge Mode				*/
    v_pIOPregs->EXTINT0 |=  (0x2 << 0);
}

PRIVATE BOOL
PBT_IsPushed(VOID)
{
	return ((v_pIOPregs->GPFDAT & (1 << 0)) ? FALSE : TRUE);
}

PRIVATE BOOL
PBT_InitializeAddresses(VOID)
{
	BOOL	RetValue = TRUE;

//	RETAILMSG(1, (TEXT(">>> PBT_initalization address..set..\r\n")));

		
	/* IO Register Allocation */

	v_pIOPregs = (volatile S3C2410X_IOPORT_REG *)VirtualAlloc(0, sizeof(S3C2410X_IOPORT_REG), MEM_RESERVE, PAGE_NOACCESS);
	if (v_pIOPregs == NULL) 
	{
		ERRORMSG(1,(TEXT("For IOPregs : VirtualAlloc failed!\r\n")));
		RetValue = FALSE;
	}
	else 
	{
		if (!VirtualCopy((PVOID)v_pIOPregs, (PVOID)(S3C2410X_BASE_REG_PA_IOPORT >> 8), sizeof(S3C2410X_IOPORT_REG), PAGE_PHYSICAL | PAGE_READWRITE | PAGE_NOCACHE)) 
		{
			ERRORMSG(1,(TEXT("For IOPregs: VirtualCopy failed!\r\n")));
			RetValue = FALSE;
		}
	}
	
	if (!RetValue) 
	{
//		RETAILMSG (1, (TEXT("::: PBT_InitializeAddresses - Fail!!\r\n") ));

		if (v_pIOPregs) 
		{
			VirtualFree((PVOID) v_pIOPregs, 0, MEM_RELEASE);
		}

		v_pIOPregs = NULL;
	}
	else RETAILMSG (1, (TEXT("::: PBT_InitializeAddresses - Success\r\n") ));

	return(RetValue);
}

DWORD
PBT_IntrThread(PVOID pArg)
{

	PBT_InitializeAddresses();

	PBT_EnableInterrupt();

	gPwrButtonIntrEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    // Request a SYSINTR value from the OAL.
    //
    if (!KernelIoControl(IOCTL_HAL_REQUEST_SYSINTR, &g_PwrButtonIrq, sizeof(UINT32), &g_PwrButtonSysIntr, sizeof(UINT32), NULL))
    {
        RETAILMSG(1, (TEXT("ERROR: PwrButton: Failed to request sysintr value for power button interrupt.\r\n")));
        return(0);
    }
    RETAILMSG(1,(TEXT("INFO: PwrButton: Mapped Irq 0x%x to SysIntr 0x%x.\r\n"), g_PwrButtonIrq, g_PwrButtonSysIntr));


	if (!(InterruptInitialize(g_PwrButtonSysIntr, gPwrButtonIntrEvent, 0, 0))) 
	{
		RETAILMSG(1, (TEXT("ERROR: PwrButton: Interrupt initialize failed.\r\n")));
	}

	while (1) 
	{
		WaitForSingleObject(gPwrButtonIntrEvent, INFINITE);
	
    	if (gOffFlag == FALSE) 
		{
			if (PBT_IsPushed())			/* To Filter Noise				*/
			{
				Sleep(200);

				if (PBT_IsPushed())
				{  
//					RETAILMSG(1, (TEXT("::: Back Light On/Off \r\n")));
				} 
				else 
				{
                                    // Soft reset and standard suspend-resume both start with suspend for now.
                                    #if (WINCEOSVER >= 400)
                                        // call whichever shutdown API is available
                                        if(gpfnSetSystemPowerState != NULL)
                                        {
                                            gpfnSetSystemPowerState(NULL, POWER_STATE_SUSPEND, POWER_FORCE);
                                        }
                                        else
                                        {
                                            PowerOffSystem();
                                        }
                                    #else
                                        PowerOffSystem();
                                    #endif
        
                                    // Give control back to system if it wants it for anything.  Not in
                                    //  power handling mode yet.  All suspend and resume operations
                                    //  will be performed in the PowerOffSystem() function.
                                    Sleep(0);
                                    
				}
			}

			InterruptDone(g_PwrButtonSysIntr);
    	}
    }        	
}


PUBLIC DWORD
DSK_Init(DWORD dwContext)
{
	PDISK	pDisk;
	DWORD   IDThread;
        HMODULE hmCore;


	pDisk = (PDISK)dwContext;


        // contain the appropriate APIs.
        gpfnSetSystemPowerState = NULL;
        hmCore = (HMODULE) LoadLibrary(_T("coredll.dll"));
        if(hmCore != NULL) {
            gpfnSetSystemPowerState = (PFN_SetSystemPowerState) GetProcAddress(hmCore, _T("SetSystemPowerState"));
            if(gpfnSetSystemPowerState == NULL) {
                FreeLibrary(hmCore);
            }
	}

	do 
	{
		gPwrButtonIntrThread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE) PBT_IntrThread, 0, 0, &IDThread);

//		RETAILMSG(1, (TEXT("::: PBT_IntrThread ID = %x\r\n"), IDThread));

		if (gPwrButtonIntrThread == 0) 
		{
//			RETAILMSG(1, (TEXT("::: CreateThread() Fail\r\n")));
			break;
		}
	} while (0);

//    RETAILMSG(1, (TEXT("::: PBT_Init Out\r\n")));

	pDisk->d_DiskCardState = 1;

	return (DWORD)pDisk;
}

PUBLIC BOOL WINAPI
DllEntry(HANDLE hInstDll, DWORD dwReason, LPVOID lpvReserved)
{
    switch ( dwReason ) 
	{
    case DLL_PROCESS_ATTACH:
//        RETAILMSG(1, (TEXT("PwrButton : DLL_PROCESS_ATTACH\r\n")));
		DisableThreadLibraryCalls((HMODULE) hInstDll);
        break;

    }
    return (TRUE);
}

BOOL
DSK_Close(
    DWORD Handle
    )
{
    return TRUE;
}   // DSK_Close


//
// Device deinit - devices are expected to close down.
// The device manager does not check the return code.
//
BOOL
DSK_Deinit(
    DWORD dwContext     // future: pointer to the per disk structure
    )
{
    return TRUE;
}   // DSK_Deinit


//
// Returns handle value for the open instance.
//
DWORD
DSK_Open(
    DWORD dwData,
    DWORD dwAccess,
    DWORD dwShareMode
    )
{
    return 0;
}   // DSK_Open

//
// I/O Control function - responds to info, read and write control codes.
// The read and write take a scatter/gather list in pInBuf
//
BOOL
DSK_IOControl(
    DWORD Handle,
    DWORD dwIoControlCode,
    PBYTE pInBuf,
    DWORD nInBufSize,
    PBYTE pOutBuf,
    DWORD nOutBufSize,
    PDWORD pBytesReturned
    )
{
    return FALSE;
}   // DSK_IOControl


DWORD DSK_Read(DWORD Handle, LPVOID pBuffer, DWORD dwNumBytes)
{
	return 0;
}
DWORD DSK_Write(DWORD Handle, LPCVOID pBuffer, DWORD dwNumBytes)
{
	return 0;
}
DWORD DSK_Seek(DWORD Handle, long lDistance, DWORD dwMoveMethod)
{
	return 0;
}

void DSK_PowerUp(void)
{
	return;
}

void
DSK_PowerDown(void)
{
	return;
}
