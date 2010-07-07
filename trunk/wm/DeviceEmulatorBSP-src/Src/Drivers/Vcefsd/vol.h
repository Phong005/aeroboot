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
    File:       vol.h

    Contains:   VirtualCE FSD volume level interface.

    Written by: Craig Vinet

    Copyright:  © 2002 Connectix Corporation

*/

// ------------------------------------------------------------
//                      Function Prototypes
// ------------------------------------------------------------

BOOL VCEFSD_CreateDirectoryW(	VolumeState* pVolumeState,
								PCWSTR pwsPathName, 
								LPSECURITY_ATTRIBUTES lpSecurityAttributes );

BOOL VCEFSD_RemoveDirectoryW(	VolumeState* pVolumeState, 
								PCWSTR pwsPathName );

DWORD VCEFSD_GetFileAttributesW(	VolumeState* pVolumeState,
									PCWSTR pwsFileName);

BOOL VCEFSD_SetFileAttributesW(	VolumeState* pVolumeState,
									PCWSTR pwsFileName,
									DWORD dwAttributes );


BOOL VCEFSD_DeleteFileW( 	VolumeState* pVolumeState,
							PCWSTR pwsFileName );

BOOL VCEFSD_GetDiskFreeSpaceW(	VolumeState* pVolumeState,
								PCWSTR pwsPathName,
								PDWORD pSectorsPerCluster,
								PDWORD pBytesPerSector,
								PDWORD pFreeClusters,
								PDWORD pClusters );

BOOL VCEFSD_MoveFileW(	VolumeState* pVolumeState, 
						PCWSTR pwsOldFileName, 
						PCWSTR pwsNewFileName );

BOOL VCEFSD_DeleteAndRenameFileW(	VolumeState* pVolumeState, 
									PCWSTR pwsOldFileName, 
									PCWSTR pwsNewFileName );

void VCEFSD_Notify( PVOLUME pvol, DWORD dwFlags );
