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
    File:       main.c

    Contains:   VirtualCE FSD DLL main.

    Written by: Craig Vinet

    Copyright:  © 2002 Connectix Corporation
*/

#include "vcefsd.h"
#include "file.h"
#include "fserver.h"
#include "find.h"
#include <ceddk.h>

//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
#ifdef DEBUG

DBGPARAM dpCurSettings =
{
    TEXT("VCEFSD"),
    {
        TEXT("Init"),
        TEXT("Api"),
        TEXT("Error"),
        TEXT("Create"),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT("")
    },
#if 1
    ZONEMASK_ERROR | ZONEMASK_CREATE
#else
    0xFFFF
#endif
    
};

#endif


//--------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------
VolumeState 		gVolumeState = { 0 }; // init
CRITICAL_SECTION 	g_csMain;
LPCRITICAL_SECTION 	g_pcsMain;
HANDLE                  g_FolderShareMutex;

PUCHAR		gvpReadWriteBuffer = NULL;	// Virtual addr.
ServerPB*	gvpServerPB = NULL;			// Virtual addr.

PUCHAR		gpReadWriteBuffer = NULL;	// Physical addr.
ServerPB*	gpServerPB = NULL;			// Physical addr.

//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL VCEFSD_MountDisk( HDSK hdsk )
{
    BOOL 		fSuccess = FALSE;
    DWORD		error;
	DMA_ADAPTER_OBJECT DmaAdapter;
	PHYSICAL_ADDRESS PhysicalAddress;

    DEBUGMSG(ZONE_INIT, (L"VCEFSD: FSD_MountDisk\n"));

    EnterCriticalSectionMM(g_pcsMain);

	if( gVolumeState.vs_Handle == 0 )
	{
		// Initialize global memory usage...

            // DeviceEmulator Folder Sharing Device
            v_FolderShareDevice = VirtualAlloc(NULL, sizeof(*v_FolderShareDevice), MEM_RESERVE, PAGE_NOACCESS);
            if (v_FolderShareDevice == NULL) {
		DEBUGMSG(ZONE_ERROR, (L"VCEFSD_MountDisk ERROR: FolderShareDevice VirtualAlloc Failed\n") );
                fSuccess = FALSE;
                goto Done;
            }
            fSuccess = VirtualCopy((LPVOID)v_FolderShareDevice, (PVOID)(0x500f4000>>8), sizeof(*v_FolderShareDevice), PAGE_READWRITE|PAGE_NOCACHE|PAGE_PHYSICAL);
            if (fSuccess == FALSE) {
                error = GetLastError();
		DEBUGMSG(ZONE_ERROR, (L"VCEFSD_MountDisk ERROR: FolderShareDevice VirtualCopy Failed\n") );
                goto Done;
            }

		// Read/Write buffer...
		DmaAdapter.ObjectSize = sizeof(DmaAdapter);
		DmaAdapter.InterfaceType = Internal;
		DmaAdapter.BusNumber = 0;

		gvpReadWriteBuffer = (PUCHAR)HalAllocateCommonBuffer(&DmaAdapter, EFSBUF_SIZE, &PhysicalAddress, FALSE);
		if (gvpReadWriteBuffer == NULL) {
			fSuccess = FALSE;
			goto Done;
		}
		gpReadWriteBuffer = (PUCHAR)PhysicalAddress.LowPart;

		// Global PB buffer...
		gvpServerPB = (ServerPB *)HalAllocateCommonBuffer(&DmaAdapter, sizeof(ServerPB), &PhysicalAddress, FALSE);
		if (gvpServerPB == NULL) {
			HalFreeCommonBuffer(NULL, 0, PhysicalAddress, gvpReadWriteBuffer, FALSE);
			fSuccess = FALSE;
			goto Done;
		}
		gpServerPB = (ServerPB*)PhysicalAddress.LowPart;

		memset( gvpServerPB, 0, sizeof( ServerPB ) );
                gvpServerPB->fStructureSize = sizeof(ServerPB);

		// Ask the server for the drive info
		ServerGetDriveConfig( gvpServerPB );
		if ( gvpServerPB->fResult != 0 ) // Gack!
		{
		   goto Done;
		}

		gVolumeState.vs_Handle = FSDMGR_RegisterVolume(hdsk, L"Storage Card", (DWORD)&gVolumeState);

		if (gVolumeState.vs_Handle)
		{
		  fSuccess = TRUE;
		  DEBUGMSG( ZONE_ERROR, (L"Mounted VCEFSD volume '\\Storage Card'\n") );
		}

		// Get the maximum read and write buffer size.
		ServerGetMaxIOSize();

		// Happiness in slavery...er, I mean init FSD stuff
		InitFiles();
		InitFind();
	}
Done:;
    LeaveCriticalSectionMM(g_pcsMain);
    return fSuccess;
}


//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL VCEFSD_UnmountDisk(HDSK hdsk)
{
	EnterCriticalSectionMM(g_pcsMain);

	if( gVolumeState.vs_Handle != 0 )
	{
		CloseFiles();
		FSDMGR_DeregisterVolume( gVolumeState.vs_Handle );

		// No more calls to ServerIO functions!
		if( gvpReadWriteBuffer != NULL )
			VirtualFree( gvpReadWriteBuffer, 0, MEM_RELEASE );

		if( gvpServerPB != NULL )
			VirtualFree( gvpServerPB, 0, MEM_RELEASE );

		gVolumeState.vs_Handle = 0;
	}
	LeaveCriticalSectionMM(g_pcsMain);
	return TRUE;
}

SHELLFILECHANGEFUNC_t	gpnShellNotify = NULL;

//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL VCEFSD_RegisterFileSystemFunction(PVOLUME pvol, SHELLFILECHANGEFUNC_t pfn)
{
    gpnShellNotify = pfn;
    return TRUE;
}

//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL WINAPI DllMain(HINSTANCE DllInstance, DWORD dwReason, LPVOID Reserved)
{
    switch(dwReason) {

    case DLL_PROCESS_ATTACH:
        {
            DisableThreadLibraryCalls( (HMODULE)DllInstance);
            DEBUGREGISTER(DllInstance);
            DEBUGMSG(ZONE_INIT,(TEXT("VCEFSD!DllMain: DLL_PROCESS_ATTACH\n")));
            g_pcsMain = MapPtrToProcess( &g_csMain, GetCurrentProcess());
            InitializeCriticalSection(g_pcsMain);
            g_FolderShareMutex = CreateMutex(NULL, FALSE, L"DeviceEmulatorFolderSharingDevice");
            if (g_FolderShareMutex == NULL) {
                return FALSE;
            }
        }            
        break;
    case DLL_PROCESS_DETACH:
        {
			if (gvpReadWriteBuffer != NULL) {
				PHYSICAL_ADDRESS p; // the only arg actually used is gvpReadWriteBuffer
				p.QuadPart = 0;
			    HalFreeCommonBuffer(NULL, EFSBUF_SIZE, p, gvpReadWriteBuffer, FALSE);
			}
			if (gvpServerPB != NULL) {
				PHYSICAL_ADDRESS p; // the only arg actually used is gvpServerPB
				p.QuadPart = 0;
			    HalFreeCommonBuffer(NULL, sizeof(ServerPB), p, gvpServerPB, FALSE);
			}

            DeleteCriticalSection(g_pcsMain);
            CloseHandle(g_FolderShareMutex);
            DEBUGMSG(ZONE_INIT,(TEXT("VCEFSD!DllMain: DLL_PROCESS_DETACH\n")));
        }
        break;
    default:
        break;
    }
    return TRUE;
}
