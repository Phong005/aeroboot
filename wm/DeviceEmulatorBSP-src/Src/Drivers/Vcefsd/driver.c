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
THIS CODE AND INFORMATION IS PROVIDED \"AS IS\" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.
--*/

#include <windows.h>
#include <types.h>
#include <winerror.h>
#include <devload.h>
#include <diskio.h>
#include <vcefsd.h>

/*****************************************************************************/


/* This driver exists only to cause the RELFSD to get loaded into the
 * system. It's silly that there's no other way to get an FSD loaded.
 */


/*****************************************************************************/


DWORD DSK_Init(DWORD dwContext)
{
    return 1;
}


/*****************************************************************************/


BOOL DSK_DeinitClose(DWORD dwContext)
{
    return TRUE;
}


/*****************************************************************************/


DWORD DSK_Dud(DWORD dwData,DWORD dwAccess,DWORD dwShareMode)
{
    return dwData;
}


/*****************************************************************************/


BOOL DSK_IOControl(DWORD Handle, DWORD dwIoControlCode,
                   PBYTE pInBuf, DWORD nInBufSize,
                   PBYTE pOutBuf, DWORD nOutBufSize,
                   PDWORD pBytesReturned)
{
    return FALSE;
}


/*****************************************************************************/


void DSK_Power(void)
{
}
