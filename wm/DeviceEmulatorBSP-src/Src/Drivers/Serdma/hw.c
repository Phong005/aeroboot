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
        File:           hw.c

        Contains:       Serial-over-DMA driver.

==================================================================*/

#include <windows.h>
#include <serhw.h>
#include <ser16550.h>
#include <windev.h>
#include <notify.h>
#include "serdma.h"

#undef ZONE_INIT
#include <serdbg.h>

#define DEBUGMODE 1

//
/////////////////// Start of exported entrypoints ////////////////
//
//
// @doc OEM 
// @func PVOID | SL_Open | Configures 16550 for default behaviour.
//
VOID
SL_Open(
       PVOID   pHead // @parm PVOID returned by HWinit.
       )
{
	PSERDMA_UART_INFO pHWHead = (PSERDMA_UART_INFO)pHead;

	RETAILMSG(DEBUGMODE, (TEXT("SL_Open 0x%X (%d opens)\r\n"), pHead, pHWHead->OpenCount));
	pHWHead->OpenCount++;
}

//
// @doc OEM 
// @func PVOID | SL_Close | Does nothing except keep track of the
// open count so that other routines know what to do.
//
VOID
SL_Close(
        PVOID   pHead // @parm PVOID returned by HWinit.
        )
{
	PSERDMA_UART_INFO pHWHead = (PSERDMA_UART_INFO)pHead;

	RETAILMSG(DEBUGMODE, (TEXT("SL_Close \r\n")));

	if ( pHWHead->OpenCount )	pHWHead->OpenCount--;
}

//
// @doc OEM 
// @func PVOID | SL_Init | Initializes 16550 device head.  
//

VOID
SL_Init(
       PVOID   pHead, // @parm points to device head
       PUCHAR  pRegBase, // Pointer to 16550 register base (actually used to pass the DMAChannelNumber for SERDMA)
       UINT8   RegStride, // Stride amongst the 16550 registers (always 0 for SERDMA)
       EVENT_FUNC EventCallback, // This callback exists in MDD
       PVOID   pMddHead,   // This is the first parm to callback
       PLOOKUP_TBL   pBaudTable  // BaudRate Table (always NULL for SERDMA)
       )
{
    PSERDMA_UART_INFO   pHWHead   = (PSERDMA_UART_INFO)pHead;

    RETAILMSG(DEBUGMODE, (TEXT("SL_Init\r\n")));
    
    // Store info for callback function
    pHWHead->EventCallback = EventCallback;
    pHWHead->pMddHead = pMddHead;
        
    // Initialize internal members
    pHWHead->DMAChannelNumber = (DWORD)pRegBase;
    pHWHead->DSRIsHigh = 0;
    pHWHead->OpenCount = 0;
}

//
// @doc OEM
// @func void | SL_PostInit | This routine takes care of final initialization.
//
// @rdesc None.
//
BOOL
SL_PostInit(
           PVOID   pHead // @parm PVOID returned by HWinit.
           )
{
    PSERDMA_UART_INFO   pHWHead   = (PSERDMA_UART_INFO)pHead;
    WCHAR DMADeviceName[6]; // "DMAx"

    RETAILMSG(DEBUGMODE, (TEXT("+SL_PostInit \r\n")));

    wcscpy(DMADeviceName, L"DMA0:");
    DMADeviceName[3] = L'0'+(WCHAR)pHWHead->DMAChannelNumber; // DMAChannelNumber has already been validated to be 0...9
    pHWHead->hDMAChannel = CreateFile(DMADeviceName, 
                                      GENERIC_READ|GENERIC_WRITE,
                                      0,
                                      NULL,
                                      OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL,
                                      NULL);
    if (pHWHead->hDMAChannel == INVALID_HANDLE_VALUE) {
        DEBUGMSG (ZONE_INIT | ZONE_ERROR,
                  (TEXT("Couldn't open DMA device %s with error %d, SL_PostInit failed\r\n"), DMADeviceName, GetLastError()));
        return FALSE;
    }

    RETAILMSG(DEBUGMODE, (TEXT("-SL_PostInit \r\n")));
    return TRUE;
}

