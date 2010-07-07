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
/******************************************************************************
 *
 * System On Chip(SOC)
 *
 * Copyright (c) 2002 Software Center, Samsung Electronics, Inc.
 * All rights reserved.
 *
 * This software is the confidential and proprietary information of Samsung
 * Electronics, Inc("Confidential Information"). You Shall not disclose such
 * Confidential Information and shall use it only in accordance with the terms
 * of the license agreement you entered into Samsung.
 *
 *-----------------------------------------------------------------------------
 *
 *  S3C2410 BSP (Windows CE.NET)
 *
 * pwrbtn2410.h : Power Button Driver
 *
 * @author      zartoven@samsung.com (SOC, SWC, SAMSUNG Electronics)
 *
 * @date        2002/04/25
 *
 * Log:
 *  2002/04/25  Start
 *
 ******************************************************************************
 */

#ifndef __PWRBTN2410_H__
#define __PWRBTN2410_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _DISK 
{
    struct _DISK *		d_next;
    CARD_CLIENT_HANDLE	d_hPcmcia;   
    CARD_SOCKET_HANDLE	d_hSock;     
    CARD_WINDOW_HANDLE	d_hSRAM[3];
    volatile PUCHAR		d_pSRAM[3];
    DWORD				d_CardAddr[3];
    DWORD				d_LastWindow;
    CRITICAL_SECTION	d_DiskCardCrit;
    DWORD				d_DiskCardState;
    DWORD				d_CIS_Size;       
    DISK_INFO			d_DiskInfo;   
    DWORD				d_OpenCount;      
    LPWSTR				d_ActivePath;    
} DISK, * PDISK; 


#ifdef __cplusplus
}
#endif

#endif // __PWRBTN2410_H__

