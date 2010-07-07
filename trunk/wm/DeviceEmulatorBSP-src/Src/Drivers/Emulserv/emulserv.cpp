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
/*==================================================================
	File:		emulserv.cpp
	
	Contains:	Pseudo service device to arbitrate emulator interrupt SYSINTR_EMULSERV
				usage.
				
	Written by:	Craig vinet
	
	Copyright:	2002 Connectix Corporation
==================================================================*/

#include <windows.h>
#include "emulserv.h"

#include <wdm.h> // for READ_PORT_XXXX, WRITE_PORT_XXXX
#include <Pkfuncs.h>
#include <nkintr.h>
#include <cardserv.h>
#include <devload.h>
#include <winnls.h>

#include <storemgr.h>
#include <diskio.h>
#include <s3c2410x_ioport.h>
#include <s3c2410x_base_regs.h>

#include <vcefsd.h>
#include "fserver.h"

#define SYSINTR_EMULSERV (SYSINTR_FIRMWARE+15) // TODO: Get some other way

// Layout of the DeviceEmulator's EmulServ device
typedef struct {
	unsigned __int32 InterruptMask;
	unsigned __int32 InterruptPending;
} EmulServDevice;

// Holds data for the device
struct DeviceInfo
{
	DeviceInfo()				// Constructor
	{
		fInterruptEvent = 0;		// Event signaled when interrupt occurs
		fInterruptID = 0;			// IRQ of the interrupt
		fIntrServiceThread = NULL;	// Handle to the interrupt servicing thread (IST)
	}			
	
	HANDLE		fInterruptEvent;	// fInterruptEvent signaled when interrupt occurs
	HANDLE		fIntrServiceThread;	// Handle to InterruptServiceThread
	DWORD		fInterruptID;		// Logical interrupt ID
};


/*------------------------------------------------------------------
	Defines and const
------------------------------------------------------------------*/

// Emulator Services IRQ
const WCHAR *g_szDrvRegPath = L"Drivers\\EMULSERV";

/*------------------------------------------------------------------
	Globals
------------------------------------------------------------------*/
BOOL 		deviceInitialized = FALSE;
DeviceInfo	deviceInfo;

HANDLE hDev = NULL;
static HANDLE g_hFSD = NULL;

#define VCEFSD_PROFILE     TEXT("VCEFSD")

CRITICAL_SECTION 	g_csMain;
LPCRITICAL_SECTION	g_pcsMain;

volatile EmulServDevice *v_EmulServDevice;

/*------------------------------------------------------------------
	Local Prototypes
------------------------------------------------------------------*/
static DWORD WINAPI InterruptServiceThread( PVOID Empty );
static void ProcessFolderSharingChange();

static BOOL StartIST();
static void StopIST();

static BOOL InitializeGlobals();
static void DeInitializeGlobals();

static ULONG AckInterrupt();
static VOID InstallInterrupt();
static VOID UninstallInterrupt();

static DWORD GetDiskInfo(	PDISK_INFO pInfo );
static BOOL GetDeviceInfo(PSTORAGEDEVICEINFO pInfo);


/*------------------------------------------------------------------
	DllMain
	
	Entry point called when a process hooks up to the DLL.
------------------------------------------------------------------*/

BOOL APIENTRY DllMain(HANDLE hinstDLL, DWORD fdwReason, LPVOID lpv)
{
	switch ( fdwReason )
	{
		case DLL_PROCESS_ATTACH:
			if ( deviceInitialized == FALSE )
			{
				return InitializeGlobals();
			}
			break;
		case DLL_PROCESS_DETACH:
			if( deviceInitialized == TRUE )
			{
				DeInitializeGlobals();
			}
			break;
		case DLL_THREAD_ATTACH:
		 	break;
		case DLL_THREAD_DETACH:
			break;
	}
	
	return TRUE;
}


/*------------------------------------------------------------------
	InterruptServiceThread
	
	When interrupt occurs on IRQ 10, this thread is triggered to 
	dispatch interrupt.

------------------------------------------------------------------*/

#define kFServerChangedServiceMask		0x40000000

