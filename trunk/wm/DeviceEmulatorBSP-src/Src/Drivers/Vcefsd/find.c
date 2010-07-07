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
    File:       find.c

    Contains:   Contains VirtualCE file system find interface.

    Written by: Craig Vinet

    Copyright:  © 2002 Connectix Corporation
*/

#include    "vcefsd.h"
#include    "fserver.h"
#include    "find.h"


#define	kMaxFC		40

//--------------------------------------------------------------------------------
// 
//--------------------------------------------------------------------------------
extern ServerPB*	gvpServerPB;
extern BOOL	LongToFileTime( LONG longTime, PFILETIME pft );

//--------------------------------------------------------------------------------
// 
//--------------------------------------------------------------------------------
FindContext gFCList[kMaxFC];

//--------------------------------------------------------------------------------
// Prototypes
//--------------------------------------------------------------------------------
int		FindOpen(	VolumeState *pVolume, 
					PCWSTR pwsFilePath, 
					PWIN32_FIND_DATAW pfd,
					FindContext ** outFindContext );

int		FindNext( FindContext  * pFindContext );

USHORT	FindPathElement( PServerPB pb, PCWSTR ptrName);

BOOL	MatchesWildcard(DWORD lenWild, PCWSTR pchWild, DWORD lenFile, PCWSTR pchFile);


FindContext * AllocateFindContext( void );
BOOL FreeFindContext( FindContext * inFCPtr );

//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
HANDLE VCEFSD_FindFirstFileW( 	VolumeState *pVolume, 
								HANDLE hProc, 
								PCWSTR pwsFilePath, 
								PWIN32_FIND_DATAW pfd )
{
    HANDLE		hSearch = INVALID_HANDLE_VALUE;
    int			error = 0;
    FindContext	*pFindContext;
	
	DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: FSD_FindFirstFileW\n" ) ) );

	EnterCriticalSectionMM(g_pcsMain);

    error = FindOpen(	pVolume, 
						pwsFilePath, 
						pfd,
						&pFindContext );
    
    if( error == 0 && pFindContext != NULL )
    {
    	// Fill in the find struct
    	if( pfd ) 
    	{
	        pfd->dwFileAttributes = pFindContext->fServerPB.fFileAttributes;
			LongToFileTime( pFindContext->fServerPB.fFileCreateTimeDate, &pfd->ftCreationTime );
			LongToFileTime( pFindContext->fServerPB.fFileTimeDate, &pfd->ftLastAccessTime );
			LongToFileTime( pFindContext->fServerPB.fFileTimeDate, &pfd->ftLastWriteTime );

			pfd->nFileSizeHigh = 0;
			pfd->nFileSizeLow = pFindContext->fServerPB.fSize;

			// Copy Unicode name.
			lstrcpyW( pfd->cFileName, pFindContext->fServerPB.u.fLfn.fName );
       }
        
        // Create FS search handle
        hSearch = FSDMGR_CreateSearchHandle( pVolume->vs_Handle, hProc, (ulong)pFindContext );
    }
	else
    {
        SetLastError( ERROR_FILE_NOT_FOUND );
    }

	LeaveCriticalSectionMM(g_pcsMain);

    return hSearch;
}

//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL VCEFSD_FindNextFileW(	FindContext* pFindContext, 
							PWIN32_FIND_DATAW pfd )
{
	BOOL fSuccess = TRUE;
	
	DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: FSD_FindNextFileW\n" ) ) );

    EnterCriticalSectionMM(g_pcsMain);
    
	if( FindNext( pFindContext ) == NO_ERROR )
	{
	   	// Fill in the find struct
		if( pfd ) 
		{
			pfd->dwFileAttributes = pFindContext->fServerPB.fFileAttributes;
			LongToFileTime( pFindContext->fServerPB.fFileCreateTimeDate, &pfd->ftCreationTime );
			LongToFileTime( pFindContext->fServerPB.fFileTimeDate, &pfd->ftLastAccessTime );
			LongToFileTime( pFindContext->fServerPB.fFileTimeDate, &pfd->ftLastWriteTime );
			
			pfd->nFileSizeHigh = 0;
			pfd->nFileSizeLow = pFindContext->fServerPB.fSize;

			// Copy Unicode name.
	        lstrcpyW( pfd->cFileName, pFindContext->fServerPB.u.fLfn.fName );
		}
	}
	else
	{
		fSuccess = FALSE;
		SetLastError( ERROR_NO_MORE_FILES );
	}

    LeaveCriticalSectionMM(g_pcsMain);
    return fSuccess;
}


