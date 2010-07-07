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
//------------------------------------------------------------------------------
//
//  File:  ioctl_tab.h
//
//  Configuration file for the OAL IOCTL component.
//
//  This file is included by the platform's ioctl.c file and defines the 
//  global IOCTL table, g_oalIoCtlTable[]. Therefore, this file may ONLY
//  define OAL_IOCTL_HANDLER entries. 
//
// IOCTL CODE,                          Flags   Handler Function
//------------------------------------------------------------------------------

#include <oal_ioctl_tab.h>

// Required Termination
{ 0,                                        0,  NULL                        }

//------------------------------------------------------------------------------