static DWORD WINAPI
InterruptServiceThread( PVOID /*ignore*/ )   // NULL pointer does nothing.
{
	while ( TRUE )		// loop forever
	{
	 	// Wait for interrrupt event
		if ( WaitForSingleObject( deviceInfo.fInterruptEvent, INFINITE ) == WAIT_FAILED ) // Something bad happened
		{
			continue;
		}
		
		EnterCriticalSection (g_pcsMain);

		DEBUGMSG( TRUE, (TEXT("Emulator Services:InterruptServiceThread() Interrupt: Fired!\n")) );
		
		// Ack int & Dispatch events here
		ULONG event = AckInterrupt();

		if( event & kFServerChangedServiceMask )
		{
			ProcessFolderSharingChange();
		}

		InterruptDone( deviceInfo.fInterruptID );

		LeaveCriticalSection (g_pcsMain);
	}
    return(0);
}

/*------------------------------------------------------------------
    ProcessFolderSharingChange
    
    Checks the Device Emulator to see if folder sharing is enabled.
    If it is enabled and the shared folder is mounted, unmount the folder.
    If it is enabled and the shared folder is unmounted, mount the folder.
------------------------------------------------------------------*/
static void ProcessFolderSharingChange()

{
    
    DEBUGMSG( TRUE, (TEXT("Emulator Services:+ProcessFolderSharingChange()\r\n")) );
    
    volatile FolderSharingDevice *v_tempFolderShareDevice;
    DWORD error;

    v_tempFolderShareDevice = (volatile FolderSharingDevice*)VirtualAlloc(NULL, sizeof(*v_tempFolderShareDevice), MEM_RESERVE, PAGE_NOACCESS);
    if (v_tempFolderShareDevice == NULL)
    {
        //DEBUGMSG(ZONE_ERROR, (L"EMULSERV_ProcessFolderSharingChange ERROR: FolderShareDevice VirtualAlloc Failed\n") );
        goto Done;
    }
    if (!VirtualCopy((LPVOID)v_tempFolderShareDevice, (PVOID)(0x500f4000>>8), sizeof(*v_tempFolderShareDevice), PAGE_READWRITE|PAGE_NOCACHE|PAGE_PHYSICAL))
    {
        error = GetLastError();
        //DEBUGMSG(ZONE_ERROR, (L"EMULSERV_ProcessFolderSharingChange ERROR: FolderShareDevice VirtualCopy Failed\n") );
        goto Done;
    }

    v_tempFolderShareDevice->Code = kServerPollCompletion;
    unsigned __int32 mounted = v_tempFolderShareDevice->Result;
    VirtualFree((LPVOID*)v_tempFolderShareDevice, 0, MEM_RELEASE);
    if(mounted != kErrorInvalidFunction)
    {
        // folder sharing enabled
        DEBUGMSG( TRUE, (TEXT("Emulator Services:Folder sharing is enabled()\r\n")) );
        if(g_hFSD == NULL)
        {
            DEBUGMSG( TRUE, (TEXT("Emulator Services:+Mounting Device()\r\n")) );
            g_hFSD = ActivateDevice(g_szDrvRegPath, 0);
            DEBUGMSG( TRUE, (TEXT("Emulator Services:-Mounting Device()\r\n")) );
        }
    }
    else
    {
        DEBUGMSG( TRUE, (TEXT("Emulator Services:Folder sharing NOT enabled()\r\n")) );
        if(g_hFSD != NULL)
        {
            DEBUGMSG( TRUE, (TEXT("Emulator Services:+Unmounting Device()\r\n")) );
            DeactivateDevice(g_hFSD);
            DEBUGMSG( TRUE, (TEXT("Emulator Services:-Unmounting Device()\r\n")) );
            g_hFSD = NULL;
        }
    }
Done:

    DEBUGMSG( TRUE, (TEXT("Emulator Services:-ProcessFolderSharingChange()\r\n")) );
}



/*------------------------------------------------------------------
	StartIST
	
	Starts the interrupt service thread
	Returns: Success(true) or failure(false)
------------------------------------------------------------------*/

BOOL
StartIST()
{
    
    ULONG firstEvent = AckInterrupt();
    if( firstEvent & kFServerChangedServiceMask )
    {
        ProcessFolderSharingChange();
    }

	// Initialize the interrupt to be associated with the fInterruptEvent event
    if ( !InterruptInitialize(deviceInfo.fInterruptID, deviceInfo.fInterruptEvent, NULL, 0) ) 
	{
		DEBUGMSG( TRUE, (TEXT("Emulator Services:StartIST() failed on InterruptInitialize.\n")) );
        return FALSE;
    }
	
    // Create the thread
	deviceInfo.fIntrServiceThread = CreateThread(NULL, 0, InterruptServiceThread, NULL, 0, NULL);
	
    if ( deviceInfo.fIntrServiceThread == NULL ) // Failure
	{
		DEBUGMSG( TRUE, (TEXT("Emulator Services:StartIST() failed to create interrupt service thread.\n")) );
        return FALSE;
    }
	
	InstallInterrupt();

	// Set thread priority above normal
    //CeSetThreadPriority( deviceInfo.fIntrServiceThread, THREAD_PRIORITY_ABOVE_NORMAL );
	SetThreadPriority(deviceInfo.fIntrServiceThread, THREAD_PRIORITY_HIGHEST);
	
	DEBUGMSG( TRUE, (TEXT("Emulator Services:StartIST() succeeded.\n")) );
	
	return TRUE;
}