//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL VCEFSD_FindClose( FindContext* pFindContext )
{
    BOOL fSuccess = TRUE;

	DEBUGMSG( ZONE_APIS,( TEXT( "VCEFSD: FSD_FindNClose\n" ) ) );

    EnterCriticalSectionMM(g_pcsMain);
    FreeFindContext( pFindContext );
    LeaveCriticalSectionMM(g_pcsMain);
    
    return fSuccess;
}


//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
int FindOpen(	VolumeState *pVolume, 
				PCWSTR pwsFilePath, 
				PWIN32_FIND_DATAW pfd,
				FindContext ** outFindContext )
{
    FindContext*	pFindContext;
    int 			error = 0;      // assume success
	PCWSTR			namePtr;
	

    *outFindContext = NULL;
 
    // Allocate a Find Context
    pFindContext = AllocateFindContext();
    
    if( pFindContext == NULL )
        return ( error = ERROR_NOT_ENOUGH_MEMORY );

    error = Find(	pVolume,
    				gvpServerPB, 
  					pwsFilePath );
    
    if( error != 0 )
    {
        FreeFindContext( pFindContext );
        return error;
    }

	// Reset index for FindNext and capture the TransactionID from the FindContext
	gvpServerPB->fIndex = -1;
    gvpServerPB->fFindTransactionID = pFindContext->fServerPB.fFindTransactionID;

	// copy over data...
	memcpy( &pFindContext->fServerPB, gvpServerPB, sizeof( ServerPB ) );

	// Get name for find next...
	namePtr = pwsFilePath + lstrlenW( pwsFilePath ) - 1;
	for( ; namePtr > pwsFilePath; namePtr-- )
	{
		if( *namePtr == L'\\' )
		{
			break;
		}
	}
	namePtr++;

	// Save name to continue FindnNext search...
	lstrcpyW( pFindContext->fFindName, namePtr );

	if( wcscmp( pFindContext->fFindName, L"*.*" ) == 0 )
	{
		pFindContext->fFindName[1] = 0;
        pFindContext->fFindName[2] = 0;
    }	
	FindNext( pFindContext ); // First call to FindNext sets file index to found file.

    // Return info to caller
    *outFindContext = pFindContext;
    return error;
}

