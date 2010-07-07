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
    File:       file.c

    Contains:   VirtualCE FSD file interface.

    Written by: Craig Vinet

    Copyright:  © 2002 Connectix Corporation
*/

#include    "vcefsd.h"
#include    "fserver.h"
#include    "find.h"
#include    "file.h"
#include    "vol.h"

//--------------------------------------------------------------------------------
// Defines
//--------------------------------------------------------------------------------
#define kMaxFD              40      // Max # of open files
#define kFDSignature		0x64664644


#define	kOpenAccessReadOnly			0x0000
#define	kOpenAccessWriteOnly		0x0001
#define	kOpenAccessReadWrite		0x0002
#define	kOpenShareCompatibility		0x0000
#define	kOpenShareDenyReadWrite		0x0010
#define	kOpenShareDenyWrite			0x0020
#define	kOpenShareDenyRead			0x0030
#define	kOpenShareDenyNone			0x0040

//--------------------------------------------------------------------------------
// Data
//--------------------------------------------------------------------------------
FileData  gFDList[kMaxFD];

extern PUCHAR		gvpReadWriteBuffer;
extern ServerPB*	gvpServerPB;

extern PUCHAR		gpReadWriteBuffer;	// Physical addr.

extern	SHELLFILECHANGEFUNC_t	gpnShellNotify;

//--------------------------------------------------------------------------------
// Prototypes
//--------------------------------------------------------------------------------
BOOL		ValidateFileHandle( FileData* pFileData );
FileData*	AllocFileData( void );
void		FreeFileData( FileData* pFileData );
BOOL		LongToFileTime( LONG longTime, PFILETIME pft );
LONG		FileTimeToLong( PFILETIME pft );

//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
void InitFiles( void )
{
	ULONG      x;
    
	DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: +InitFiles\n" ) ) );

    for( x=0; x<kMaxFD; x++ )
    {
        gFDList[x].fInUse = FALSE;
		gFDList[x].fSignature = 0;
		gFDList[x].fHandle = 0;
    }

	DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: -InitFiles\n" ) ) );
}

//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL ValidateFileHandle( FileData* pFileData )
{
    if( pFileData == 0 )
    {
 		DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: ValidateFileHandle - bad handle\n" ) ) );
        return FALSE;
    }
    
	if( pFileData->fSignature != kFDSignature )
    {
        DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: ValidateFileHandle - bad handle\n" ) ) );
        return FALSE;
    }
    return TRUE;
}


//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
FileData * AllocFileData( void )
{
    ULONG      x;
    FileData    * pFileData = 0;

	DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: +AllocFileData\n" ) ) );

    for( x=0; x<kMaxFD; x++ )
    {
        if( gFDList[x].fInUse == FALSE )
        {
            pFileData = &gFDList[x];
            break;
        }
    }

    if( pFileData )
    {
        pFileData->fSignature = kFDSignature;
        pFileData->fInUse = TRUE;
    }

    DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: -AllocFileData\n" ) ) );
    return pFileData;
}


//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
void FreeFileData( FileData* pFileData )
{
    DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: +FreeFileData\n" ) ) );

    if( ValidateFileHandle( pFileData ) != TRUE )
        goto Done;

    pFileData->fInUse = FALSE;
    pFileData->fSignature = 0;
    pFileData->fHandle = 0;

Done:;
    DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: -FreeFileData\n" ) ) );
}


//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
void CloseFiles( void )
{
    ULONG		x;
    ULONG		error;

	DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: +CloseFiles\n" ) ) );

    for( x=0; x<kMaxFD; x++ )
    {
        if( gFDList[x].fInUse )
        {
            // Fill in the parameter block for the ServerSetAttributes call
            memset( gvpServerPB, 0, sizeof(ServerPB) );
            gvpServerPB->fStructureSize = sizeof(ServerPB);
            gvpServerPB->fHandle = gFDList[x].fHandle;
            error = ServerGetFCBInfo( gvpServerPB );
            ASSERT( error == 0 );

            // Close the file
            gvpServerPB->fHandle = gFDList[x].fHandle;
            error = ServerClose( gvpServerPB );
            ASSERT( error == 0 );

            // If we need to update the attributes, do so

            if( gFDList[x].fWritten || gFDList[x].fSetAttrOnClose )
            {
                DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: CloseFiles setting attributes\n" ) ) );

                if( gFDList[x].fWritten )
                    gvpServerPB->fFileTimeDate = gFDList[x].fFileTimeDate;
                else
                    gvpServerPB->fFileTimeDate = 0;
                gvpServerPB->fFileAttributes =( BYTE ) gFDList[x].fFileAttributes;
                ServerSetAttributes( gvpServerPB );
            }

            // Destroy the file data

            FreeFileData( &gFDList[x] );
        }
    }

	DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: -CloseFiles\n" ) ) );
}


