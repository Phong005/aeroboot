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
        File:           SerDMA.c

        Contains:       Serial-over-DMA driver.

==================================================================*/

#include <windows.h>
#include <serhw.h>
#include <ser16550.h>
#include "serdma.h"

#undef ZONE_INIT
#include <serdbg.h>

#define DEBUGMODE 0


/*
 @doc OEM 
 @func PVOID | SerInit | Initializes device identified by argument.
 *  This routine sets information controlled by the user
 *  such as Line control and baud rate. It can also initialize events and
 *  interrupts, thereby indirectly managing initializing hardware buffers.
 *  Exported only to driver, called only once per process.
 *
 @rdesc The return value is a PVOID to be passed back into the HW
 dependent layer when HW functions are called.
 */
static
PVOID
SerInit(
		BOOL bIR,
		ULONG   Identifier, // @parm Device identifier.
		PVOID   pMddHead,   // @parm First argument to mdd callbacks.
		PHWOBJ  pHWObj      // @parm Pointer to our own HW OBJ for this device
       )
{
    PSER_INFO pHWHead;
    HKEY hKey;
    DWORD dwDataSize;
    DWORD DMAChannelNumber;

    RETAILMSG(DEBUGMODE, (TEXT("SerInit\r\n")));

    // Allocate for our main data structure and one of it's fields.
    pHWHead = (PSER_INFO)LocalAlloc( LMEM_ZEROINIT|LMEM_FIXED, sizeof(SER_INFO) );
    if ( !pHWHead ) {
        return NULL;
    }

    hKey = OpenDeviceKey((LPCTSTR)Identifier);
    if ( hKey == NULL ) {
	DEBUGMSG(ZONE_INIT | ZONE_ERROR,(TEXT("Failed to open device key\r\n")));
	return FALSE;
    }
    dwDataSize = sizeof(DMAChannelNumber);
    if ( RegQueryValueEx(hKey, L"DMAChannel", NULL, NULL,
                         (LPBYTE)&DMAChannelNumber, &dwDataSize) || DMAChannelNumber > 9) {
        DEBUGMSG (ZONE_INIT | ZONE_ERROR,
                  (TEXT("Failed to get DMAChannel value, SerInit failed\r\n")));
        RegCloseKey (hKey);
        LocalFree(pHWHead);
        return NULL;
    }
    RegCloseKey (hKey);

    pHWHead->pMddHead = pMddHead;
    pHWHead->pHWObj = pHWObj;

    // Set up our Comm Properties data    
    pHWHead->CommProp.wPacketLength       = 0xffff;
    pHWHead->CommProp.wPacketVersion     = 0xffff;
    pHWHead->CommProp.dwServiceMask      = SP_SERIALCOMM;
    pHWHead->CommProp.dwReserved1         = 0;
    pHWHead->CommProp.dwMaxTxQueue        = 0;
    pHWHead->CommProp.dwMaxRxQueue        = 0;
    pHWHead->CommProp.dwMaxBaud       = BAUD_115200;
    pHWHead->CommProp.dwProvSubType      = PST_RS232;
    pHWHead->CommProp.dwProvCapabilities =
		PCF_DTRDSR | PCF_RLSD | PCF_RTSCTS |
		PCF_SETXCHAR |
		PCF_INTTIMEOUTS |
		PCF_PARITY_CHECK |
		PCF_SPECIALCHARS |
		PCF_TOTALTIMEOUTS |
		PCF_XONXOFF;
    pHWHead->CommProp.dwSettableBaud      =
		BAUD_075 | BAUD_110 | BAUD_150 | BAUD_300 | BAUD_600 |
		BAUD_1200 | BAUD_1800 | BAUD_2400 | BAUD_4800 |
		BAUD_7200 | BAUD_9600 | BAUD_14400 |
		BAUD_19200 | BAUD_38400 | BAUD_56K | BAUD_128K |
		BAUD_115200 | BAUD_57600 | BAUD_USER;
    pHWHead->CommProp.dwSettableParams    =
		SP_BAUD | SP_DATABITS | SP_HANDSHAKING | SP_PARITY |
		SP_PARITY_CHECK | SP_RLSD | SP_STOPBITS;
    pHWHead->CommProp.wSettableData       =
		DATABITS_5 | DATABITS_6 | DATABITS_7 | DATABITS_8;
    pHWHead->CommProp.wSettableStopParity =
		STOPBITS_10 | STOPBITS_20 |
		PARITY_NONE | PARITY_ODD | PARITY_EVEN | PARITY_SPACE |
		PARITY_MARK;

    SL_Init( pHWHead, (PUCHAR)DMAChannelNumber, 0, EvaluateEventFlag, pMddHead, NULL);

    return pHWHead;
}