//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
int  Find(   VolumeState* 	pVolume,
				ServerPB* 		ioServerPB,
                PCWSTR			pwsPath )
{
	USHORT result;
	PWCHAR ptrName;
	USHORT charsRemaining, maxPathElementLength;
	USHORT pathElementLength = 0;

	// initialize & setup the Virtual PC control block
	memset( ioServerPB, 0, sizeof(ServerPB) );
	
        ioServerPB->fStructureSize = sizeof(ServerPB);
        ioServerPB->fFindTransactionID = 0xffffffff;
        ioServerPB->fWildCard = FALSE;
	ioServerPB->fHandle = kInvalidSrvHandle;        // to recognize an unopened file
		
	// find each directory node in the path
	// We find it hard to believe the control blocks for the parent
	//  directory are unavailable.
	ptrName = ( PWCHAR )pwsPath;
	for( charsRemaining = lstrlenW( pwsPath ); 
  		 charsRemaining > 1;
  		 charsRemaining -= ( pathElementLength + 1),// count the leading slash
         ptrName += ( pathElementLength + 1))      	// count the leading slash
	{
		// search for another level
		for( pathElementLength = 1, maxPathElementLength = lstrlenW( pwsPath ) - ( ptrName - pwsPath );
		     pathElementLength < maxPathElementLength &&  ptrName[pathElementLength] != L'\\'; 
		     pathElementLength++ )  
		{
			// Let it be known that we have wildcards...
			if( ptrName[ pathElementLength] == L'*' ||
				ptrName[ pathElementLength] == L'?' )
			{
				ioServerPB->fWildCard = TRUE;
			}
		}
		
		if( pathElementLength >= maxPathElementLength )	// if the last node
			break;								// we are done here

		ptrName[pathElementLength] = 0;

		// find the node at the server
                pathElementLength--;
		result = FindPathElement( ioServerPB, pwsPath );
		
		if( result != 0)
			return -1;

		ptrName[pathElementLength+1] = L'\\';

		// ensure we found a directory node
		if( ( ioServerPB->fFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 )
			return -1;

  	}  // loop once for each directory level

	// look for the bottom node in the path
	// It is alright if this node does not exist.
  	if(  charsRemaining <= 1 )
    	return 0;
    
        pathElementLength--;
 	result = FindPathElement( ioServerPB, pwsPath);
 
	if(  result != 0)
	{
		return -1;
	}
	
  	return 0;
}


//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
int FindNext( FindContext  * pFindContext )
{
    ULONG      result;
    
    // Get ptrs to the data we will need
	if( pFindContext == NULL )
	{
		return ERROR_INVALID_PARAMETER;
	}
	
    // Copy saved data to global PB
	memcpy( gvpServerPB, &pFindContext->fServerPB, sizeof( ServerPB ) );

	// Search for the next file
	do
	{
        // Move to the next file index
        gvpServerPB->fIndex++;

        // Get info on the next file
        result = ServerGetInfo( gvpServerPB );
        if( result != 0)
        {
            // No more files
            return ERROR_NO_MORE_FILES;
        }

		if( MatchesWildcard( lstrlenW( pFindContext->fFindName ), pFindContext->fFindName, lstrlenW( gvpServerPB->u.fLfn.fName ), gvpServerPB->u.fLfn.fName ) )
		{
			// Copy over new PB..
			memcpy( &pFindContext->fServerPB, gvpServerPB, sizeof( ServerPB ) );
			return NO_ERROR;
		}

    }while( 1 );
}


//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
USHORT FindPathElement( PServerPB pb, PCWSTR ptrName)
{
	USHORT 			result;
  
	pb->fIndex = -1;                         // only search for a name

	if( pb->fWildCard)
		pb->fIndex++;

	pb->u.fLfn.fNameLength = lstrlenW(ptrName) * sizeof(WCHAR);
	lstrcpyW( pb->u.fLfn.fName, ptrName );

	result = ServerGetInfo( pb);
	return result;                           // includes a successful return
} 



//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL MatchesWildcard(DWORD lenWild, PCWSTR pchWild, DWORD lenFile, PCWSTR pchFile)
{
    while( lenWild && lenFile )
	{
        if( *pchWild == L'*' )
		{
            pchWild++;
            if( --lenWild )
			{
                while( lenFile ) 
				{
                    if( MatchesWildcard( lenWild, pchWild, lenFile--, pchFile++ ) )
                        return TRUE;
                }
                return FALSE;
            }
            return TRUE;
        }
		else if( ( *pchWild != L'?' ) && _wcsnicmp( pchWild, pchFile, 1 ) )
		{
            return FALSE;
        }
		
		lenWild--;
        pchWild++;
        lenFile--;
        pchFile++;
    }
    
	// Done?
	if( !lenWild && !lenFile )
	{
        return TRUE;
	}

    if( !lenWild )
	{
        return FALSE;
	}
    else 
	{
		while( lenWild-- )
		{
			if( *pchWild++ != L'*' )
				return FALSE;
		}
	}
    return TRUE;
}

//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
void InitFind( void )
{
 int i;

	for( i = 0; i < kMaxFC; i++ )
	{
		gFCList[i].fInUse = FALSE;
		memset( &gFCList[i].fServerPB, 0, sizeof(ServerPB) );
		memset( gFCList[i].fFindName, 0, sizeof(gFCList[i].fFindName) );
    }
}


//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
FindContext * AllocateFindContext( void )
{
    int i;

	for( i = 0; i < kMaxFC; i++ )
	{
		if( gFCList[i].fInUse == FALSE )
		{
			gFCList[i].fInUse = TRUE;

			// Welcome to the department of redundency department...
			memset( &gFCList[i].fServerPB, 0, sizeof(ServerPB) );
			memset( gFCList[i].fFindName, 0, sizeof(gFCList[i].fFindName) );

                        gFCList[i].fServerPB.fStructureSize = sizeof(ServerPB);
                        gFCList[i].fServerPB.fFindTransactionID = i;
			return &gFCList[i];
		}
    }

    return NULL;
}


//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
BOOL FreeFindContext( FindContext * inFCPtr )
{
 int i;

	for( i = 0; i < kMaxFC; i++ )
	{
		if( inFCPtr == & gFCList[i] )
		{
			inFCPtr->fInUse = FALSE;
			memset( &inFCPtr->fServerPB, 0, sizeof(ServerPB) );
			memset( inFCPtr->fFindName, 0, sizeof(inFCPtr->fFindName) );
			return TRUE;
		}
    }
	return FALSE;
}


