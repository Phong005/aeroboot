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
    File:       vol.c

    Contains:   VirtualCE FSD volume level interface.

    Written by: Craig Vinet

    Copyright:  © 2002 Connectix Corporation
*/

#include    "vcefsd.h"
#include    "fserver.h"
#include    "find.h"
#include    "vol.h"

#define dim(x) (sizeof(x)/sizeof((x)[0]))

extern	ServerPB*				gvpServerPB;
extern	SHELLFILECHANGEFUNC_t	gpnShellNotify;

extern BOOL	LongToFileTime( LONG longTime, PFILETIME pft );

//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL VCEFSD_CreateDirectoryW(	VolumeState* pVolumeState,
								PCWSTR pwsPathName, 
								LPSECURITY_ATTRIBUTES lpSecurityAttributes )
{
	int				error;
	FILECHANGEINFO	changeInfo;
	WCHAR			fullPathname[MAX_PATH];

    DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: FSD_CreateDirectoryW " ) ) );
	EnterCriticalSectionMM(g_pcsMain);

	if( Find( pVolumeState, gvpServerPB, pwsPathName ) == 0 )
    {
		error = ERROR_ALREADY_EXISTS;
		SetLastError( error );
		goto Done;
	}

    error = ServerMkDir( gvpServerPB );
    
	if( error == 0 )
	{
		changeInfo.cbSize = sizeof( FILECHANGEINFO );
		changeInfo.wEventId = SHCNE_MKDIR;
		changeInfo.uFlags = SHCNF_PATH|SHCNF_FLUSHNOWAIT;
		
		// Get full  path name...
		FSDMGR_GetVolumeName( pVolumeState->vs_Handle, fullPathname, MAX_PATH );
		wcscat( fullPathname, pwsPathName );

		changeInfo.dwItem1 = (DWORD)fullPathname; 
		changeInfo.dwItem2 = (DWORD)NULL;

		// Rather than hop over the wall again to get the attributes, just set em' 
		// - we know we just created a folder...
		changeInfo.dwAttributes = FILE_ATTRIBUTE_DIRECTORY;
		LongToFileTime( gvpServerPB->fFileTimeDate, &changeInfo.ftModified );
		changeInfo.nFileSize = gvpServerPB->fSize;
		
		if( gpnShellNotify != NULL )
			gpnShellNotify( &changeInfo );
			
	}

Done:;
	LeaveCriticalSectionMM(g_pcsMain);    	
    return( error == 0 );
}

//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL VCEFSD_RemoveDirectoryW(	VolumeState* pVolumeState, 
								PCWSTR pwsPathName )
{
    int				error;
	FILECHANGEINFO	changeInfo;
	WCHAR			fullPathname[MAX_PATH];

	DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: FSD_RemoveDirectoryW " ) ) );
    EnterCriticalSectionMM(g_pcsMain);

    // Find the dir...
    error = Find( pVolumeState, gvpServerPB, pwsPathName );

	if( error == 0 )
    {
		changeInfo.cbSize = sizeof( FILECHANGEINFO );
		changeInfo.wEventId = SHCNE_RMDIR;
		changeInfo.uFlags = SHCNF_PATH|SHCNF_FLUSHNOWAIT;
		
		// Get full file path name...
		FSDMGR_GetVolumeName( pVolumeState->vs_Handle, fullPathname, MAX_PATH );
		wcscat( fullPathname, pwsPathName );

		changeInfo.dwItem1 = (DWORD)fullPathname; 
		changeInfo.dwItem2 = (DWORD)NULL;
		changeInfo.dwAttributes = gvpServerPB->fFileAttributes; 
		LongToFileTime( gvpServerPB->fFileTimeDate, &changeInfo.ftModified );
		changeInfo.nFileSize = gvpServerPB->fSize; 
	
    	// Remove the dir...
    	error = ServerRmDir( gvpServerPB );

		if( error == 0 )
		{
			if( gpnShellNotify != NULL )
				gpnShellNotify( &changeInfo );
		}
		else
		{
			SetLastError( ERROR_ACCESS_DENIED );
		}
    }

	LeaveCriticalSectionMM(g_pcsMain);    	
    return( error == 0 );
}