PVOID
SerInitSerial(
       ULONG   Identifier, // @parm Device identifier.
       PVOID   pMddHead,   // @parm First argument to mdd callbacks.
       PHWOBJ  pHWObj      // @parm Pointer to our own HW OBJ for this device
)
{
    RETAILMSG(DEBUGMODE, (TEXT("SerInitSerial\r\n")));
    return SerInit(FALSE, Identifier, pMddHead, pHWObj);
}

PVOID
SerInitIR(
       ULONG   Identifier, // @parm Device identifier.
       PVOID   pMddHead,   // @parm First argument to mdd callbacks.
       PHWOBJ  pHWObj      // @parm Pointer to our own HW OBJ for this device
)
{
    RETAILMSG(DEBUGMODE, (TEXT("SerInitIR\r\n")));
    return SerInit(TRUE, Identifier, pMddHead, pHWObj);
}

/*
 @doc OEM
 @func ULONG | SerClose | This routine closes the device identified by the PVOID returned by SerInit.
 *  Not exported to users, only to driver.
 *
 @rdesc The return value is 0.
 */
static
ULONG
SerClose(
        PVOID   pHead   // @parm PVOID returned by SerInit.
        )
{
    PSER_INFO   pHWHead = (PSER_INFO)pHead;

    RETAILMSG (DEBUGMODE,(TEXT("+SerClose\r\n")));

    if ( pHWHead->cOpenCount ) {
        DEBUGMSG (ZONE_CLOSE, (TEXT("SerClose, closing device\r\n")));
        pHWHead->cOpenCount--;

        DEBUGMSG (ZONE_CLOSE, (TEXT("SerClose - Calling SL_Close\r\n")));
        SL_Close( pHWHead );
    }

    RETAILMSG(DEBUGMODE,(TEXT("-SerClose\r\n")));
    return 0;
}

/*
 @doc OEM 
 @func PVOID | SerDeinit | Deinitializes device identified by argument.
 *  This routine frees any memory allocated by SerInit.
 *
 */
static
BOOL
SerDeinit(
         PVOID   pHead   // @parm PVOID returned by SerInit.
         )
{
    PSER_INFO   pHWHead = (PSER_INFO)pHead;

    RETAILMSG(DEBUGMODE,(TEXT("SerDeinit \r\n")));

    if ( !pHWHead )
        return FALSE;

    // Make sure device is closed before doing DeInit
    if ( pHWHead->cOpenCount )
        SerClose( pHead );

    // Free the HWObj
    LocalFree(pHWHead->pHWObj);

    // And now free the SER_INFO structure.
    LocalFree(pHWHead);

    return TRUE;
}

/*
 @doc OEM
 @func	VOID | SerGetCommProperties | Retrieves Comm Properties.
 *
 @rdesc	None.
 */
static
VOID
SerGetCommProperties(
                    PVOID   pHead,      // @parm PVOID returned by SerInit. 
                    LPCOMMPROP  pCommProp   // @parm Pointer to receive COMMPROP structure. 
                    )
{
    PSER_INFO   pHWHead = (PSER_INFO)pHead;

    RETAILMSG(DEBUGMODE,(TEXT("SerGetCommProperties\r\n")));

    *pCommProp = pHWHead->CommProp;
}