//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
HANDLE VCEFSD_CreateFileW(	VolumeState* pVolumeState, 
							HANDLE hProc, 
							LPCWSTR lpFileName, 
							DWORD dwAccess, 
							DWORD dwShareMode,
	                       	LPSECURITY_ATTRIBUTES lpSecurityAttributes, 
	                       	DWORD dwCreate, 
	                       	DWORD dwFlagsAndAttributes, 
	                       	HANDLE hTemplateFile )
{
    FileData*		pFileData;
    Boolean			fileExists = FALSE;
    Boolean			truncate = FALSE;
    Boolean			create = FALSE;
	Boolean			createAlways = FALSE;
	DWORD			error = NO_ERROR;
	HANDLE 			hFile = INVALID_HANDLE_VALUE;
	FILECHANGEINFO	changeInfo;
	WCHAR			fullFilePathname[MAX_PATH];
	SHORT			delIndex = 0x7FFF;

    DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: +FSD_CreateFileW\n" ) ) );
	EnterCriticalSectionMM(g_pcsMain);

    // See if the file exists
	if( Find( pVolumeState, gvpServerPB, lpFileName ) == 0 )
    {
        fileExists = TRUE;
    }

    // Depending on the action code, do the right thing...
    switch( dwCreate )
    {
        case OPEN_EXISTING:
            // Open the existing file
            if( fileExists == FALSE )	
            	error = ERROR_FILE_NOT_FOUND;
            break;

        case TRUNCATE_EXISTING:
        	if( fileExists == FALSE )
			{
            	error = ERROR_FILE_NOT_FOUND;
			}
			else
			{
				truncate = TRUE;
				create = TRUE;	
			}      
            break;
				
        case CREATE_ALWAYS:
            // Truncate existing file or create if none
            truncate = TRUE;
            createAlways = TRUE;
            break;

        case OPEN_ALWAYS:
            // Create a new empty file or just open
            if( fileExists == FALSE )
            {
	            create = TRUE;
			}
            break;
		
		case CREATE_NEW:
			if( fileExists == TRUE )
			{	
            	error = ERROR_ALREADY_EXISTS;
			}
			else
			{
				create = TRUE;
			}
			break;

        default:
            // In the weeds
            error = ERROR_INVALID_PARAMETER;
            break;
    }

    // Truncate( Delete ) the file if need be
    if( error == 0  && truncate && fileExists == TRUE )
    {
		delIndex = gvpServerPB->fIndex;

        if( ServerDelete( gvpServerPB ) != 0 )
		{
            error = ERROR_ACCESS_DENIED;
		}
    }
	
    // Create a new file if need be
    if( ( create || createAlways ) && error == NO_ERROR )
    {
        if( ServerCreate( gvpServerPB ) == 0 )
        {
            // Set the file's attributes
            gvpServerPB->fFileAttributes = ( USHORT )( dwFlagsAndAttributes & 0xFFFF ); 
            
            if( ServerSetAttributes( gvpServerPB ) != 0 )
			{
				error = ERROR_ACCESS_DENIED;
			}
		}
		else
		{
			error = ERROR_ACCESS_DENIED;
		}
    }

	if( error == 0 && truncate && fileExists == TRUE && !createAlways )
    {
		// Fix me: reset file date/time to original date/time.
	}

    // If everything is ok, then open the file
    if( error == NO_ERROR )
    {
		// If we created a file, send a notification to OS...
		if( create )
		{
			changeInfo.cbSize = sizeof( FILECHANGEINFO );
	
			if( truncate && !createAlways )
			{
				changeInfo.wEventId = SHCNE_UPDATEITEM;
			}
			else
			{
				changeInfo.wEventId = SHCNE_CREATE;
			}

			changeInfo.uFlags = SHCNF_PATH|SHCNF_FLUSHNOWAIT;
			
			// Get full file path name...
			FSDMGR_GetVolumeName( pVolumeState->vs_Handle, fullFilePathname, MAX_PATH );
			wcscat( fullFilePathname, lpFileName );

			changeInfo.dwItem1 = (DWORD)fullFilePathname; 
			changeInfo.dwItem2 = (DWORD)NULL;
			changeInfo.dwAttributes = gvpServerPB->fFileAttributes; 
			LongToFileTime( gvpServerPB->fFileTimeDate, &changeInfo.ftModified );
			changeInfo.nFileSize = gvpServerPB->fSize;
			
			if( gpnShellNotify != NULL )
				gpnShellNotify( &changeInfo ); 
		}

		gvpServerPB->fOpenMode = kOpenShareCompatibility;

		switch ( dwAccess )  
		{
			case GENERIC_READ | GENERIC_WRITE:
				gvpServerPB->fOpenMode |= kOpenAccessReadWrite;
				break;

			case GENERIC_READ:
				gvpServerPB->fOpenMode |= kOpenAccessReadOnly;
				break;

			case GENERIC_WRITE:
				gvpServerPB->fOpenMode |= kOpenAccessWriteOnly;
				break;
	
			default:
				break;
		}

		switch( dwShareMode )
		{
			case FILE_SHARE_READ | FILE_SHARE_WRITE:
				gvpServerPB->fOpenMode |= kOpenShareDenyNone;
				break;

			case FILE_SHARE_READ:
				gvpServerPB->fOpenMode |= kOpenShareDenyWrite;
				break;

			case FILE_SHARE_WRITE:
				gvpServerPB->fOpenMode |= kOpenShareDenyRead;
				break;
      
			default :
				gvpServerPB->fOpenMode |= kOpenShareDenyReadWrite;
				break;
		}
        
		if( ServerOpen( gvpServerPB ) == 0 )
        {
            // Allocate and initialize a FileData struct
            pFileData = AllocFileData();
            if( pFileData )
            {
                pFileData->fResHandle =( void* )pVolumeState;
                pFileData->fHandle = gvpServerPB->fHandle;
                pFileData->fPosition = 0;
                pFileData->fSize = gvpServerPB->fSize;
                pFileData->fWritten = FALSE;
                
				lstrcpyW( pFileData->fFileName, lpFileName );

                if( create &&( dwFlagsAndAttributes & FILE_ATTRIBUTE_READONLY ) )
                    pFileData->fSetAttrOnClose = TRUE;
                else
                    pFileData->fSetAttrOnClose = FALSE;
                    
                pFileData->fFileAttributes = ( gvpServerPB->fFileAttributes & 0xFFFF );
		        pFileData->fFileTimeDate = gvpServerPB->fFileTimeDate;
		        pFileData->fFileCreateTimeDate = gvpServerPB->fFileCreateTimeDate;
				pFileData->pVolumeState = pVolumeState;
            }
            else
            {
                ServerClose( gvpServerPB );
            }
        }
		else
		{
			error = ERROR_ACCESS_DENIED;
		}
    }

    // If we failed, zero all the return items
    if( error == NO_ERROR )
    {
        hFile = FSDMGR_CreateFileHandle( pVolumeState->vs_Handle, hProc,( ulong )pFileData );
		SetLastError( NO_ERROR );
    }
    else
	{
		SetLastError( error );
	}
    LeaveCriticalSectionMM(g_pcsMain);
    DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: -FSD_CreateFileW\n" ) ) );
    return hFile;
}