//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
DWORD VCEFSD_GetFileAttributesW(	VolumeState* pVolumeState,
									PCWSTR pwsFileName)
{
	int 		result = -1;
    int			error;
	
    DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: FSD_GetFileAttributesW " ) ) );
	EnterCriticalSectionMM(g_pcsMain);

    // Find the file...
    error = Find( pVolumeState, gvpServerPB, pwsFileName );

	if( error == 0 )
    {
    	// Get the attributes...
    	error = ServerGetInfo( gvpServerPB );
    	
    	if( error == 0 )
    	{
    		result = gvpServerPB->fFileAttributes;
    	}
    }

	LeaveCriticalSectionMM(g_pcsMain);    	
    return result;
}

//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL VCEFSD_SetFileAttributesW(	VolumeState* pVolumeState,
									PCWSTR pwsFileName,
									DWORD dwAttributes )
{
	int 			result = -1;
    int				error;
	FILECHANGEINFO	changeInfo;
	WCHAR			fullFilePathname[MAX_PATH];

    DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: FSD_SetFileAttributesW " ) ) );
	EnterCriticalSectionMM(g_pcsMain);

    // Find the file...
    error = Find( pVolumeState, gvpServerPB, pwsFileName );

	if( error == 0 )
    {
    	gvpServerPB->fFileTimeDate = 0;
		gvpServerPB->fFileAttributes =( USHORT )dwAttributes;
		
		// Set the attributes...
		error = ServerSetAttributes( gvpServerPB );

		if( error == 0 )
		{
			changeInfo.cbSize = sizeof( FILECHANGEINFO );
			changeInfo.wEventId = SHCNE_UPDATEITEM; // Funny, the docs say use SHCNE_ATTRIBUTES - sigh.
			changeInfo.uFlags = SHCNF_PATH|SHCNF_FLUSHNOWAIT;
			
			// Get full file path name...
			FSDMGR_GetVolumeName( pVolumeState->vs_Handle, fullFilePathname, MAX_PATH );
			wcscat( fullFilePathname, pwsFileName );

			changeInfo.dwItem1 = (DWORD)fullFilePathname; 
			changeInfo.dwItem2 = (DWORD)NULL;
			changeInfo.dwAttributes = gvpServerPB->fFileAttributes; 
			LongToFileTime( gvpServerPB->fFileTimeDate, &changeInfo.ftModified );
			changeInfo.nFileSize = gvpServerPB->fSize;
			
			if( gpnShellNotify != NULL )
				gpnShellNotify( &changeInfo ); 
		}
    }

	LeaveCriticalSectionMM(g_pcsMain);    	
    return( error == 0 );
}

//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL VCEFSD_DeleteFileW( 	VolumeState* pVolumeState,
							PCWSTR pwsFileName )
{
    int				error;
	FILECHANGEINFO	changeInfo;
	WCHAR			fullFilePathname[MAX_PATH];

    DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: FSD_DeleteFileW " ) ) );
	EnterCriticalSectionMM(g_pcsMain);

    // Find the file...
    error = Find( pVolumeState, gvpServerPB, pwsFileName );

	if( error == 0 )
    {
		changeInfo.cbSize = sizeof( FILECHANGEINFO );
		changeInfo.wEventId = SHCNE_DELETE;
		changeInfo.uFlags = SHCNF_PATH|SHCNF_FLUSHNOWAIT;
		
		// Get full file path name...
		FSDMGR_GetVolumeName( pVolumeState->vs_Handle, fullFilePathname, MAX_PATH );
		wcscat( fullFilePathname, pwsFileName );

		changeInfo.dwItem1 = (DWORD)fullFilePathname; 
		changeInfo.dwItem2 = (DWORD)NULL;
		changeInfo.dwAttributes = gvpServerPB->fFileAttributes; 
		LongToFileTime( gvpServerPB->fFileTimeDate, &changeInfo.ftModified );
		changeInfo.nFileSize = gvpServerPB->fSize; 
	
    	// Delete the file...
    	error = ServerDelete( gvpServerPB );
		
		if( error == 0 )
		{
			if( gpnShellNotify != NULL )
				gpnShellNotify( &changeInfo );
		}
		else
		{
			SetLastError( ERROR_ACCESS_DENIED );
		}
    }

	LeaveCriticalSectionMM(g_pcsMain);    	
    return( error == 0 );
}