/*
 @doc OEM
 @func VOID | SerSetBaudRate |
 * This routine sets the baud rate of the device.
 *  Not exported to users, only to driver.
 *
 @rdesc None.
 */
static
BOOL
SerSetBaudRate(
              PVOID   pHead,  // @parm     PVOID returned by SerInit
              ULONG   BaudRate    // @parm     ULONG representing decimal baud rate.
              )
{
    PSER_INFO   pHWHead = (PSER_INFO)pHead;

    RETAILMSG(DEBUGMODE,(TEXT("SerSetBaudRate \r\n")));

    return SL_SetBaudRate( pHead, BaudRate );
}

/*
 @doc OEM
 @func BOOL | SerPowerOff |
 *  Called by driver to turn off power to serial port.
 *  Not exported to users, only to driver.
 *
 @rdesc This routine returns a status.
 */
static
BOOL
SerPowerOff(
           PVOID   pHead       // @parm	PVOID returned by SerInit.
           )
{
    PSER_INFO   pHWHead = (PSER_INFO)pHead;

    RETAILMSG(DEBUGMODE,(TEXT("SerPowerOff\r\n")));

    SL_PowerOff( pHWHead );

    return TRUE;
}

/*
 @doc OEM
 @func BOOL | SerPowerOn |
 *  Called by driver to turn on power to serial port.
 *  Not exported to users, only to driver.
 *
 @rdesc This routine returns a status.
 */
static
BOOL
SerPowerOn(
          PVOID   pHead       // @parm	PVOID returned by SerInit.
          )
{
    PSER_INFO   pHWHead = (PSER_INFO)pHead;

    RETAILMSG(DEBUGMODE,(TEXT("SerPowerOn\r\n")));

    SL_PowerOn( pHWHead );

    return TRUE;
}

/*
 @doc OEM
 @func BOOL | SerEnableIR | This routine enables ir.
 *  Not exported to users, only to driver.
 *
 @rdesc Returns TRUE if successful, FALSE otherwise.
 */
static
BOOL
SerEnableIR(
           PVOID   pHead, // @parm PVOID returned by Serinit.
           ULONG   BaudRate  // @parm PVOID returned by HWinit.
           )
{
    RETAILMSG(DEBUGMODE,(TEXT("SerEnableIR\r\n")));
    ASSERT(FALSE); // this is not called for SERDMA
    return FALSE; /* IR isn't supported */
}

/*
 @doc OEM
 @func BOOL | SerDisableIR | This routine disable the ir.
 *  Not exported to users, only to driver.
 *
 @rdesc Returns TRUE if successful, FALSE otherwise.
 */
static
BOOL
SerDisableIR(
            PVOID   pHead /*@parm PVOID returned by Serinit. */
            )
{
    RETAILMSG(DEBUGMODE,(TEXT("SerDisableIR\r\n")));
    ASSERT(FALSE); // this is never called for SERDMA
    return TRUE; /* IR isn't supported */
}

/*
 @doc OEM
 @func BOOL | SerEnableSerial | This routine enables serial (instead of IR).
 *  Not exported to users, only to driver.
 *
 @rdesc Returns TRUE if successful, FALSE otherwise.
 */
static
BOOL
SerEnableSerial(
           PVOID   pHead, // @parm PVOID returned by Serinit.
           ULONG   BaudRate  // @parm PVOID returned by HWinit.
           )
{
    RETAILMSG(DEBUGMODE,(TEXT("SerEnableSerial\r\n")));

    return TRUE;
}

/*
 @doc OEM
 @func BOOL | SerDisableSerial | This routine disable the serial, switching to IR.
 *  Not exported to users, only to driver.
 *
 @rdesc Returns TRUE if successful, FALSEotherwise.
 */
static
BOOL
SerDisableSerial(
            PVOID   pHead /*@parm PVOID returned by Serinit. */
            )
{
    RETAILMSG(DEBUGMODE,(TEXT("SerDisableSerial\r\n")));

    return FALSE; /* IR isn't supported */
}