//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL VCEFSD_CloseFile( FileData* pFileData )
{
	VolumeState*	pVolumeState;
    int				error = -1;
	FILECHANGEINFO	changeInfo;
	WCHAR			fullFilePathname[MAX_PATH];

	DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: +FSD_CloseFile\n" ) ) );
	EnterCriticalSectionMM(g_pcsMain);
	
    // Verify the file handle
    if( ValidateFileHandle( pFileData ) == FALSE )
        goto Done;

	pVolumeState =( VolumeState* )pFileData->fResHandle;

	// Fill in the parameter block for the ServerSetAttributes call
   	memset( gvpServerPB, 0, sizeof(ServerPB));
        gvpServerPB->fStructureSize = sizeof(ServerPB);

	gvpServerPB->fHandle = pFileData->fHandle;
	
	error = ServerGetFCBInfo( gvpServerPB );
	if( error == 0 )
	{
		// Close the file
		gvpServerPB->fHandle = pFileData->fHandle;
		error = ServerClose( gvpServerPB );

		// If we need to update the attributes, do so
		if( error == 0 && ( pFileData->fWritten || pFileData->fSetAttrOnClose ) )
		{
			gvpServerPB->fFileTimeDate = pFileData->fFileTimeDate;
			gvpServerPB->fFileCreateTimeDate = pFileData->fFileCreateTimeDate;
			gvpServerPB->fFileAttributes =( BYTE ) pFileData->fFileAttributes;
			ServerSetAttributes( gvpServerPB );

			// Set up notify...
			changeInfo.cbSize = sizeof( FILECHANGEINFO );
			changeInfo.wEventId = SHCNE_UPDATEITEM;
			changeInfo.uFlags = SHCNF_PATH|SHCNF_FLUSHNOWAIT;
			
			// Get full file path name...
			FSDMGR_GetVolumeName( pFileData->pVolumeState->vs_Handle, fullFilePathname, MAX_PATH );
			wcscat( fullFilePathname, pFileData->fFileName );

			changeInfo.dwItem1 = (DWORD)fullFilePathname; 
			changeInfo.dwItem2 = (DWORD)NULL;
			changeInfo.dwAttributes = gvpServerPB->fFileAttributes; 
			LongToFileTime( gvpServerPB->fFileTimeDate, &changeInfo.ftModified );
			changeInfo.nFileSize = gvpServerPB->fSize; 
			
			if( gpnShellNotify != NULL )
				gpnShellNotify( &changeInfo ); 
		}

		// Destroy the file data
		FreeFileData( pFileData );
	}

Done:;
	LeaveCriticalSectionMM(g_pcsMain);
    DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: -FS_Close" ) ) );
    return( error == 0 );
}


