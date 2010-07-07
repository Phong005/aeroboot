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
    File:       File.h

    Contains:   VirtualCE FSD file interface.

    Written by: Craig Vinet

    Copyright:  © 2002 Connectix Corporation
*/

#ifndef _FILE_H
#define _FILE_H

// ------------------------------------------------------------
// Prototypes
// ------------------------------------------------------------
void InitFiles( void );
void CloseFiles( void );

HANDLE VCEFSD_CreateFileW(	VolumeState* pVolume, 
							HANDLE hProc, 
							LPCWSTR lpFileName, 
							DWORD dwAccess, 
							DWORD dwShareMode,
	                       	LPSECURITY_ATTRIBUTES lpSecurityAttributes, 
	                       	DWORD dwCreate, 
	                       	DWORD dwFlagsAndAttributes, 
	                       	HANDLE hTemplateFile );

BOOL VCEFSD_CloseFile( FileData* pFileData );

BOOL VCEFSD_ReadFile( 	FileData* pFileData, 
						PBYTE pBuffer, 
						DWORD cbRead, 
						PDWORD pNumBytesRead, 
						LPOVERLAPPED lpOverlapped );

BOOL VCEFSD_WriteFile(	FileData* pFileData,
						LPCVOID pBuffer, 
						DWORD cbWrite, 
						PDWORD pNumBytesWritten, 
						LPOVERLAPPED lpOverlapped);

DWORD VCEFSD_SetFilePointer(	FileData* pFileData,
								LONG lDistanceToMove, 
								PLONG pDistanceToMoveHigh, 
								DWORD dwMoveMethod );

BOOL VCEFSD_ReadFileWithSeek(	FileData* pFileData,
								PBYTE pBuffer, 
								DWORD cbRead, 
								PDWORD pNumBytesRead, 
								LPOVERLAPPED lpOverlapped, 
								DWORD dwLowOffset, 
								DWORD dwHighOffset );

BOOL VCEFSD_WriteFileWithSeek(	FileData* pFileData,
								PBYTE pBuffer, 
								DWORD cbWrite, 
								PDWORD pNumBytesWritten, 
								LPOVERLAPPED lpOverlapped, 
								DWORD dwLowOffset, 
								DWORD dwHighOffset );

DWORD VCEFSD_GetFileSize(	FileData* pFileData, 
							PDWORD pFileSizeHigh );

BOOL VCEFSD_GetFileInformationByHandle(	FileData* pFileData,
										PBY_HANDLE_FILE_INFORMATION pFileInfo);

BOOL VCEFSD_FlushFileBuffers( FileData* pFileData );

BOOL VCEFSD_GetFileTime(	FileData* pFileData, 
							PFILETIME pCreation, 
							PFILETIME pLastAccess, 
							PFILETIME pLastWrite );

BOOL VCEFSD_SetFileTime(	FileData* pFileData, 
							PFILETIME pCreation, 
							PFILETIME pLastAccess, 
							PFILETIME pLastWrite );

BOOL VCEFSD_SetEndOfFile( FileData* pFileData );

#endif // _FILE_H
