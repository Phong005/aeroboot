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
#include <windows.h>
#include <keybdpdd.h>

// Add NOP driver for USB HID and RDP support.
BOOL
WINAPI
PS2_NOP_Entry(
    UINT uiPddId,
    PFN_KEYBD_EVENT pfnKeybdEvent,
    PKEYBD_PDD *ppKeybdPdd
    );

BOOL
WINAPI
Matrix_Entry(
    UINT uiPddId,
    PFN_KEYBD_EVENT pfnKeybdEvent,
    PKEYBD_PDD *ppKeybdPdd
    );

PFN_KEYBD_PDD_ENTRY g_rgpfnPddEntries[] = {
    PS2_NOP_Entry,
    Matrix_Entry,
    NULL
};