//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL VCEFSD_ReadFile( 	FileData* pFileData, 
						PBYTE pBuffer, 
						DWORD cbRead, 
						PDWORD pNumBytesRead, 
						LPOVERLAPPED lpOverlapped )
{
	VolumeState*	pVolumeState;
    ULONG			xferCount;
    ULONG			totalCount = 0;
    ULONG			countLeft = 0;
	int				error = -1;
	PBYTE			fDTAPtr;
	
	DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: +FS_ReadFile" ) ) );
	EnterCriticalSectionMM(g_pcsMain);
	
	*pNumBytesRead = 0;
	
    // Verify the file handle
    if( ValidateFileHandle( pFileData ) == FALSE )
        goto Done;

	pVolumeState =( VolumeState* )pFileData->fResHandle;

	// Fill in the parameter block...
   	memset( gvpServerPB, 0, sizeof(ServerPB));
        gvpServerPB->fStructureSize = sizeof(ServerPB);
	gvpServerPB->fHandle = pFileData->fHandle;

    // Make sure we do not read beyond the file
    if( pFileData->fPosition >= pFileData->fSize )
    {
    	countLeft = 0;
		error = 0; // return non-error on read past EOF
    }
    else if( ( pFileData->fPosition + cbRead ) > pFileData->fSize )
    {
        countLeft = pFileData->fSize - pFileData->fPosition;
    }
    else
    {
        countLeft = cbRead;
	}
	
	if( countLeft )
    {
        fDTAPtr = pBuffer;

		//WARNING!!! WARNING!!! WARNING!!! WARNING!!! WARNING!!! WARNING!!! WARNING!!! WARNING!!!
		gvpServerPB->fDTAPtr = gpReadWriteBuffer; // store physical addr. of our global data buffer
		gvpServerPB->fPosition = pFileData->fPosition;

        // Read the data
        while( countLeft )
        {
            if( countLeft > kMaxReadWriteSize )
                xferCount = kMaxReadWriteSize;
            else
                xferCount = countLeft;

            gvpServerPB->fSize = xferCount;

			error = ServerRead( gvpServerPB );

            if( error != 0 )
                break;

            gvpServerPB->fPosition += gvpServerPB->fSize;
			
			memcpy( fDTAPtr, gvpReadWriteBuffer, gvpServerPB->fSize );
            fDTAPtr += gvpServerPB->fSize;
            
            totalCount += gvpServerPB->fSize;
            countLeft -= gvpServerPB->fSize;

            if( gvpServerPB->fSize < xferCount )
                break;
        }
    }

Done:;
	LeaveCriticalSectionMM(g_pcsMain);
    DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: -FS_ReadFile" ) ) );
    
	pFileData->fPosition += totalCount;

    // Return bytes read...
    *pNumBytesRead = totalCount;
    return( error == 0 );
}