/*
 @doc OEM
 @func BOOL | SerOpen | This routine is called when the port is opened.
 *  Not exported to users, only to driver.
 *
 @rdesc Returns TRUE if successful, FALSEotherwise.
 */
static
BOOL
SerOpen(
       PVOID   pHead /*@parm PVOID returned by Serinit. */
       )
{
    PSER_INFO   pHWHead = (PSER_INFO)pHead;

    // Disallow multiple simultaneous opens
    if ( pHWHead->cOpenCount )
        return FALSE;

    pHWHead->cOpenCount++;

    RETAILMSG(DEBUGMODE, (TEXT("SerOpen - Calling SL_Open\r\n")));
    SL_Open( pHWHead );
    RETAILMSG(DEBUGMODE, (TEXT("SerOpen - Return TRUE\r\n")));

    return TRUE;
}

const
HW_VTBL IoVTbl = {
	SerInitSerial,
	SL_PostInit,
	SerDeinit,
	SerOpen,
	SerClose,
	SL_GetInterruptType,
	SL_RxIntr,
	SL_TxIntrEx,
	SL_ModemIntr,
	SL_LineIntr,
	SL_GetRxBufferSize,
	SerPowerOff,
	SerPowerOn,
	SL_ClearDTR,
	SL_SetDTR,
	SL_ClearRTS,
	SL_SetRTS,
	SerEnableSerial,
	SerDisableSerial,
	SL_ClearBreak,
	SL_SetBreak,
	SL_XmitComChar,
	SL_GetStatus,
	SL_Reset,
	SL_GetModemStatus,
	SerGetCommProperties,
	SL_PurgeComm,
	SL_SetDCB,
	SL_SetCommTimeouts,
	};
extern const HW_VTBL SerCardIoVTbl;

const
HW_VTBL IrVTbl = {
	SerInitIR,
	SL_PostInit,
	SerDeinit,
	SerOpen,
	SerClose,
	SL_GetInterruptType,
	SL_RxIntr,
	SL_TxIntrEx,
	SL_ModemIntr,
	SL_LineIntr,
	SL_GetRxBufferSize,
	SerPowerOff,
	SerPowerOn,
	SL_ClearDTR,
	SL_SetDTR,
	SL_ClearRTS,
	SL_SetRTS,
	SerEnableIR,
	SerDisableIR,
	SL_ClearBreak,
	SL_SetBreak,
	SL_XmitComChar,
	SL_GetStatus,
	SL_Reset,
	SL_GetModemStatus,
	SerGetCommProperties,
	SL_PurgeComm,
	SL_SetDCB,
	SL_SetCommTimeouts,
	};
extern const HW_VTBL SerCardIrVTbl;

const HWOBJ IoObj = {
	THREAD_AT_INIT,
	0,
	(PHW_VTBL) &IoVTbl
};

const HWOBJ IrObj = {
	THREAD_AT_INIT,
	0,
	(PHW_VTBL) &IrVTbl
};

typedef HWOBJ const *PCHWOBJ;

const PCHWOBJ HWObjects[] = {
	&IoObj,
	&IrObj
};

// GetSerialObj : The purpose of this function is to allow multiple PDDs to be
// linked with a single MDD creating a multiport driver.  In such a driver, the
// MDD must be able to determine the correct vtbl and associated parameters for
// each PDD.  Immediately prior to calling HWInit, the MDD calls GetSerialObject
// to get the correct function pointers and parameters.
//
PHWOBJ
GetSerialObject(
               DWORD DeviceArrayIndex
               )
{
    PHWOBJ pSerObj;

    RETAILMSG(DEBUGMODE,(TEXT("GetSerialObject : DeviceArrayIndex = %d\r\n"), DeviceArrayIndex));

    // Now return this structure to the MDD.
    if ( DeviceArrayIndex == 1 )
        pSerObj = (PHWOBJ)(&IrObj);
    else
        pSerObj = (PHWOBJ)(&IoObj);

    return pSerObj;
}