/*------------------------------------------------------------------
	StopIST
------------------------------------------------------------------*/

void
StopIST()
{
	UninstallInterrupt();
}


/*------------------------------------------------------------------
	InitializeGlobals
	
	Initializations that should only occur once for the
	entire life of the driver
------------------------------------------------------------------*/

BOOL InitializeGlobals()
{
	// Set logical interrupt ID
	BOOL fRet = FALSE;
    volatile S3C2410X_IOPORT_REG *vpIOPRegs = NULL;

    ASSERT(v_EmulServDevice == NULL);
    
    DWORD dwIrq = 39;
    fRet = KernelIoControl(IOCTL_HAL_REQUEST_SYSINTR, &dwIrq, sizeof(UINT32), &deviceInfo.fInterruptID, sizeof(UINT32), NULL);
    if (fRet == FALSE) {
		DEBUGMSG (1,(TEXT("IOCTL_HAL_REQUEST_SYSINTR for irq 0x%x failed\n\r"), dwIrq));
        goto EXIT;
    }

	v_EmulServDevice = (volatile EmulServDevice*)VirtualAlloc(NULL, sizeof(*v_EmulServDevice), MEM_RESERVE, PAGE_NOACCESS);
	if (v_EmulServDevice == NULL)
	{
		DEBUGMSG (1,(TEXT("v_EmulServDevice is not allocated\n\r")));
		fRet = FALSE;
        goto EXIT;
	}
	fRet = VirtualCopy((LPVOID)v_EmulServDevice, (PVOID)(0x500f5000>>8), sizeof(*v_EmulServDevice), PAGE_READWRITE|PAGE_NOCACHE|PAGE_PHYSICAL); 
	if (fRet == FALSE) {
		DEBUGMSG (1,(TEXT("v_EmulServDevice is not mapped\n\r")));
        goto EXIT;
	}
	DEBUGMSG (1,(TEXT("v_EmulServDevice is mapped to %x\n\r"), v_EmulServDevice));

	vpIOPRegs = (S3C2410X_IOPORT_REG*)VirtualAlloc(0, sizeof(S3C2410X_IOPORT_REG), MEM_RESERVE, PAGE_NOACCESS);
	if (vpIOPRegs == NULL) 
	{
		DEBUGMSG (1,(TEXT("m_vpIOPRegs is not allocated\n\r")));
		fRet = FALSE;
        goto EXIT;
	}
	fRet = VirtualCopy((PVOID)vpIOPRegs, (PVOID)(S3C2410X_BASE_REG_PA_IOPORT >> 8), sizeof(S3C2410X_IOPORT_REG), PAGE_PHYSICAL|PAGE_READWRITE|PAGE_NOCACHE);
    if (fRet == FALSE) {
		DEBUGMSG (1,(TEXT("m_vpIOPRegs is not mapped\n\r")));
		goto EXIT;
	}
	DEBUGMSG (1,(TEXT("m_vpIOPRegs is mapped to %x\n\r"), vpIOPRegs));

	vpIOPRegs->GPFCON = (vpIOPRegs->GPFCON & ~(0x3<<0x6)) | (0x2<<0x6); 
	vpIOPRegs->GPFUP  = (vpIOPRegs->GPFUP | (0x1<<0x3));    
	vpIOPRegs->EXTINT1 =(vpIOPRegs->EXTINT1 & ~(0xf<<0xc)) | (0x1<<0xc); 
	
	// Create events
	deviceInfo.fInterruptEvent	= CreateEvent(0, FALSE, FALSE, NULL);
	
	// Make sure events are valid
	if ( !deviceInfo.fInterruptEvent )
	{
        fRet = FALSE;
        goto EXIT;
	}
	
	// Hook the interrupt and start the associated thread.
	if ( !StartIST() ) 
	{
        fRet = FALSE;
        goto EXIT;
	}

	g_pcsMain = ( LPCRITICAL_SECTION )MapPtrToProcess( &g_csMain, GetCurrentProcess());
    InitializeCriticalSection(g_pcsMain);        

	deviceInitialized = TRUE;
    fRet = TRUE;

EXIT:    
    if (fRet == FALSE) {
        if (v_EmulServDevice) {
            VirtualFree((LPVOID)v_EmulServDevice, 0, MEM_RELEASE);
            v_EmulServDevice = NULL;
        }
    }

    if (vpIOPRegs) {
        VirtualFree((LPVOID)vpIOPRegs, 0, MEM_RELEASE);
    }
    
	return fRet;
}