//
// @doc OEM 
// @func PVOID | SL_Deinit | De-initializes 16550 device head.  
//
VOID
SL_Deinit(
         PVOID   pHead // @parm points to device head
         )
{
    PSERDMA_UART_INFO   pHWHead   = (PSERDMA_UART_INFO)pHead;

    RETAILMSG(DEBUGMODE, (TEXT("SL_Deinit \r\n")));

    if (pHWHead->hDMAChannel && pHWHead->hDMAChannel != INVALID_HANDLE_VALUE) {
        CloseHandle(pHWHead->hDMAChannel);
    }
    pHWHead->hDMAChannel = NULL;
}

//
// @doc OEM
// @func void | SL_ClearDtr | This routine clears DTR.
//
// @rdesc None.
//
VOID
SL_ClearDTR(
           PVOID   pHead // @parm PVOID returned by HWinit.
           )
{
    RETAILMSG(DEBUGMODE, (TEXT("SL_ClearDTR, 0x%X\r\n"), pHead));
    ASSERT(FALSE); // this should not be called for SERDMA
}

//
// @doc OEM
// @func VOID | SL_SetDTR | This routine sets DTR.
// 
// @rdesc None.
//
VOID
SL_SetDTR(
         PVOID   pHead // @parm PVOID returned by HWinit.
         )
{    
    RETAILMSG(DEBUGMODE, (TEXT("SL_SetDTR, 0x%X\r\n"), pHead));
    ASSERT(FALSE); // this should not be called for SERDMA
}

//
// @doc OEM
// @func VOID | SL_ClearRTS | This routine clears RTS.
// 
// @rdesc None.
// 
VOID
SL_ClearRTS(
           PVOID   pHead // @parm PVOID returned by HWinit.
           )
{
    RETAILMSG(DEBUGMODE, (TEXT("SL_ClearRTS, 0x%X\r\n"), pHead));
    ASSERT(FALSE); // this should not be called for SERDMA
}

//
// @doc OEM
// @func VOID | SL_SetRTS | This routine sets RTS.
// 
// @rdesc None.
//
VOID
SL_SetRTS(
         PVOID   pHead // @parm PVOID returned by HWinit.
         )
{
    RETAILMSG(DEBUGMODE, (TEXT("SL_SetRTS, 0x%X\r\n"), pHead));
    ASSERT(FALSE); // this should not be called for SERDMA
}

//
// @doc OEM
// @func VOID | SL_ClearBreak | This routine clears break.
// 
// @rdesc None.
// 
VOID
SL_ClearBreak(
             PVOID   pHead // @parm PVOID returned by HWinit.
             )
{
    RETAILMSG(DEBUGMODE,  (TEXT("SL_ClearBreak:\r\n")));
    ASSERT(FALSE); // not called for SERDMA
}

//
// @doc OEM
// @func VOID | SL_SetBreak | This routine sets break.
// 
// @rdesc None.
//
VOID
SL_SetBreak(
           PVOID   pHead // @parm PVOID returned by HWinit.
           )
{
    RETAILMSG(DEBUGMODE, (TEXT("SL_SetBreak, 0x%X\r\n"), pHead));
    ASSERT(FALSE); // not called for SERDMA
}

//
// @doc OEM
// @func BOOL | SL_SetBaudRate |
//  This routine sets the baud rate of the device.
//
// @rdesc None.
//
BOOL
SL_SetBaudRate(
              PVOID   pHead,    // @parm     PVOID returned by HWInit
              ULONG   BaudRate    // @parm     ULONG representing decimal baud rate.
              )
{
    RETAILMSG (DEBUGMODE, (TEXT("SL_SetbaudRate 0x%X, %d\r\n"), pHead, BaudRate));
    // This is a no-op for SERDMA
    return TRUE;
}

