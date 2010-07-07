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

@doc    EXTERNAL

@module sctovk.cpp |

Sample implementation of the keyboard platform dependent scan code to
Virtual Key mapping for keyboard driver.

Exports ScanCodeToVKey for the PDD to use to map scan codes to virtual
keys.  A version of this will be needed for every physical/virtual key
configuration.

*/

#include <windows.h>
#include <keybddr.h>
#include <laymgr.h>
#include <devicelayout.h>

#include "s3c2410kbd.hpp"


#define VK_MATRIX_FN   0xC1


#define ScanCodeTableFirst  0x00
#define ScanCodeTableLast   0x6F
UINT8 ScanCodeToVKeyTable[] =
{
	0,  // Scan Code 0x0
	VK_MENU, // Scan Code 0x1
	0, // Scan Code 0x2
	0, // Scan Code 0x3
	0, // Scan Code 0x4
	0, // Scan Code 0x5
	0, // Scan Code 0x6
	0, // Scan Code 0x7
	0, // Scan Code 0x8
	VK_BACKQUOTE, // Scan Code 0x9 
	VK_BACKSLASH, // Scan Code 0xA
	VK_TAB, // Scan Code 0xB
	'Z', // Scan Code 0xC
	'A', // Scan Code 0xD
	'X', // Scan Code 0xE
	0, // Scan Code 0xF
	0, // Scan Code 0x10
	0, // Scan Code 0x11
	VK_LSHIFT, // Scan Code 0x12
	0, // Scan Code 0x13
	0, // Scan Code 0x14
	0, // Scan Code 0x15
	0, // Scan Code 0x16
	0, // Scan Code 0x17
	0, // Scan Code 0x18
	VK_CONTROL, // Scan Code 0x19
	0, // Scan Code 0x1A
	0, // Scan Code 0x1B
	0, // Scan Code 0x1C
	0, // Scan Code 0x1D
	0, // Scan Code 0x1E
	0, // Scan Code 0x1F
	0, // Scan Code 0x20
	VK_MATRIX_FN, // Scan Code 0x21, Fn
	0, // Scan Code 0x22
	0, // Scan Code 0x23
	0, // Scan Code 0x24
	0, // Scan Code 0x25
	0, // Scan Code 0x26
	0, // Scan Code 0x27
	0, // Scan Code 0x28
	VK_ESCAPE, // Scan Code 0x29
	VK_DELETE, // Scan Code 0x2A
	'Q', // Scan Code 0x2B
	VK_CAPITAL, // Scan Code 0x2C
	'S', // Scan Code 0x2D
	'C', // Scan Code 0x2E
	'3', // Scan Code 0x2F
	0, // Scan Code 0x30
	'1', // Scan Code 0x31
	0, // Scan Code 0x32
	'W', // Scan Code 0x33
	0, // Scan Code 0x34
	'D', // Scan Code 0x35
	'V', // Scan Code 0x36
	'4', // Scan Code 0x37
	0, // Scan Code 0x38
	'2', // Scan Code 0x39
	'T', // Scan Code 0x3A
	'E', // Scan Code 0x3B
	0, // Scan Code 0x3C
	'F', // Scan Code 0x3D
	'B', // Scan Code 0x3E
	'5', // Scan Code 0x3F
	0, // Scan Code 0x40
	'9', // Scan Code 0x41
	'Y', // Scan Code 0x42
	'R', // Scan Code 0x43
	'K', // Scan Code 0x44
	'G', // Scan Code 0x45
	'N', // Scan Code 0x46
	'6', // Scan Code 0x47
	0, // Scan Code 0x48
	'0', // Scan Code 0x49
	'U', // Scan Code 0x4A
	'O', // Scan Code 0x4B
	'L', // Scan Code 0x4C
	'H', // Scan Code 0x4D
	'M', // Scan Code 0x4E
	'7', // Scan Code 0x4F
	0, // Scan Code 0x50
	VK_HYPHEN, // Scan Code 0x51
	'I', // Scan Code 0x52
	'P', // Scan Code 0x53
	VK_SEMICOLON, // Scan Code 0x54
	'J', // Scan Code 0x55
	VK_COMMA, // Scan Code 0x56
	'8', // Scan Code 0x57
	0, // Scan Code 0x58
	VK_EQUAL, // Scan Code 0x59
	VK_RETURN, // Scan Code 0x5A
	VK_LBRACKET, // Scan Code 0x5B
	VK_APOSTROPHE, // Scan Code 0x5C
	VK_SLASH, // Scan Code 0x5D
	VK_PERIOD, // Scan Code 0x5E
	VK_RWIN, // Scan Code 0x5F, Prog
	VK_LWIN, // Scan Code 0x60
	0, // Scan Code 0x61
	VK_RSHIFT, // Scan Code 0x62
	VK_MATRIX_FN, // Scan Code 0x63
	VK_APP_LAUNCH1, // Scan Code 0x64
	VK_APP_LAUNCH2, // Scan Code 0x65
	VK_APP_LAUNCH3, // Scan Code 0x66
	VK_APP_LAUNCH4, // Scan Code 0x67
	VK_APP_LAUNCH5, // Scan Code 0x68
	VK_BACK, // Scan Code 0x69
	VK_DOWN, // Scan Code 0x6A
	VK_RBRACKET, // Scan Code 0x6B
	VK_UP, // Scan Code 0x6C
	VK_LEFT, // Scan Code 0x6D
	VK_SPACE, // Scan Code 0x6E
	VK_RIGHT, // Scan Code 0x6F
};

