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
	File:		emulserv.h
	
	Contains:	
				
	Written by:	Craig Vinet
	
	Copyright:	2002 Connectix Corporation
==================================================================*/

/*------------------------------------------------------------------
	Prototypes
------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

DWORD	EMS_Init( DWORD registryPath );
BOOL	EMS_Deinit( DWORD dwData );
DWORD	EMS_Open( DWORD dwData, DWORD access, DWORD shareMode );
BOOL	EMS_Close( DWORD dwData );
DWORD	EMS_Write( DWORD dwData, LPCVOID writeBytes,  DWORD numBytes );
DWORD	EMS_Read( DWORD dwData, LPVOID buffer, DWORD numRead );


#ifdef __cplusplus
}
#endif


/*==================================================================
	Change History (most recent first):
	
	$Log: emulserv.h,v $
	
==================================================================*/