//
// @doc OEM
// @func BOOL | SL_SetByteSize |
//  This routine sets the WordSize of the device.
//
// @rdesc None.
//
BOOL
SL_SetByteSize(
              PVOID   pHead,        // @parm     PVOID returned by HWInit
              ULONG   ByteSize    // @parm     ULONG ByteSize field from DCB.
              )
{
    RETAILMSG(DEBUGMODE,(TEXT("SL_SetByteSize 0x%X, 0x%X\r\n"), pHead, ByteSize));
    // This is a no-op for SERDMA
    return TRUE;
}

//
// @doc OEM
// @func BOOL | SL_SetParity |
//  This routine sets the parity of the device.
//
// @rdesc None.
//
BOOL
SL_SetParity(
            PVOID   pHead,    // @parm     PVOID returned by HWInit
            ULONG   Parity    // @parm     ULONG parity field from DCB.
            )
{
    RETAILMSG(DEBUGMODE,(TEXT("SL_SetParity 0x%X, 0x%X\r\n"), pHead, Parity));
    // This is a no-op for SERDMA
    return TRUE;
}
//
// @doc OEM
// @func VOID | SL_SetStopBits |
//  This routine sets the Stop Bits for the device.
//
// @rdesc None.
//
BOOL
SL_SetStopBits(
              PVOID   pHead,      // @parm     PVOID returned by HWInit
              ULONG   StopBits  // @parm     ULONG StopBits field from DCB.
              )
{
    RETAILMSG (DEBUGMODE,(TEXT("SL_SetStopBits 0x%X, 0x%X\r\n"), pHead, StopBits));
    // This is a no-op for SERDMA
    return TRUE;
}

//
// @doc OEM
// @func ULONG | SL_GetRxBufferSize | This function returns
// the size of the hardware buffer passed to the interrupt
// initialize function.  It would be used only for devices
// which share a buffer between the MDD/PDD and an ISR.
//
// 
// @rdesc This routine always returns 0 for 16550 UARTS.
// 
ULONG
SL_GetRxBufferSize(
                  PVOID pHead
                  )
{
    RETAILMSG(DEBUGMODE, (TEXT("SL_GetRxBufferSize \r\n")));
    // This is a no-op for SERDMA
    return 0;
}

//
// @doc OEM
// @func ULONG | SL_GetInterruptType | This function is called
//   by the MDD whenever an interrupt occurs.  The return code
//   is then checked by the MDD to determine which of the four
//   interrupt handling routines are to be called.
// 
// @rdesc This routine returns a bitmask indicating which interrupts
//   are currently pending.
// 
INTERRUPT_TYPE
SL_GetInterruptType(
                   PVOID pHead      // Pointer to hardware head
                   )
{
    RETAILMSG(DEBUGMODE, (TEXT("+SL_GetInterruptType \r\n")));

    ASSERT(FALSE); // This should never be called for SERDMA, as there are no interrupts involved

    RETAILMSG(DEBUGMODE, (TEXT("-SL_GetInterruptType \r\n")));
    return INTR_NONE;
}

// @doc OEM
// @func ULONG | SL_RxIntr | This routine gets several characters from the hardware
//   receive buffer and puts them in a buffer provided via the second argument.
//   It returns the number of bytes lost to overrun.
// 
// @rdesc The return value indicates the number of overruns detected.
//   The actual number of dropped characters may be higher.
//
ULONG
SL_RxIntr(
         PVOID pHead,                // @parm Pointer to hardware head
         PUCHAR pRxBuffer,           // @parm Pointer to receive buffer
         ULONG *pBufflen             // @parm In = max bytes to read, out = bytes read
         )
{
    PSERDMA_UART_INFO pHWHead = (PSERDMA_UART_INFO)pHead;
    ULONG cbRead;
    BOOL bRet;

    RETAILMSG(DEBUGMODE, (TEXT("SL_RxIntr \r\n")));

    bRet = ReadFile(pHWHead->hDMAChannel,
                    pRxBuffer,
                    *pBufflen,
                    &cbRead,
                    NULL);
    if (!bRet) {
        ASSERT(FALSE);
        cbRead=(ULONG)-1; // the caller treats this value as an error;
    }

    *pBufflen = cbRead;

    return 0;
}