//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL VCEFSD_WriteFile(	FileData* pFileData,
						LPCVOID pBuffer, 
						DWORD cbWrite, 
						PDWORD pNumBytesWritten, 
						LPOVERLAPPED lpOverlapped )
{
	VolumeState*	pVolumeState;
    ULONG			xferCount;
    ULONG			totalCount = 0;
    ULONG			countLeft = 0;
	int				error = -1;
	PBYTE			fDTAPtr;

	DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: +FSD_WriteFile" ) ) );
	EnterCriticalSectionMM(g_pcsMain);
	
	*pNumBytesWritten = 0;
	
    // Verify the file handle
    if( ValidateFileHandle( pFileData ) == FALSE )
        goto Done;

	pVolumeState =( VolumeState* )pFileData->fResHandle;

	// Fill in the parameter block...
	memset( gvpServerPB, 0, sizeof(ServerPB));
        gvpServerPB->fStructureSize = sizeof(ServerPB);
	gvpServerPB->fHandle = pFileData->fHandle;

	countLeft = cbWrite;
	gvpServerPB->fPosition = pFileData->fPosition;

    // Write or set EOF...
    if( countLeft == 0 )
    {
        error = ServerSetEOF( gvpServerPB );
        if( error == 0 )
        {
            pFileData->fSize = pFileData->fPosition;
        }
        else
        {
        	goto Done;
        }
    }
    else if( countLeft )
    {
		//WARNING!!! WARNING!!! WARNING!!! WARNING!!! WARNING!!! WARNING!!! WARNING!!! WARNING!!!
        gvpServerPB->fDTAPtr = gpReadWriteBuffer; // store physical addr.
		fDTAPtr = ( LPVOID )( pBuffer );

        // Read the data...
        while( countLeft )
        {
            if( countLeft > kMaxReadWriteSize )
                xferCount = kMaxReadWriteSize;
            else
                xferCount = countLeft;

            gvpServerPB->fSize = xferCount;
            

			memcpy( gvpReadWriteBuffer, fDTAPtr, gvpServerPB->fSize );
            error = ServerWrite( gvpServerPB );

            if( error != 0 )
                break;

			pFileData->fWritten = TRUE;

            gvpServerPB->fPosition += gvpServerPB->fSize;
            fDTAPtr += gvpServerPB->fSize;
            
            totalCount += gvpServerPB->fSize;
            countLeft -= gvpServerPB->fSize;

            if( gvpServerPB->fSize < xferCount )
                break;
        }
    }

	// Set new size if required...
	if( gvpServerPB->fPosition > pFileData->fSize )
        pFileData->fSize = gvpServerPB->fPosition;

Done:;
	LeaveCriticalSectionMM(g_pcsMain);
    DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: -FSD_WriteFile" ) ) );
    
	pFileData->fPosition += totalCount;

	// Return bytes written...
    *pNumBytesWritten = totalCount;
    return( error == 0 );
}


