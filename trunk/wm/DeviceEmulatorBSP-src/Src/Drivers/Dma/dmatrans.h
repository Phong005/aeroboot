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
	File:		DMATrans.h
	
	Contains:	Stream-interface CE driver for the Emulator's
				DMATransportDevice. This driver provides a stream
				interface for applications running in the emulator
				to communicate with applications running on the
				host machine (outside the emulator).
				
				Communication on the other side of the device
				is done via the Emulator's IVirtualTransport COM
				interface.
				
	Written by:	Paul Pearcy
	
	Copyright:	2001 Connectix Corporation
==================================================================*/

/*------------------------------------------------------------------
	Prototypes
------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

#define FILE_DEVICE_DMA 32768
#define DMA_CHANNEL_INFO 1
#define DMA_CHANNEL_CONNECTED 2
#define IOCTL_GET_DMA_CHANNEL_INFO      CTL_CODE(FILE_DEVICE_DMA, DMA_CHANNEL_INFO, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_DMA_CHANNEL_CONNECTED CTL_CODE(FILE_DEVICE_DMA, DMA_CHANNEL_CONNECTED, METHOD_BUFFERED, FILE_ANY_ACCESS)

DWORD	DMA_Init( DWORD registryPath );
BOOL	DMA_Deinit( DWORD channel );
DWORD	DMA_Open( DWORD channelNumber, DWORD access, DWORD shareMode );
BOOL	DMA_Close( DWORD channel );
DWORD	DMA_Write( DWORD channel, LPCVOID writeBytes,  DWORD numBytes );
DWORD	DMA_Read( DWORD channel, LPVOID buffer, DWORD numRead );

#ifdef __cplusplus
}
#endif


/*==================================================================
	Change History (most recent first):
	
	$Log: dmatrans.h,v $
	Revision 1.3  2001/09/13 19:48:27  jhoelter
	Small bug fixes plus major clean-up of files.
	
	
==================================================================*/
