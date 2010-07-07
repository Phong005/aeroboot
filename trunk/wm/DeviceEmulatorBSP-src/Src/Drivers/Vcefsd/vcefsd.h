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
    File:       vcefsd.h

    Contains:   VirtualCE FSD interface header.

    Written by: Craig Vinet

    Copyright:  © 2002 Connectix Corporation

*/
#ifndef _VCEFSD_H
#define _VCEFSD_H

#if defined(_DEBUG) && !defined(DEBUG)
#define DEBUG
#endif


#include <windows.h>
#include <tchar.h>
#include <types.h>
#include <excpt.h>
#include <memory.h>
#include <diskio.h>
#include <fsdmgr.h>

#define KITL_MAX_DATA_SIZE 1446


#ifdef UNDER_CE
#define GetSystemTimeAsFileTime(pft) { \
        SYSTEMTIME st; \
        GetSystemTime(&st); \
        SystemTimeToFileTime(&st, pft); \
}
#endif


#define ENTER_BREAK_SCOPE switch(TRUE) { case TRUE:
#define LEAVE_BREAK_SCOPE }

#define INVALID_PTR             ((PVOID)0xFFFFFFFF)

#ifdef DEBUG
#define DEBUGONLY(s)            s
#define RETAILONLY(s)           
#define VERIFYTRUE(c)           DEBUGCHK(c)
#define VERIFYNULL(c)           DEBUGCHK(!(c))
#else
#define DEBUGONLY(s)
#define RETAILONLY(s)           s
#define VERIFYTRUE(c)           c
#define VERIFYNULL(c)           c
#endif

#ifndef ERRFALSE
#define ERRFALSE(exp)           extern char __ERRXX[(exp)!=0]
#endif


/*  Debug-zone stuff
 */

#ifdef DEBUG


#define DEBUGBREAK(cond)         if (cond) DebugBreak(); else
#define DEBUGMSGBREAK(cond,msg)  if (cond) {DEBUGMSG(TRUE,msg); DebugBreak();} else
#define DEBUGMSGWBREAK(cond,msg) if (cond) {DEBUGMSGW(TRUE,msg); DebugBreak();} else

#else   // !DEBUG

#define DEBUGBREAK(cond)
#define DEBUGMSGBREAK(cond,msg)
#define DEBUGMSGWBREAK(cond,msg)
#endif


#define BLOCK_SIZE              512     


#ifdef DEBUG

/*****************************************************************************/

/* debug zones */
#define ZONEID_INIT  0
#define ZONEID_APIS  1
#define ZONEID_ERROR 2
#define ZONEID_CREATE 3

/* zone masks */
#define ZONEMASK_INIT   (1 << ZONEID_INIT)
#define ZONEMASK_APIS   (1 << ZONEID_APIS)
#define ZONEMASK_ERROR  (1 << ZONEID_ERROR)
#define ZONEMASK_CREATE (1 << ZONEID_CREATE)


/* these are used as the first arg to DEBUGMSG */
#define ZONE_INIT   DEBUGZONE(ZONEID_INIT)
#define ZONE_APIS   DEBUGZONE(ZONEID_APIS)
#define ZONE_ERROR  DEBUGZONE(ZONEID_ERROR)
#define ZONE_CREATE DEBUGZONE(ZONEID_CREATE)

/*****************************************************************************/


#endif /* DEBUG_H */

#define SEEK_SET    0        /* seek to an absolute position */
#define SEEK_CUR    1        /* seek relative to current position */
#define SEEK_END    2        /* seek relative to end of file */

#define EFSBUF_SIZE (64*1024)	// this size is hard-coded in the DeviceEmulator:  do not change.

typedef struct
{
    BOOL        fConnected;         // This connection is valid
} Connection, PConnection;

typedef struct VolumeState
{
    HVOL 		vs_Handle;
} VolumeState;

typedef struct
{
    int          fs_Handle;
    VolumeState *fs_Volume;
} FileState;

typedef struct FileData
{
    ULONG			fSignature;             // Our debug signature
    BOOL			fInUse;                 // This record is in use
    USHORT			fHandle;                // Handle to the open file on the server
    void*			fResHandle;             // Which net resource handle this file is on
    ULONG			fPosition;              // Current file position
    ULONG			fSize;                  // Current file size
    BOOL			fWritten;               // TRUE if file has been written to
    BOOL			fSetAttrOnClose;        // We need to set the actuall attributes on close
    ULONG			fFileAttributes;        // Actual file attributes
    ULONG			fFileTimeDate;          // last modify time
    ULONG			fFileCreateTimeDate;	// yep
	VolumeState*	pVolumeState;			// needed for CE's special FSD_GetFileInformationByHandle method
	WCHAR			fFileName[MAX_PATH];		// um, yeah...the file's name.				
} FileData;


extern LPCRITICAL_SECTION g_pcsMain;

#define EnterCriticalSectionMM(pcs) { \
    EnterCriticalSection (pcs); \
}                                   

#define LeaveCriticalSectionMM(pcs) {  \
    LeaveCriticalSection (pcs); \
}   

extern HANDLE g_FolderShareMutex;

typedef struct {
    unsigned __int32 ServerPB;
    unsigned __int32 Code;
    unsigned __int32 IOPending;
    unsigned __int32 Result;
    } FolderSharingDevice;

volatile FolderSharingDevice *v_FolderShareDevice;

#endif /* _VCEFSD_H */