// @doc OEM
// @func ULONG | SL_PutBytes | This routine is called from the MDD
//   in order to write a stream of data to the device. (Obsolete)
// 
// @rdesc Always returns 0
//
ULONG
SL_PutBytes(
           PVOID   pHead,        // @parm    PVOID returned by HWInit.
           PUCHAR  pSrc,        // @parm    Pointer to bytes to be sent.
           ULONG   NumberOfBytes,  // @parm    Number of bytes to be sent.
           PULONG  pBytesSent        // @parm    Pointer to actual number of bytes put.
           )
{
	RETAILMSG(1,(TEXT("This routine is called by old MDD\r\n")));
        ASSERT(FALSE);
	return 0;
}

//
// @doc OEM
// @func ULONG | SL_TXIntr | This routine is called from the old MDD
//   whenever INTR_TX is returned by SL_GetInterruptType (Obsolete)
// 
// @rdesc None
//
VOID
SL_TxIntr(
         PVOID pHead                // Hardware Head
         )
{
    RETAILMSG(1, (TEXT("SL_TxIntr(From old MDD)\n")));
    ASSERT(FALSE);
}

//
// @doc OEM
// @func ULONG | SL_TXIntrEx | This routine is called from the new MDD
//   whenever INTR_TX is returned by SL_GetInterruptType
// 
// @rdesc None
//
VOID
SL_TxIntrEx(
           PVOID pHead,                // Hardware Head
           PUCHAR pTxBuffer,          // @parm Pointer to receive buffer
           ULONG *pBufflen            // @parm In = max bytes to transmit, out = bytes transmitted
           )
{
    PSERDMA_UART_INFO pHWHead = (PSERDMA_UART_INFO)pHead;
    ULONG cbWritten;
    BOOL bRet;

    RETAILMSG(DEBUGMODE, (TEXT("+SL_TxIntrEx \r\n")));

    bRet = WriteFile(pHWHead->hDMAChannel,
                     pTxBuffer,
                     *pBufflen,
                     &cbWritten,
                     NULL);
    if (!bRet) {
        ASSERT(FALSE);
        *pBufflen = (ULONG)-1; // the caller treats this value as an error
    }
    *pBufflen = cbWritten;

    RETAILMSG(DEBUGMODE, (TEXT("-SL_TxIntrEx \r\n")));
}

//
// @doc OEM
// @func ULONG | SL_LineIntr | This routine is called from the MDD
//   whenever INTR_LINE is returned by SL_GetInterruptType.
// 
// @rdesc None
//
VOID
SL_LineIntr(
           PVOID pHead                // Hardware Head
           )
{
    RETAILMSG(DEBUGMODE,(TEXT("INTR_LINE \r\n")));
}

//
// @doc OEM
// @func ULONG | SL_OtherIntr | This routine is called from the MDD
//   whenever INTR_MODEM is returned by SL_GetInterruptType.
// 
// @rdesc None
//
VOID
SL_OtherIntr(
            PVOID pHead                // Hardware Head
            )
{
    RETAILMSG(DEBUGMODE,(TEXT("SL_OtherIntr \r\n")));
}