static ScanCodeToVKeyData scvkEngUS = 
{
    0,
    ScanCodeTableFirst,
    ScanCodeTableLast,
    ScanCodeToVKeyTable
};

static ScanCodeToVKeyData *rgscvkMatrixEngUSTables[] = 
    { &scvkEngUS };


struct VirtualKeyMapping {
    UINT32 uiVk;
    UINT32 uiVkGenerated;
};

static const VirtualKeyMapping g_rgvkMapFn[] = {
    {  '1', VK_F1 },
    {  '2', VK_F2 },
    {  '3', VK_F3 },
    {  '4', VK_F4 },
    {  '5', VK_F5 },
    {  '6', VK_F6 },
    {  '7', VK_F7 },
    {  '8', VK_F8 },
    {  '9', VK_F9 },
    {  '0', VK_F10 },
    { VK_HYPHEN, VK_NUMLOCK },
    { VK_EQUAL, VK_CANCEL },
    {  'P', VK_INSERT },
    { VK_LBRACKET, VK_PAUSE },
    { VK_RBRACKET, VK_SCROLL },
    { VK_SEMICOLON, VK_SNAPSHOT },
    { VK_APOSTROPHE, VK_SNAPSHOT },
    {  VK_LEFT, VK_HOME },
    {  VK_UP, VK_PRIOR},
    {  VK_DOWN, VK_NEXT },
    {  VK_RIGHT, VK_END },
};

static const VirtualKeyMapping g_rgvkMapNumLock[] = {
    {  '7', VK_NUMPAD7 },
    {  '8', VK_NUMPAD8 },
    {  '9', VK_NUMPAD9 },
    {  '0', VK_MULTIPLY },
    {  'U', VK_NUMPAD4 },
    {  'I', VK_NUMPAD5 },
    {  'O', VK_NUMPAD6 },
    {  'P', VK_SUBTRACT },
    {  'J', VK_NUMPAD1 },
    {  'K', VK_NUMPAD2 },
    {  'L', VK_NUMPAD3 },
    {  VK_SEMICOLON, VK_ADD },
    {  'M', VK_NUMPAD0 },
    {  VK_PERIOD, VK_DECIMAL },
    {  VK_SLASH, VK_DIVIDE },
};


// Find a virtual key mapping in the given array.
static
const VirtualKeyMapping * 
FindRemappedKey(
    UINT32 uiVk,
    const VirtualKeyMapping *pvkMap,
    DWORD cvkMap
    )
{
    const VirtualKeyMapping *pvkMapMatch = NULL;
    UINT ui;
    
    DEBUGCHK(pvkMap);

    for (ui = 0; ui < cvkMap; ++ui) {
        if (pvkMap[ui].uiVk == uiVk) {
            pvkMapMatch = &pvkMap[ui];
            break;
        }
    }

    return pvkMapMatch;
}


#define IS_NUMLOCK_ON(ksf) (ksf & KeyShiftNumLockFlag)