//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
DWORD VCEFSD_SetFilePointer(	FileData* pFileData,
								LONG lDistanceToMove, 
								PLONG pDistanceToMoveHigh, 
								DWORD dwMoveMethod )
{
	LONG lPos = -1;
	
    DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: +FSD_SetFilePointer" ) ) );
	EnterCriticalSectionMM(g_pcsMain);
	
	// Why? Don't ask me...
	if( pDistanceToMoveHigh )
        *pDistanceToMoveHigh = 0;
	
    // Verify the file handle
    if( ValidateFileHandle( pFileData ) == FALSE )
        goto Done;

    // Compute the new position based on move method...
	switch( dwMoveMethod ) 
	{
        case FILE_BEGIN:
        {
            lPos = pFileData->fPosition = lDistanceToMove;
            break;
        }

        case FILE_CURRENT:
        {
            lPos = pFileData->fPosition += lDistanceToMove;
            break;
        }

        case FILE_END :
        {
            lPos = pFileData->fPosition = pFileData->fSize - lDistanceToMove;
            break;
        }

        default:
        {	
        	SetLastError(ERROR_INVALID_PARAMETER);
        	break;
        }
    }
    
    SetLastError(NO_ERROR);
Done:;
    LeaveCriticalSectionMM(g_pcsMain);
	DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: -FSD_SetFilePointer\n" ) ) );
	
    return( lPos );
}


//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL VCEFSD_ReadFileWithSeek(	FileData* pFileData,
								PBYTE pBuffer, 
								DWORD cbRead, 
								PDWORD pNumBytesRead, 
								LPOVERLAPPED lpOverlapped, 
								DWORD dwLowOffset, 
								DWORD dwHighOffset )
{
    DWORD lPos;
    DEBUGCHK(pFileData != NULL);
    
    // If pBuffer and cbRead are both zero we are supposed to return TRUE or FALSE
    // to indicate whether we do or do not support paging.
    if (pBuffer == NULL && cbRead == 0) {
        return TRUE; // indicate that we support demand paging
    }
    
    lPos = VCEFSD_SetFilePointer( pFileData, dwLowOffset, NULL, FILE_BEGIN );
    
    *pNumBytesRead = 0;
    
    if( lPos == -1 )
        return FALSE;
    
    return VCEFSD_ReadFile( pFileData, pBuffer, cbRead, pNumBytesRead, NULL ); 
}


//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL VCEFSD_WriteFileWithSeek(	FileData* pFileData,
								PBYTE pBuffer, 
								DWORD cbWrite, 
								PDWORD pNumBytesWritten, 
								LPOVERLAPPED lpOverlapped, 
								DWORD dwLowOffset, 
								DWORD dwHighOffset )
{
DWORD lPos;
	DEBUGMSG(ZONE_APIS, (TEXT("VCEFSD: FSD_WriteFileWithSeek\n")));
	
	*pNumBytesWritten = 0;
	lPos = VCEFSD_SetFilePointer( pFileData, dwLowOffset, NULL, FILE_BEGIN );
	
	if( lPos == -1 )
		return FALSE;
		
	return VCEFSD_WriteFile( pFileData, pBuffer, cbWrite, pNumBytesWritten, NULL );
}


//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
DWORD VCEFSD_GetFileSize(	FileData* pFileData, 
							PDWORD pFileSizeHigh)
{
    LONG lSize;

    DEBUGMSG(ZONE_APIS, (TEXT("VCEFSD: FSD_GetFileSize\n")));

    EnterCriticalSectionMM(g_pcsMain);

	lSize = pFileData->fSize;

    LeaveCriticalSectionMM(g_pcsMain);

    if (pFileSizeHigh) // Gack!
        *pFileSizeHigh = 0;

    return lSize;
}


//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL VCEFSD_GetFileInformationByHandle(	FileData* pFileData,
										PBY_HANDLE_FILE_INFORMATION pFileInfo)
{
    DEBUGMSG(ZONE_APIS, (TEXT("VCEFSD: FSD_GetFileInformationByHandle\n")));

    memset(pFileInfo, 0, sizeof(*pFileInfo));
    pFileInfo->dwFileAttributes = pFileData->fFileAttributes;
    pFileInfo->dwVolumeSerialNumber = (DWORD)pFileData->pVolumeState;
    pFileInfo->nFileSizeLow = pFileData->fSize;
    pFileInfo->nNumberOfLinks = 1;
    pFileInfo->dwOID = (DWORD)INVALID_HANDLE_VALUE;

    return TRUE;
}


//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL VCEFSD_FlushFileBuffers( FileData* pFileData )
{
	SetLastError( ERROR_NOT_SUPPORTED );
    return FALSE;
}


