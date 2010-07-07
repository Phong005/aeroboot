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
#include <ceddk.h>
#include <keybddbg.h>
#include <keybdpdd.h>

#include "s3c2410kbd.hpp"

extern Ps2Keybd	*v_pp2k;

UINT v_uiPddId;
PFN_KEYBD_EVENT v_pfnKeybdEvent;

void
ReadRegDWORD(
	LPCWSTR	szKeyName,
	LPCWSTR szValueName,
    LPDWORD pdwValue
    )
{
    HKEY hKeybd;
    DWORD ValType;
    DWORD ValLen;
    DWORD status;

    //
    // Get the device key from the active device registry key
    //
    status = RegOpenKeyEx(
                HKEY_LOCAL_MACHINE,
                szKeyName,
                0,
                0,
                &hKeybd);
    if (status) {
        DEBUGMSG(1, (TEXT("ReadRegDWORD: RegOpenKeyEx(HLM\\%s) returned %d!!!\r\n"),
                  szKeyName, status));
		*pdwValue = 0;   // Fail
        return;
    }

    ValLen = sizeof(DWORD);
    status = RegQueryValueEx(       // Retrieve the value
                hKeybd,
                szValueName,
                NULL,
                &ValType,
                (PUCHAR)pdwValue,
                &ValLen);
    if (status != ERROR_SUCCESS) {
        DEBUGMSG(1, (TEXT("ReadRegDWORD: RegQueryValueEx(%s) returned %d\r\n"),
                  szValueName, status));
		*pdwValue = 0;   // Fail
    }

	DEBUGMSG(1, (_T("ReadRegDWORD(): %s -> %s is 0x%x\r\n"), szKeyName, szValueName, *pdwValue));

    RegCloseKey(hKeybd);
}   // OpenDeviceKey


void
WINAPI
KeybdPdd_PowerHandler(
	BOOL	bOff
	);


static
void
WINAPI
Matrix_PowerHandler(
    UINT uiPddId,
    BOOL fTurnOff
    )
{
    KeybdPdd_PowerHandler(fTurnOff);
}

static
void 
WINAPI
Matrix_ToggleLights(
    UINT uiPddId,
    KEY_STATE_FLAGS KeyStateFlags
    )
{
	static const KEY_STATE_FLAGS ksfLightMask = KeyShiftCapitalFlag | 
		KeyShiftNumLockFlag | KeyShiftScrollLockFlag; 
	static KEY_STATE_FLAGS ksfCurr;

    SETFNAME(_T("Matrix_ToggleLights"));

	KEY_STATE_FLAGS ksfNewState = (ksfLightMask & KeyStateFlags);

	if (ksfNewState != ksfCurr) 
    {
        DEBUGMSG(ZONE_PDD, (_T("%s: PDD %u: Changing light state\r\n"), 
            pszFname, uiPddId));
		KeybdPdd_ToggleKeyNotification(ksfNewState);
		ksfCurr = ksfNewState;
	}

    return;
}


static KEYBD_PDD MatrixPdd = {
    MATRIX_PDD,
    _T("Matrix"),
    Matrix_PowerHandler,
    Matrix_ToggleLights
};

BOOL KeybdDriverInitializeAddresses(void);

BOOL
WINAPI
Matrix_Entry(
    UINT uiPddId,
    PFN_KEYBD_EVENT pfnKeybdEvent,
    PKEYBD_PDD *ppKeybdPdd
    )
{
    SETFNAME(_T("PS2_8042_Entry"));

	BOOL fRet = FALSE;

    v_uiPddId = uiPddId;
    v_pfnKeybdEvent = pfnKeybdEvent;

    DEBUGMSG(ZONE_INIT, (_T("%s: Initialize Matrix ID %u\r\n"), pszFname, 
        uiPddId));
    DEBUGCHK(ppKeybdPdd != NULL);

    *ppKeybdPdd = &MatrixPdd;
	
    if (v_pp2k) {
        fRet = TRUE;
        goto leave;
    }
    //	We always assume that there is a keyboard.
    v_pp2k = new Ps2Keybd;
    if (v_pp2k->Initialize()) {
        v_pp2k ->IsrThreadStart();
    } 
    else {
        ERRORMSG(1,(TEXT("Could not initialize ps2 keyboard.\r\n")));
        delete v_pp2k;
        v_pp2k = NULL;
    }

	if (!KeybdDriverInitializeAddresses()) {
		goto leave;
	}

    if (v_pp2k)
    {
	    v_pp2k->KeybdPowerOn();
    }

	fRet = TRUE;
		
leave:
    DEBUGMSG(ZONE_INIT, (_T("%s: Initialization complete\r\n"), pszFname));
	return fRet;
}
#ifdef DEBUG
// Verify function declaration against the typedef.
static PFN_KEYBD_PDD_ENTRY v_pfnKeybdEntry = Matrix_Entry;
#endif


