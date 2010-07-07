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
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.
Copyright (c) 2002. Samsung Electronics, co. ltd  All rights reserved.

Module Name:  

Abstract:

	This file implements the S3C2410 Keyboard function

rev:
	2002.4.4	: First S3C2410 version (kwangyoon LEE, kwangyoon@samsung.com)

	2002.1.31	: CE.NET port (kwangyoon LEE, kwangyoon@samsung.com)

Notes: 
--*/

#ifndef __PS2KEYBD_HPP_INCLUDED__
#define __PS2KEYBD_HPP_INCLUDED__


#include <windows.h>

#define MATRIX_PDD 8

class Ps2Port;

class Ps2Keybd
	{
	HANDLE	m_hevInterrupt;


public:
	BOOL
	Initialize(
		void
		);

	BOOL
	IsrThreadStart(
		void
		);

	BOOL
	IsrThreadProc(
		void
		);

	BOOL
	KeybdPowerOff(
		void
		);

	BOOL
	KeybdPowerOn(
		void
		);

friend
	void
	KeybdPdd_PowerHandler(
		BOOL bOff
		);

friend
	int
	WINAPI
	KeybdPdd_GetEventEx(
		UINT32			VKeyBuf[16],
		UINT32			ScanCodeBuf[16],
		KEY_STATE_FLAGS	KeyStateFlagsBuf[16]
		);

friend
	void
	WINAPI
	KeybdPdd_ToggleKeyNotification(
		KEY_STATE_FLAGS	KeyStateFlags
		);


	};













#endif