//
// @doc OEM
// @func ULONG | SL_OtherIntr | This routine is called from the MDD
//   whenever INTR_MODEM is returned by SL_GetInterruptType.
//
//   For SERDMA, it is called at the end of SERDMA_Init()
// 
// @rdesc None
//
VOID
SL_ModemIntr(
            PVOID pHead                // Hardware Head
            )
{
    PSERDMA_UART_INFO pHWHead = (PSERDMA_UART_INFO)pHead;

    RETAILMSG (DEBUGMODE, (TEXT("+SL_ModemIntr\r\n")));

    if ( !pHWHead->OpenCount && (pHWHead->ControlFlags & 1)) {
        // We want to indicate a cable event.
        RETAILMSG (DEBUGMODE, (TEXT("Indicating RS232 Cable Event\r\n")));

        if ( IsAPIReady(SH_WMGR) ) {
            CeEventHasOccurred (NOTIFICATION_EVENT_RS232_DETECTED,NULL);
            pHWHead->DSRIsHigh = 1;
        }
    }
    else if (pHWHead->ControlFlags & 0x2) {
        // We want to disconnect
        RETAILMSG (DEBUGMODE, (TEXT("Disconnecting on request from host\r\n")));
        pHWHead->DSRIsHigh = 0;
        pHWHead->EventCallback(pHWHead->pMddHead, EV_DSR|EV_RLSD);
    }

    RETAILMSG (DEBUGMODE, (TEXT("-SL_ModemIntr\r\n")));
}

//  
// @doc OEM
// @func    ULONG | SL_GetStatus | This structure is called by the MDD
//   to retrieve the contents of a COMSTAT structure.
//
// @rdesc    The return is a ULONG, representing success (0) or failure (-1).
//
ULONG
SL_GetStatus(
            PVOID    pHead,    // @parm PVOID returned by HWInit.
            LPCOMSTAT    lpStat    // Pointer to LPCOMMSTAT to hold status.
            )
{
    RETAILMSG(DEBUGMODE, (TEXT("+SL_GetStatus 0x%X\r\n"), pHead));

    memset(lpStat, 0, sizeof(*lpStat)); // clear all status

    RETAILMSG(DEBUGMODE, (TEXT("-SL_GetStatus 0x%X\r\n"), pHead));
    return 0;
}

//
// @doc OEM
// @func    ULONG | SL_Reset | Perform any operations associated
//   with a device reset
//
// @rdesc    None.
//
VOID
SL_Reset(
        PVOID   pHead    // @parm PVOID returned by HWInit.
        )
{
    RETAILMSG(DEBUGMODE,(TEXT("SL_Reset 0x%X\r\n"), pHead));

    // A no-op for SERDMA
}

//
// @doc OEM
// @func    VOID | SL_GetModemStatus | Retrieves modem status.
//
// @rdesc    None.
//
VOID
SL_GetModemStatus(
                 PVOID   pHead,        // @parm PVOID returned by HWInit.
                 PULONG  pModemStatus    // @parm PULONG passed in by user.
                 )
{
    PSERDMA_UART_INFO pHWHead = (PSERDMA_UART_INFO)pHead;

    RETAILMSG(DEBUGMODE,  (TEXT("SL_GetModemStatus:\r\n")));

    *pModemStatus |= MS_CTS_ON;
    *pModemStatus |= pHWHead->DSRIsHigh ? MS_DSR_ON : 0;
    *pModemStatus |= pHWHead->DSRIsHigh ? MS_RLSD_ON : 0;
}

//
// @doc OEM
// @func    VOID | SL_PurgeComm | Purge RX and/or TX
// 
// @rdesc    None.
//

VOID
SL_PurgeComm(
            PVOID   pHead,        // @parm PVOID returned by HWInit.
            DWORD   fdwAction        // @parm Action to take. 
            )
{
    RETAILMSG(DEBUGMODE,(TEXT("SL_PurgeComm 0x%X\r\n"), fdwAction));
}

//
// @doc OEM
// @func    BOOL | SL_XmitComChar | Transmit a char immediately
// 
// @rdesc    TRUE if succesful
//
BOOL
SL_XmitComChar(
              PVOID   pHead,    // @parm PVOID returned by HWInit.
              UCHAR   ComChar   // @parm Character to transmit. 
              )
{
	RETAILMSG (DEBUGMODE,(TEXT("SL_XmitComChar 0x%X\r\n"), pHead));
        ASSERT(FALSE); // this is never called for SERDMA
        return FALSE;
}