//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL VCEFSD_GetFileTime(	FileData* pFileData, 
							PFILETIME pCreation, 
							PFILETIME pLastAccess, 
							PFILETIME pLastWrite ) 
{
    BOOL fSuccess = FALSE;

    DEBUGMSG(ZONE_APIS, (TEXT("VCEFSD: FSD_GetFileTime\n")));

    EnterCriticalSectionMM(g_pcsMain);

	if( pCreation != NULL )
	{
		LongToFileTime( pFileData->fFileCreateTimeDate, pCreation );
		fSuccess = TRUE;
	}
	
	if(	pLastAccess != NULL )
	{
		LongToFileTime( pFileData->fFileTimeDate, pLastAccess );
		fSuccess = TRUE;
	}
	
	if( pLastWrite != NULL )
	{	
		LongToFileTime( pFileData->fFileTimeDate, pLastWrite );
		fSuccess = TRUE;
	}

    LeaveCriticalSectionMM(g_pcsMain);

    return fSuccess;
}


//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL VCEFSD_SetFileTime(	FileData* pFileData, 
							PFILETIME pCreation, 
							PFILETIME pLastAccess, 
							PFILETIME pLastWrite ) 
{
    BOOL fSuccess = FALSE;

    DEBUGMSG(ZONE_APIS, (TEXT("VCEFSD: FSD_SetFileTime\n")));

    EnterCriticalSectionMM(g_pcsMain);

	if( pCreation != NULL )
	{
		pFileData->fFileCreateTimeDate = FileTimeToLong( pCreation );
		fSuccess = TRUE;
	}

	if( pLastWrite != NULL )
	{
		pFileData->fFileTimeDate = FileTimeToLong( pLastWrite );
		fSuccess = TRUE;
	}

    LeaveCriticalSectionMM(g_pcsMain);

    return fSuccess;
}


//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL VCEFSD_SetEndOfFile( FileData* pFileData )
{
    VolumeState*	pVolumeState;
	BOOL			fSuccess = FALSE;

    DEBUGMSG(ZONE_APIS, (TEXT("VCEFSD: FSD_SetEndOfFile\n")));

    EnterCriticalSectionMM(g_pcsMain);

	if( pFileData->fPosition > pFileData->fSize )
	{
		pVolumeState =( VolumeState* )pFileData->fResHandle;

		// Init PB
		memset( gvpServerPB, 0, sizeof(ServerPB) );
                gvpServerPB->fStructureSize = sizeof(ServerPB);
		gvpServerPB->fHandle = pFileData->fHandle;
		gvpServerPB->fPosition = pFileData->fPosition;
		
		if( ServerSetEOF( gvpServerPB ) == 0 )	
			fSuccess = TRUE;
	}
	else
	{
        // Can't make a file smaller...
        SetLastError(ERROR_NOT_SUPPORTED);
	}
	
    LeaveCriticalSectionMM(g_pcsMain);

    return fSuccess;
}



//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL LongToFileTime( LONG longTime, PFILETIME pft )
{
    FILETIME	ft;
    SYSTEMTIME	st;
	WORD		dosDate;
	WORD		dosTime;

	dosTime = LOWORD(longTime);
	dosDate = HIWORD(longTime);

    st.wYear = (dosDate >> 9) + 1980;
    st.wMonth = (dosDate >> 5) & 0xf;
    st.wDayOfWeek = 0;
    st.wDay = dosDate & 0x1f;
    st.wHour = dosTime >> 11;
    st.wMinute = (dosTime >> 5) & 0x3f;
    st.wSecond = (dosTime & 0x1f) << 1;
    st.wMilliseconds = 0;

    if (SystemTimeToFileTime(&st, &ft)) 
	{
        if (LocalFileTimeToFileTime(&ft, pft)) 
		{
		    return TRUE;
        }
    }
    return FALSE;
}



//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
LONG FileTimeToLong( PFILETIME pft )
{
    FILETIME ft;
    SYSTEMTIME st;

	WORD	dosDate;
	WORD	dosTime;
	LONG	outTime = 0;

    if( FileTimeToLocalFileTime(pft, &ft) )
	{
		if( FileTimeToSystemTime(&ft, &st) )
		{
			dosDate = ((st.wYear-1980) << 9) | (st.wMonth << 5) | (st.wDay);
			dosTime = (st.wHour << 11) | (st.wMinute << 5) | (st.wSecond >> 1);

			outTime = MAKELONG(dosTime,dosDate);
		}
	}
    return outTime;
}