/*------------------------------------------------------------------
	DeInitializeGlobals
------------------------------------------------------------------*/

void 
DeInitializeGlobals(void)
{
	StopIST();
	DeleteCriticalSection(g_pcsMain);
	deviceInitialized = FALSE;
}


//------------------------------------------------------------------
// AckInterrupt
//------------------------------------------------------------------


ULONG 
AckInterrupt()
{
    DEBUGMSG( TRUE, (TEXT("EmulatorServices:AckInterrupt() In ISR, ACKing Interrupt")) );

	return v_EmulServDevice->InterruptPending;
}

VOID
InstallInterrupt()
{
    DEBUGMSG( TRUE, (TEXT("EmulatorServices:InitInterrupt() Install Interrupt")) );

	v_EmulServDevice->InterruptMask = 0xffffffff; // Unmask all interrupts
}


VOID
UninstallInterrupt()
{
    DEBUGMSG( TRUE, (TEXT("EmulatorServices:InitInterrupt() Uninstall Interrupt")) );

	v_EmulServDevice->InterruptMask = 0;
}


/*------------------------------------------------------------------
	EMS_Init
	
	Standard stream interface Init function. Does nothing here.
------------------------------------------------------------------*/

DWORD
EMS_Init( DWORD /*registryPath*/ )
{
	return 1;
}


/*------------------------------------------------------------------
	EMS_Deinit
	
	Standard stream interface Deinit function. Does nothing here.
------------------------------------------------------------------*/

BOOL
EMS_Deinit( DWORD /*dwData*/ )
{	
	return TRUE;
}


/*------------------------------------------------------------------
	EMS_Open
	
	Standard stream interface Open function. Does nothing here.
------------------------------------------------------------------*/

extern "C"  DWORD
EMS_Open( 	DWORD /*dwData*/,
	  		DWORD /*access*/,
	  		DWORD /*shareMode*/ )  
{
	return 0;
}


/*------------------------------------------------------------------
	EMS_Close
	
	Standard stream interface Close function. Does nothing here.
------------------------------------------------------------------*/

BOOL
EMS_Close( DWORD /*dwData*/ )
{
	return TRUE;
}


/*------------------------------------------------------------------
	EMS_Write
	
	Standard stream interface Write function. Does nothing here.
------------------------------------------------------------------*/

DWORD
EMS_Write( DWORD	/*dwData*/,	
		   LPCVOID	/*writeBytes*/,	
		   DWORD	/*numBytes*/ )
{
	return 0;
}


/*------------------------------------------------------------------
	EMS_Read
	
	Standard stream interface Read function. Does nothing here.
------------------------------------------------------------------*/

DWORD
EMS_Read( DWORD		/*dwData*/,
		  LPVOID	/*buffer*/,
		  DWORD		/*numRead*/ )
{
	return 0;
}


/*------------------------------------------------------------------
	EMS_IOControl
	
	Standard stream interface IOControl function. Does nothing here.
------------------------------------------------------------------*/

extern "C"  BOOL
EMS_IOControl( DWORD	/*dwData*/,		
			   DWORD	/*code*/,		
			   PBYTE	/*inBuf*/,		
			   DWORD	/*inLength*/,	
			   DWORD	/*outLength*/,	
			   PBYTE	/*outBuf*/,	
			   PDWORD	/*actualOut*/	)
{
	return FALSE;
}


/*------------------------------------------------------------------
	EMS_PowerUp
	
	Standard stream interface PowerUp function. Does nothing here.
------------------------------------------------------------------*/

extern "C"  void
EMS_PowerUp( DWORD /*hContext*/ )
{	
	// do nothing
}


/*------------------------------------------------------------------
	EMS_PowerDown
	
	Standard stream interface PowerDown function. Does nothing here.
------------------------------------------------------------------*/
extern "C"  void
EMS_PowerDown( DWORD /*hContext*/ )
{	
	// do nothing
}