// Remapping function for the matrix keyboard
static
UINT
WINAPI
MatrixUsRemapVKey(
    const KEYBD_EVENT *pKbdEvents,
    UINT               cKbdEvents,
    KEYBD_EVENT       *pRmpKbdEvents,
    UINT               cMaxRmpKbdEvents
    )
{
    SETFNAME(_T("MatrixUsRemapVKey"));
    
    static BOOL fFnDown = FALSE;
    
    UINT cRmpKbdEvents = 0;
    UINT ui;

    if (pRmpKbdEvents == NULL) {
        // 1 to 1 mapping
        DEBUGCHK(cMaxRmpKbdEvents == 0);
        return cKbdEvents;
    }
    
    DEBUGCHK(pKbdEvents != NULL);

    if (cMaxRmpKbdEvents < cKbdEvents) {
        DEBUGMSG(ZONE_ERROR, (_T("%s: Buffer is not large enough!\r\n"),
            pszFname));
        return 0;
    }
    
    for (ui = 0; ui < cKbdEvents; ++ui) {
        const KEYBD_EVENT *pKbdEventCurr = &pKbdEvents[ui];
        KEYBD_EVENT *pKbdEventRmpCurr = &pRmpKbdEvents[cRmpKbdEvents];

        // Copy the input key event to our remapped list
        pKbdEventRmpCurr->uiVk = pKbdEventCurr->uiVk;
        pKbdEventRmpCurr->uiSc = pKbdEventCurr->uiSc;
        pKbdEventRmpCurr->KeyStateFlags = pKbdEventCurr->KeyStateFlags;

        const VirtualKeyMapping *pvkMap = NULL;
        BOOL fKeyDown = (pKbdEventCurr->KeyStateFlags & KeyStateDownFlag) != 0;
        UINT32 uiVkCurr = pKbdEventCurr->uiVk;
	UINT32 uiScCurr = pKbdEventCurr->uiSc;

        if (uiVkCurr == VK_MATRIX_FN && uiScCurr == 0x63 ) {
            fFnDown = fKeyDown;
            // Fn virtual key does not get sent to the system so
            // do not increment cRmpKbdEvents.
            DEBUGMSG(ZONE_DEVICELAYOUT, (_T("%s: Fn key is now %s\r\n"),
                pszFname, (fFnDown ? _T("DOWN") : _T("UP"))));
        }
        else {
            // We have one key event
            ++cRmpKbdEvents;

            if (fKeyDown) {
                // Handle key down
                if (fFnDown) {
                    // Fn key is on
                    if (IS_NUMLOCK_ON(pKbdEventCurr->KeyStateFlags)) {
                        pvkMap = FindRemappedKey(uiVkCurr,
                            g_rgvkMapNumLock, dim(g_rgvkMapNumLock));
                    }

                    if (pvkMap == NULL) {
                        // NumLock did not effect this key. See if the
                        // Fn key by itself does.                        
                        pvkMap = FindRemappedKey(uiVkCurr, 
                            g_rgvkMapFn, dim(g_rgvkMapFn));
                    }
                }
            }
            else {
                // Handle key up
                if (fFnDown) {
                    // Fn key is on
                    if (IS_NUMLOCK_ON(pKbdEventCurr->KeyStateFlags)) {
                        pvkMap = FindRemappedKey(uiVkCurr,
                            g_rgvkMapNumLock, dim(g_rgvkMapNumLock));
                    }

                    if (pvkMap == NULL) {
                        // NumLock did not effect this key. See if the
                        // Fn key by itself does.                        
                        pvkMap = FindRemappedKey(uiVkCurr, 
                            g_rgvkMapFn, dim(g_rgvkMapFn));
                    }
                }
            }

            if (pvkMap != NULL) {
                // This combination generates a different virtual key
                DEBUGCHK(pvkMap->uiVkGenerated != 0);
                pKbdEventRmpCurr->uiVk = pvkMap->uiVkGenerated;
            }
        }
    }

    return cRmpKbdEvents;    
}
    

static DEVICE_LAYOUT dlMatrixEngUs =
{
    sizeof(DEVICE_LAYOUT),
    MATRIX_PDD,
    rgscvkMatrixEngUSTables,
    dim(rgscvkMatrixEngUSTables),
    MatrixUsRemapVKey,
};

extern "C"
BOOL
Matrix(
    PDEVICE_LAYOUT pDeviceLayout
    )
{
    DEBUGCHK(pDeviceLayout != NULL);

    BOOL fRet = FALSE;

    if (pDeviceLayout->dwSize != sizeof(DEVICE_LAYOUT)) {
        RETAILMSG(1, (_T("Matrix: data structure size mismatch\r\n")));
        goto leave;
    }

    // Make sure that the Sc->Vk tables are the sizes that we expect
    DEBUGCHK(dim(ScanCodeToVKeyTable) == (1 + ScanCodeTableLast - ScanCodeTableFirst));

    *pDeviceLayout = dlMatrixEngUs;

    fRet = TRUE;

leave:
    return fRet;
}
#ifdef DEBUG
// Verify function declaration against the typedef.
static PFN_DEVICE_LAYOUT_ENTRY v_pfnDeviceLayout = Matrix;
#endif