//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL VCEFSD_GetDiskFreeSpaceW(	VolumeState* pVolumeState,
								PCWSTR pwsPathName,
								PDWORD pSectorsPerCluster,
								PDWORD pBytesPerSector,
								PDWORD pFreeClusters,
								PDWORD pClusters )
{
    int			error;
	
    DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: FSD_GetDiskFreeSpaceW " ) ) );
	EnterCriticalSectionMM(g_pcsMain);

	// Ask the server for the info
	error = ServerGetSpace( gvpServerPB );

    if( error == 0 )
    {
        *pBytesPerSector = 512;				// sector size
        *pClusters = gvpServerPB->fPosition;	// # of allocation units
        *pSectorsPerCluster = 64;			// # of sectors per allocation unit( 64*512 = 32k )
        *pFreeClusters = gvpServerPB->fSize;	// # of free allocation units
    }
	
	LeaveCriticalSectionMM(g_pcsMain);
	
    return( error == 0 );
}

//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL VCEFSD_MoveFileW(	VolumeState* pVolumeState, 
						PCWSTR pwsOldFileName, 
						PCWSTR pwsNewFileName )
{
    int error;
	WCHAR sourceFile[MAX_PATH];
	WCHAR destFile[MAX_PATH];
	FILECHANGEINFO	changeInfo;
	HRESULT hr;

	DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: FSD_MoveFileW\n" ) ) );
	EnterCriticalSectionMM(g_pcsMain);

	hr = StringCchCopyW(sourceFile, dim(sourceFile), pwsOldFileName);
	if (!SUCCEEDED(hr)) {
		error = HRESULT_CODE(hr);
		goto EXIT;
	}
	hr = StringCchCopyW(destFile, dim(destFile), pwsNewFileName);
	if (!SUCCEEDED(hr)) {
		error = HRESULT_CODE(hr);
		goto EXIT;
	}

	error = Find( pVolumeState, gvpServerPB, sourceFile );
	if (!error)
	{
		// Source file exists and fName contains its name - do the move

		hr = StringCchCopyW(gvpServerPB->u.fLfn.fName2, dim(gvpServerPB->u.fLfn.fName2), destFile);
		if (!SUCCEEDED(hr)) {
			error = HRESULT_CODE(hr);
			goto EXIT;
		}

		gvpServerPB->u.fLfn.fName2Length = lstrlenW( destFile )* sizeof(WCHAR);

		// Prepare to notify the shell of the change
		changeInfo.cbSize = sizeof( FILECHANGEINFO );

		if( gvpServerPB->fFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
		{
			changeInfo.wEventId = SHCNE_RENAMEFOLDER;
		}
		else
		{
			changeInfo.wEventId = SHCNE_RENAMEITEM;
		}
		changeInfo.uFlags = SHCNF_PATH|SHCNF_FLUSHNOWAIT;
			
		// Get source full file path name...
		FSDMGR_GetVolumeName( pVolumeState->vs_Handle, sourceFile, MAX_PATH );
		wcscat( sourceFile, pwsOldFileName );

		// Get dest full file path name...
		FSDMGR_GetVolumeName( pVolumeState->vs_Handle, destFile, MAX_PATH );
		wcscat( destFile, pwsNewFileName );

		changeInfo.dwItem1 = (DWORD)sourceFile; 
		changeInfo.dwItem2 = (DWORD)destFile;

		// Returned from Find()...
		changeInfo.dwAttributes = gvpServerPB->fFileAttributes;
		LongToFileTime( gvpServerPB->fFileTimeDate, &changeInfo.ftModified );
		changeInfo.nFileSize = gvpServerPB->fSize; 

		// Rename/Move the file...		
		error = ServerRename( gvpServerPB );

		if( error == 0 )
		{
			if( gpnShellNotify != NULL )
			{
				gpnShellNotify( &changeInfo );
			}
		}
		else
		{
			SetLastError( ERROR_ACCESS_DENIED );
		}
	}

EXIT:
	LeaveCriticalSectionMM(g_pcsMain);

    return( error == 0 );
}

//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL VCEFSD_DeleteAndRenameFileW(	VolumeState* pVolumeState, 
									PCWSTR pwsOldFileName, 
									PCWSTR pwsNewFileName )
{
	// All I can say is, "Yuck, this behavior is very poorly documented and designed! 
	// They call this, 'Presto Chango', sigh"...
	if( VCEFSD_DeleteFileW( pVolumeState, pwsOldFileName ) )
	{
		return VCEFSD_MoveFileW( pVolumeState, pwsNewFileName, pwsOldFileName );
	}
	return FALSE;
}


//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
void VCEFSD_Notify( PVOLUME pvol, DWORD dwFlags )
{
    return;
}