/*------------------------------------------------------------------
	EMS_Seek
	
	Standard stream interface Seek function. Does nothing here.
------------------------------------------------------------------*/

extern "C"  DWORD
EMS_Seek( DWORD /*hOpenContext*/, 
		  long /*amount*/, 
		  WORD /*type*/ )
{
	// Do nothing, return fail
	return -1;
}


//------------------------------------------------------------------------
//
//------------------------------------------------------------------------
DWORD DSK_Init(DWORD dwContext)
{
    return 1;
}


//------------------------------------------------------------------------
//
//------------------------------------------------------------------------
DWORD
DSK_Open(
    DWORD dwData,
    DWORD dwAccess,
    DWORD dwShareMode
    )
{
    return 1;
}


//------------------------------------------------------------------------
//
//------------------------------------------------------------------------
BOOL DSK_DeinitClose(DWORD dwContext)
{
    return TRUE;
}


//------------------------------------------------------------------------
//
//------------------------------------------------------------------------
DWORD DSK_Dud(DWORD dwData,DWORD dwAccess,DWORD dwShareMode)
{
    return dwData;
}


//------------------------------------------------------------------------
//
//------------------------------------------------------------------------
void DSK_Power(void)
{
}


//------------------------------------------------------------------------
//
//------------------------------------------------------------------------
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
    BOOL initted = (BOOL)Handle;

    DEBUGMSG( TRUE, (TEXT("Emulator Services: +DSK_IOControl\n")) );

    if( !initted ) 
    {
        SetLastError(ERROR_INVALID_HANDLE);
        DEBUGMSG(TRUE, (TEXT("-DSK_IOControl (invalid disk) \r\n")));
        return FALSE;
    }

     //
    // Check parameters
    //
    switch (dwIoControlCode) 
    {
	    case DISK_IOCTL_GETINFO:
	        if (pInBuf == NULL) {
	            SetLastError(ERROR_INVALID_PARAMETER);
	            return FALSE;
	        }
	        break;


	    case IOCTL_DISK_DEVICE_INFO:
	        if(!pInBuf || nInBufSize != sizeof(STORAGEDEVICEINFO)) {
	            SetLastError(ERROR_INVALID_PARAMETER);   
	            return FALSE;
	        }
	        break;

	    default:
	        SetLastError(ERROR_INVALID_PARAMETER);
	        return FALSE;
    }

    //
    // Execute dwIoControlCode
    //
    switch (dwIoControlCode)
    {
	    case DISK_IOCTL_GETINFO:
	        SetLastError(GetDiskInfo((PDISK_INFO)pInBuf));
			DEBUGMSG( TRUE, (TEXT("Emulator Services: -DSK_IOControl:DISK_IOCTL_GETINFO\n")) );
	        return TRUE;

	    case IOCTL_DISK_DEVICE_INFO: // new ioctl for disk info
	        DEBUGMSG( TRUE, (TEXT("Emulator Services: -DSK_IOControl:IOCTL_DISK_DEVICE_INFO\n")) );
	        return GetDeviceInfo((PSTORAGEDEVICEINFO)pInBuf);

	    default:
	        DEBUGMSG( TRUE, (TEXT("Emulator Services: -DSK_IOControl:default\n")) );
	        SetLastError(ERROR_INVALID_PARAMETER);
	        return FALSE;
    }
}   // DSK_IOControl


//------------------------------------------------------------------------
//
//------------------------------------------------------------------------
DWORD GetDiskInfo(	PDISK_INFO pInfo )
{
    pInfo->di_flags = 0;
    return ERROR_SUCCESS;
}   // GetDiskInfo


//------------------------------------------------------------------------
//
//------------------------------------------------------------------------
BOOL GetDeviceInfo(PSTORAGEDEVICEINFO pInfo)
{
    BOOL fRet = FALSE;
    
    if( pInfo )
    {
        _tcsncpy(pInfo->szProfile, VCEFSD_PROFILE, PROFILENAMESIZE);
        
        // set our class, type, and flags
       pInfo->dwDeviceClass = STORAGE_DEVICE_CLASS_MULTIMEDIA;
       pInfo->dwDeviceType = STORAGE_DEVICE_TYPE_UNKNOWN;
       pInfo->dwDeviceFlags = STORAGE_DEVICE_FLAG_READWRITE;
       fRet = TRUE;
    }
    return fRet;
}

/*==================================================================
	Change History (most recent first):
	
	$Log: emulserv.cpp,v $
	
==================================================================*/