//
// @doc OEM
// @func    BOOL | SL_PowerOff | Perform powerdown sequence.
// 
// @rdesc    TRUE if succesful
//
VOID
SL_PowerOff(
           PVOID   pHead        // @parm    PVOID returned by HWInit.
           )
{
    RETAILMSG(DEBUGMODE, (TEXT("SL_PowerOff \r\n")));
    // This is a no-op for SERDMA
}

//
// @doc OEM
// @func    BOOL | SL_PowerOn | Perform poweron sequence.
// 
// @rdesc    TRUE if succesful
//
VOID
SL_PowerOn(
          PVOID   pHead        // @parm    PVOID returned by HWInit.
          )
{
    // Restore any registers that we need
    RETAILMSG(DEBUGMODE, (TEXT("SL_PowerOn \r\n")));
    // This is a no-op for SERDMA
}

//
// @doc OEM
// @func    BOOL | SL_SetDCB | Sets new values for DCB.  This
// routine gets a DCB from the MDD.  It must then compare
// this to the current DCB, and if any fields have changed take
// appropriate action.
// 
// @rdesc    BOOL
//
BOOL
SL_SetDCB(
         PVOID   pHead,        // @parm    PVOID returned by HWInit.
         LPDCB   lpDCB       // @parm    Pointer to DCB structure
         )
{
    PSERDMA_UART_INFO pHWHead = (PSERDMA_UART_INFO)pHead;

    RETAILMSG(DEBUGMODE,(TEXT("SL_SetDCB\r\n")));
    pHWHead->dcb = *lpDCB;
    return TRUE;
}

//
// @doc OEM
// @func    BOOL | SL_SetCommTimeouts | Sets new values for the
// CommTimeouts structure. routine gets a DCB from the MDD.  It
// must then compare this to the current DCB, and if any fields
// have changed take appropriate action.
// 
// @rdesc    ULONG
//
ULONG
SL_SetCommTimeouts(
                  PVOID   pHead,        // @parm    PVOID returned by HWInit.
                  LPCOMMTIMEOUTS   lpCommTimeouts // @parm Pointer to CommTimeout structure
                  )
{
    PSERDMA_UART_INFO pHWHead = (PSERDMA_UART_INFO)pHead;

    RETAILMSG(DEBUGMODE,(TEXT("SL_SetCommTimeout 0x%X\r\n"), pHead));
    pHWHead->CommTimeouts = *lpCommTimeouts;
    return 0;
}

//
//  @doc OEM
//  @func    BOOL | SL_Ioctl | Device IO control routine.  
//  @parm DWORD | dwOpenData | value returned from COM_Open call
//    @parm DWORD | dwCode | io control code to be performed
//    @parm PBYTE | pBufIn | input data to the device
//    @parm DWORD | dwLenIn | number of bytes being passed in
//    @parm PBYTE | pBufOut | output data from the device
//    @parm DWORD | dwLenOut |maximum number of bytes to receive from device
//    @parm PDWORD | pdwActualOut | actual number of bytes received from device
//
//    @rdesc        Returns TRUE for success, FALSE for failure
//
//  @remark  The MDD will pass any unrecognized IOCTLs through to this function.
//
BOOL
SL_Ioctl(PVOID pHead, DWORD dwCode,PBYTE pBufIn,DWORD dwLenIn,
         PBYTE pBufOut,DWORD dwLenOut,PDWORD pdwActualOut)
{
	BOOL RetVal = TRUE;
	RETAILMSG(DEBUGMODE, (TEXT("+SL_Ioctl 0x%X\r\n"), pHead));
	switch (dwCode) {
	// Currently, no defined IOCTLs
	default:
		RetVal = FALSE;
		DEBUGMSG (ZONE_FUNCTION, (TEXT(" Unsupported ioctl 0x%X\r\n"), dwCode));
	break;            
	}
	RETAILMSG(DEBUGMODE, (TEXT("-SL_Ioctl 0x%X\r\n"), pHead));
	return(RetVal);
}


