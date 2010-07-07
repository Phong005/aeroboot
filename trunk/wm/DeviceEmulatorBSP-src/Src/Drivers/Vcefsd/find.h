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
    File:       find.h

    Contains:   Contains VirtualCE file system find interface.

    Written by: Craig Vinet

    Copyright:  © 2002 Connectix Corporation
*/

#ifndef _FIND_H
#define _FIND_H

//--------------------------------------------------------------------------------
// Data
//--------------------------------------------------------------------------------

typedef struct FindContext
{
    ServerPB    fServerPB;              // Server Parameter block for searches
	WCHAR		fFindName[MAX_PATH];
	BOOL		fInUse;	
} FindContext;

//--------------------------------------------------------------------------------
// Prototypes
//--------------------------------------------------------------------------------
HANDLE	VCEFSD_FindFirstFileW( 	VolumeState *pVolume, 
								HANDLE hProc, 
								PCWSTR pwsFilePath, 
								PWIN32_FIND_DATAW pfd );

BOOL	VCEFSD_FindNextFileW(	FindContext* pFindContext, 
								PWIN32_FIND_DATAW pfd );

BOOL	VCEFSD_FindClose( FindContext* pFindContext );

int  Find( VolumeState* pVolume, ServerPB* ioServerPB, PCWSTR pwsPath );

void InitFind( void );

#endif // _FIND_H
