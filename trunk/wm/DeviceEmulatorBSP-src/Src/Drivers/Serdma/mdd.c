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
        File:           mdd.c

        Contains:       Serial-over-DMA driver.

==================================================================*/

#include <windows.h>
#include <linklist.h>
#include <serhw.h>
#include <ser16550.h>
#include <pegdser.h>
#include <pm.h>
#include "serdma.h"

#undef ZONE_INIT
#include <serdbg.h>


/* Debug Zones.
 */
#ifdef DEBUG

    #define DBG_INIT    0x0001
    #define DBG_OPEN    0x0002
    #define DBG_READ    0x0004
    #define DBG_WRITE   0x0008
    #define DBG_CLOSE   0x0010
    #define DBG_IOCTL   0x0020
    #define DBG_THREAD  0x0040
    #define DBG_EVENTS  0x0080
    #define DBG_CRITSEC 0x0100
    #define DBG_FLOW    0x0200
    #define DBG_IR      0x0400
    #define DBG_NOTHING 0x0800
    #define DBG_ALLOC   0x1000
    #define DBG_FUNCTION 0x2000
    #define DBG_WARNING 0x4000
    #define DBG_ERROR   0x8000

DBGPARAM dpCurSettings = {
    TEXT("SerDMA"), {
        TEXT("Init"),TEXT("Open"),TEXT("Read"),TEXT("Write"),
        TEXT("Close"),TEXT("Ioctl"),TEXT("Thread"),TEXT("Events"),
        TEXT("CritSec"),TEXT("FlowCtrl"),TEXT("Infrared"),TEXT("User Read"),
        TEXT("Alloc"),TEXT("Function"),TEXT("Warning"),TEXT("Error")},
    0
};
#endif


// SERDMA_Write uses this buffer to store data to be transmitted. Since multiple threads
// can call into it at the same time, we will provide one global buffer and if it
// is already in use, that 2nd (or 3rd...) thread will allocate a temporary buffer.
DMA_PACKET_BUFFER g_PacketBuffer;
volatile DMA_PACKET_BUFFER *g_pPacketBufferCache = &g_PacketBuffer;


//
// Macros to maintain a usage count for a particular serial structure,
// so that we know when it is safe to deallocate it.
//
#define COM_INC_USAGE_CNT(pOpenHead)  \
    InterlockedIncrement(&pOpenHead->StructUsers)

#define COM_DEC_USAGE_CNT(pOpenHead)  \
    InterlockedDecrement(&pOpenHead->StructUsers)

BOOL
SERDMA_Deinit(PHW_INDEP_INFO pSerialHead);


BOOL
DllEntry(
              HINSTANCE   hinstDll,
              DWORD   dwReason,
              LPVOID  lpReserved
              )
{
    if ( dwReason == DLL_PROCESS_ATTACH ) {
        DEBUGREGISTER(hinstDll);
        DEBUGMSG (ZONE_INIT, (TEXT("serial port process attach\r\n")));
        DisableThreadLibraryCalls((HMODULE) hinstDll);
    }

    if ( dwReason == DLL_PROCESS_DETACH ) {
        DEBUGMSG (ZONE_INIT, (TEXT("process detach called\r\n")));
    }

    return TRUE;
}

/*
 @doc INTERNAL
 @func  ULONG | SerialDispatchThread | Main serial event dispatch thread code.
 *      This is the reading and dispatching thread. It gets the
 *      event associated with the logical interrupt dwIntID and calls
 *      hardware specific routines to determine whether it's a receive event
 *      or a transmit event. If it's a transmit event, it calls the HW tx handler.
 *      If it's a receive event, it calls for the number of characters and calls
 *      atomic GetByte() to extract characters and put them into the drivers
 *      buffer represented by pSerialHead->pTargetBuffer, managing waiting
 *      for events and checking to see if those signals correspond to reading.
 *      It relies on NK masking the interrupts while it does it's thing, calling
 *      InterruptDone() to unmask them for each of the above cases.
 *
 *      Not exported to users.
 *
 @rdesc This thread technically returns a status, but in practice, doesn't return
 *              while the device is open.
 */
static DWORD WINAPI
SerialDispatchThread(
                    PVOID   pContext    /* @parm [IN] Pointer to main data structure. */
                    )
{
    PHW_INDEP_INFO      pSerialHead    = (PHW_INDEP_INFO)pContext;
    PHW_VTBL            pFuncTbl = pSerialHead->pHWObj->pFuncTbl;
    PVOID               pHWHead = pSerialHead->pHWHead;
    ULONG               RoomLeft;

    DEBUGMSG (ZONE_THREAD, (TEXT("Entered SerialDispatchThread %X\r\n"),
                            pSerialHead));

    // It is possible for a PDD to use this routine in its private thread, so
    // don't just assume that the MDD synchronization mechanism is in place.
    if ( pSerialHead->pHWObj->BindFlags & THREAD_IN_MDD ) {
        DEBUGMSG(ZONE_INIT,
                 (TEXT("Spinning in dispatch thread %X %X\n\r"), pSerialHead, pSerialHead->pHWObj));
        while ( !pSerialHead->pDispatchThread ) {
            Sleep(20);
        }
    }

    /* Wait for the event that any serial port action creates.
     */
    while ( !pSerialHead->KillRxThread ) {
        DWORD dw;

        // WinCE doesn't support WaitForMultipleObjects() with fWaitAll==TRUE.  So
        // Wait on each HANDLE individually.
        dw = WaitForSingleObject(pSerialHead->hReceiveBufferEmpty, INFINITE); // wait for the buffer to receive into to become available
        if (dw != WAIT_OBJECT_0) {
            DEBUGMSG (ZONE_FUNCTION|ZONE_ERROR,
                                (TEXT("Dispatch thread failed (1) - dw=%x, gle=%d\n"), dw, GetLastError()));
            DEBUGMSG (ZONE_THREAD, (TEXT("Exiting thread after error\r\n")));
            SetEvent(pSerialHead->hKillDispatchThread);
            return 0;
        }
        dw = WaitForSingleObject(pSerialHead->hReceiveDataReady, INFINITE); // wait for the DMA device to indicate that a packet is ready
        if (dw != WAIT_OBJECT_0) {
            DEBUGMSG (ZONE_FUNCTION|ZONE_ERROR,
                                (TEXT("Dispatch thread failed (2) - dw=%x, gle=%d\n"), dw, GetLastError()));
            DEBUGMSG (ZONE_THREAD, (TEXT("Exiting thread after error\r\n")));
            SetEvent(pSerialHead->hKillDispatchThread);
            return 0;
        }

        // The remainder of this while() loop is taken from SerialEventHandler() in the original SERIAL device
        if ( pSerialHead->KillRxThread ||
            !pSerialHead->hReceiveDataReady ) {
            DEBUGMSG (ZONE_THREAD, (TEXT("Exiting thread\r\n")));
            SetEvent(pSerialHead->hKillDispatchThread);
            return 0;
        }

        // NOTE - This one is a little tricky.  If the only owner is a monitoring task
        // then I don't have an owner for read/write, yet I might be in this routine
        // due to a change in line status.  Lets just do the best we can and increment
        // the count for the access owner if available.
        if ( pSerialHead->pAccessOwner )
            COM_INC_USAGE_CNT(pSerialHead->pAccessOwner);

        // Receive the packet from DMA into our ReceivePacketBuffer
        RoomLeft = sizeof(pSerialHead->ReceivePacketBuffer);
        pFuncTbl->HWRxIntrHandler(pHWHead,
                                  (PUCHAR)&pSerialHead->ReceivePacketBuffer,
                                  &RoomLeft);

        if (pSerialHead->ReceivePacketBuffer.u.PacketLength == 0xffff) {
            // Simulate a modem interrupt, which triggers an OS cable event if
            // no app currently has the driver open.
    	    ((PSERDMA_UART_INFO)pHWHead)->ControlFlags = 
                pSerialHead->ReceivePacketBuffer.u.PacketContents[0];
            DEBUGMSG (ZONE_EVENTS, (TEXT("SerialDispatchThread: ControlFlags = 0x%x\r\n"),
                ((PSERDMA_UART_INFO)pHWHead)->ControlFlags));
            pFuncTbl->HWModemIntrHandler(pHWHead);
        } else {
             // Indicate that data is available
            pSerialHead->RxDataReady = 1;
            SetEvent(pSerialHead->hReadEvent);
            EvaluateEventFlag(pSerialHead, EV_RXCHAR);
        }
    }

    DEBUGMSG (ZONE_THREAD, (TEXT("SerialDispatchThread %x exiting\r\n"),
                            pSerialHead));
    return 0;
}


// ****************************************************************
//
//      @doc INTERNAL
//      @func           BOOL | StartDispatchThread | Start thread if requested by PDD.
//
//      @parm           ULONG  | pSerialHead
//
//       @rdesc         TRUE if success, FALSE if failed.
//
BOOL
StartDispatchThread(
                   PHW_INDEP_INFO  pSerialHead
                   )
{
    DEBUGMSG(ZONE_INIT,
             (TEXT("Spinning thread %X\n\r"), pSerialHead));

    pSerialHead->pDispatchThread = CreateThread(NULL,0, SerialDispatchThread,
                                                pSerialHead, 0,NULL);
    if ( pSerialHead->pDispatchThread == NULL ) {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR,
                 (TEXT("Error creating dispatch thread (%d)\n\r"),
                  GetLastError()));
        return FALSE;
    }

    return TRUE;
}

// ****************************************************************
//
//      @doc INTERNAL
//      @func           BOOL | StartDispatchThread | Stop thread, disable interrupt.
//
//      @parm           ULONG  | pSerialHead
//
//       @rdesc         TRUE if success, FALSE if failed.
//
BOOL
StopDispatchThread(
                  PHW_INDEP_INFO  pSerialHead
                  )

{
    HANDLE              pThisThread = GetCurrentThread();
    ULONG               priority256;

    /* If we have an interrupt handler thread, kill it */
    if ( pSerialHead->pDispatchThread ) {
        DEBUGMSG (ZONE_INIT, (TEXT("\r\nTrying to close dispatch thread\r\n")));

        /* Set the priority of the dispatch thread to be equal to this one,
         * so that it shuts down before we free its memory. If this routine
         * has been called from SerialDllEntry then RxCharBuffer is set to
         * NULL and the dispatch thread is already dead, so just skip the
         * code which kills the thread.
         */
        priority256 = CeGetThreadPriority(pThisThread);
        CeSetThreadPriority(pSerialHead->pDispatchThread, priority256);

        /* Signal the Dispatch thread to die.
         */
        pSerialHead->KillRxThread = 1;
        SetEvent(pSerialHead->hReceiveBufferEmpty);
        DEBUGMSG (ZONE_INIT, (TEXT("\r\nTrying to signal serial thread.\r\n")));
        SetEvent(pSerialHead->hReceiveDataReady);

        WaitForSingleObject(pSerialHead->hKillDispatchThread, 3000);
        Sleep(10);

        DEBUGMSG (ZONE_INIT, (TEXT("\r\nTrying to call CloseHandle\r\n")));

        CloseHandle(pSerialHead->pDispatchThread);
        pSerialHead->pDispatchThread = NULL;
        DEBUGMSG (ZONE_INIT, (TEXT("\r\nReturned from CloseHandle\r\n")));
    }

    return TRUE;
}


// ****************************************************************
//
//      @doc EXTERNAL
//      @func           HANDLE | SERDMA_Init | Serial device initialization.
//
//      @parm           ULONG  | Identifier | Port identifier.  The device loader
//                              passes in the registry key that contains information
//                              about the active device.
//
//      @remark         This routine is called at device load time in order
//                              to perform any initialization.   Typically the init
//                              routine does as little as possible, postponing memory
//                              allocation and device power-on to Open time.
//
//       @rdesc         Returns a pointer to the serial head which is passed into
//                              the SERDMA_Open and SERDMA_Deinit entry points as a device handle.
//
HANDLE
SERDMA_Init(
        ULONG   Identifier
        )
{
    PVOID           pHWHead     = NULL;
    PHW_INDEP_INFO  pSerialHead = NULL;
    DWORD           DevIndex;
    HKEY            hKey;
    ULONG           datasize;
    DWORD           DMAChannelNumber;
    WCHAR           ChannelEventName[9]; // "channelX"

    DEBUGMSG (ZONE_INIT | ZONE_FUNCTION, (TEXT("+SERDMA_Init\r\n")));

    // Allocate our control structure.
    pSerialHead  =  (PHW_INDEP_INFO)LocalAlloc(LPTR, sizeof(HW_INDEP_INFO));

    if ( !pSerialHead ) {
        DEBUGMSG(ZONE_INIT | ZONE_ERROR,
                 (TEXT("Error allocating memory for pSerialHead, SERDMA_Init failed\n\r")));
        return NULL;
    }

    // Initially, open list is empty.
    InitializeListHead( &pSerialHead->OpenList );
    InitializeCriticalSection(&(pSerialHead->OpenCS));
    pSerialHead->pAccessOwner = NULL;
    pSerialHead->fEventMask = 0;

    /* Initialize the critical sections that will guard the parts of
     * the receive and transmit buffers.
     */
    InitializeCriticalSection(&(pSerialHead->ReceiveCritSec1));

    /* Want to use the Identifier to do RegOpenKey and RegQueryValue (?)
     * to get the index to be passed to GetHWObj.
     * The HWObj will also have a flag denoting whether to start the
     * listening thread or provide the callback.
     */
    DEBUGMSG (ZONE_INIT,(TEXT("Try to open %s\r\n"), (LPCTSTR)Identifier));
    hKey = OpenDeviceKey((LPCTSTR)Identifier);
    if ( !hKey ) {
        DEBUGMSG (ZONE_INIT | ZONE_ERROR,
                  (TEXT("Failed to open devkeypath, SERDMA_Init failed\r\n")));
        LocalFree(pSerialHead);
        return(NULL);
    }

    datasize = sizeof(DWORD);

    if ( RegQueryValueEx(hKey, L"DeviceArrayIndex", NULL, NULL,
                         (LPBYTE)&DevIndex, &datasize) ) {
        DEBUGMSG (ZONE_INIT | ZONE_ERROR,
                  (TEXT("Failed to get DeviceArrayIndex value, SERDMA_Init failed\r\n")));
        RegCloseKey (hKey);
        LocalFree(pSerialHead);
        return(NULL);
    }

    datasize = sizeof(DWORD);
    if ( RegQueryValueEx(hKey, L"Priority256", NULL, NULL,
                         (LPBYTE)&pSerialHead->Priority256, &datasize) ) {
        pSerialHead->Priority256 = DEFAULT_CE_THREAD_PRIORITY;
        DEBUGMSG (ZONE_INIT | ZONE_WARN,
                  (TEXT("Failed to get Priority256 value, defaulting to %d\r\n"), pSerialHead->Priority256));
    }

    datasize = sizeof(DWORD);
    if ( RegQueryValueEx(hKey, L"DMAChannel", NULL, NULL,
                         (LPBYTE)&DMAChannelNumber, &datasize) || DMAChannelNumber > 9) {
        DEBUGMSG (ZONE_INIT | ZONE_ERROR,
                  (TEXT("Failed to get DMAChannel value, SERDMA_Init failed\r\n")));
        RegCloseKey (hKey);
        LocalFree(pSerialHead);
        return(NULL);
    }

    RegCloseKey (hKey);

    DEBUGMSG (ZONE_INIT,
              (TEXT("DevIndex %X\r\n"), DevIndex));

    pSerialHead->hKillDispatchThread = CreateEvent(0, FALSE, FALSE, NULL); // auto reset, initially clear
    pSerialHead->hReadEvent = CreateEvent(0, TRUE, FALSE, NULL); // manual reset, initially clear
    pSerialHead->hReceiveBufferEmpty = CreateEvent(0, TRUE, TRUE, NULL); // manual reset, initially set
    wcscpy(ChannelEventName, L"channel0");
    ChannelEventName[7] = '0'+(WCHAR)DMAChannelNumber; // DMAChannelNumber has been validated to be 0...9 already.
    pSerialHead->hReceiveDataReady = CreateEvent(0, FALSE, FALSE, ChannelEventName); // set/reset by the DMA device itself.  The second and third args are ignored.
    if ( !pSerialHead->hKillDispatchThread ||
         !pSerialHead->hReadEvent ||
         !pSerialHead->hReceiveBufferEmpty ||
         !pSerialHead->hReceiveDataReady) {
        DEBUGMSG(ZONE_ERROR | ZONE_INIT,
                 (TEXT("Error creating event, SERDMA_Init failed\n\r")));
        LocalFree(pSerialHead);
        return NULL;
    }

    // Initialize hardware dependent data.
    pSerialHead->pHWObj = GetSerialObject( DevIndex );
    if ( !pSerialHead->pHWObj ) {
        DEBUGMSG(ZONE_ERROR | ZONE_INIT,
                 (TEXT("Error in GetSerialObject, SERDMA_Init failed\n\r")));
        LocalFree(pSerialHead);
        return NULL;
    }

    DEBUGMSG (ZONE_INIT, (TEXT("About to call HWInit(%s,0x%X)\r\n"),
                          Identifier, pSerialHead));
    pHWHead = pSerialHead->pHWObj->pFuncTbl->HWInit(Identifier, pSerialHead, pSerialHead->pHWObj);
    pSerialHead->pHWHead = pHWHead;

    /* Check that HWInit did stuff ok.  From here on out, call Deinit function
     * when things fail.
     */
    if ( !pHWHead ) {
        DEBUGMSG (ZONE_INIT | ZONE_ERROR,
                  (TEXT("Hardware doesn't init correctly, SERDMA_Init failed\r\n")));
        SERDMA_Deinit(pSerialHead);
        return NULL;
    }
    DEBUGMSG (ZONE_INIT,
              (TEXT("Back from hardware init\r\n")));

    if ( pSerialHead->pHWObj->BindFlags & THREAD_AT_INIT ) {
        // Hook the interrupt and start the associated thread.
        if ( ! StartDispatchThread( pSerialHead ) ) {
            // Failed on InterruptInitialize or CreateThread.  Bail.
            SERDMA_Deinit(pSerialHead);
            return NULL;
        }

    }

    // OK, now that everything is ready on our end, give the PDD
    // one last chance to init interrupts, etc.
    // For SERDMA, this opens the file HANDLE to the DMA device
    // instance.
    if (!pSerialHead->pHWObj->pFuncTbl->HWPostInit( pHWHead )) {
        SERDMA_Deinit(pSerialHead);
        return NULL;
    }

    DEBUGMSG (ZONE_INIT | ZONE_FUNCTION, (TEXT("-SERDMA_Init\r\n")));
    return pSerialHead;
}

/*
 @doc EXTERNAL
 @func          HANDLE | SERDMA_Open | Serial port driver initialization.
 *      Description: This routine must be called by the user to open the
 *      serial device. The HANDLE returned must be used by the application in
 *      all subsequent calls to the serial driver. This routine starts the thread
 *      which handles the serial events.
 *      Exported to users.
 *
 @rdesc This routine returns a HANDLE representing the device.
 */
HANDLE
SERDMA_Open(
        HANDLE  pHead,          // @parm Handle returned by SERDMA_Init.
        DWORD   AccessCode,     // @parm access code.
        DWORD   ShareMode       // @parm share mode - Not used in this driver.
        )
{
    PHW_INDEP_INFO  pSerialHead = (PHW_INDEP_INFO)pHead;
    PHW_OPEN_INFO   pOpenHead;
    PHWOBJ          pHWObj      = pSerialHead->pHWObj;

    DEBUGMSG (ZONE_OPEN|ZONE_FUNCTION, (TEXT("+SERDMA_Open handle x%X, access x%X, share x%X\r\n"),
                                        pHead, AccessCode, ShareMode));


    // Return NULL if SerialInit failed.
    if ( !pSerialHead ) {
        DEBUGMSG (ZONE_OPEN|ZONE_ERROR,
                  (TEXT("Open attempted on uninited device!\r\n")));
        SetLastError(ERROR_INVALID_HANDLE);
        return(NULL);
    }

    // Return NULL if opening with access & someone else already has
    if ( (AccessCode & (GENERIC_READ | GENERIC_WRITE)) &&
         pSerialHead->pAccessOwner ) {
        DEBUGMSG (ZONE_OPEN|ZONE_ERROR,
                  (TEXT("Open requested access %x, handle x%X already has x%X!\r\n"),
                   AccessCode, pSerialHead->pAccessOwner,
                   pSerialHead->pAccessOwner->AccessCode));
        SetLastError(ERROR_INVALID_ACCESS);
        return(NULL);
    }
    // OK, lets allocate an open structure
    pOpenHead    =  (PHW_OPEN_INFO)LocalAlloc(LPTR, sizeof(HW_OPEN_INFO));
    if ( !pOpenHead ) {
        DEBUGMSG(ZONE_INIT | ZONE_ERROR,
                 (TEXT("Error allocating memory for pOpenHead, SERDMA_Open failed\n\r")));
        return(NULL);
    }

    // Init the structure
    pOpenHead->pSerialHead = pSerialHead;  // pointer back to our parent
    pOpenHead->StructUsers = 0;
    pOpenHead->AccessCode = AccessCode;
    pOpenHead->ShareMode = ShareMode;
    pOpenHead->CommEvents.hCommEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    pOpenHead->CommEvents.fEventMask = 0;
    pOpenHead->CommEvents.fEventData = 0;
    pOpenHead->CommEvents.fAbort = 0;
        InitializeCriticalSection(&(pOpenHead->CommEvents.EventCS));

    // if we have access permissions, note it in pSerialhead
    if ( AccessCode & (GENERIC_READ | GENERIC_WRITE) ) {
        DEBUGMSG(ZONE_INIT|ZONE_CLOSE,
                 (TEXT("SERDMA_Open: Access permission handle granted x%X\n\r"),
                  pOpenHead));
        pSerialHead->pAccessOwner = pOpenHead;
    }

    // add this open entry to list of open entries.
    // Note that we hold the open CS for the duration of the routine since
    // all of our state info is in flux during this time.  In particular,
    // without the CS is would be possible for an open & close to be going on
    // simultaneously and have bad things happen like spinning a new event
    // thread before the old one was gone, etc.
    EnterCriticalSection(&(pSerialHead->OpenCS));
    InsertHeadList(&pSerialHead->OpenList,
                   &pOpenHead->llist);

    // If port not yet opened, we need to do some init
    if ( ! pSerialHead->OpenCnt ) {
        DEBUGMSG(ZONE_INIT|ZONE_OPEN,
                 (TEXT("SERDMA_Open: First open : Do Init x%X\n\r"),
                  pOpenHead));

        if ( pSerialHead->pHWObj->BindFlags & THREAD_AT_OPEN ) {
            DEBUGMSG(ZONE_INIT|ZONE_OPEN,
                     (TEXT("SERDMA_Open: Starting DispatchThread x%X\n\r"),
                      pOpenHead));
            // Hook the interrupt and start the associated thread.
            if ( ! StartDispatchThread( pSerialHead ) ) {
                // Failed on InterruptInitialize or CreateThread.  Bail.
                DEBUGMSG(ZONE_INIT|ZONE_OPEN,
                         (TEXT("SERDMA_Open: Failed StartDispatchThread x%X\n\r"),
                          pOpenHead));
                goto OpenFail;
            }
        }
        pHWObj->pFuncTbl->HWSetCommTimeouts(pSerialHead->pHWHead,
                                            &(pSerialHead->CommTimeouts));

        if ( !pHWObj->pFuncTbl->HWOpen(pSerialHead->pHWHead) ) {
            DEBUGMSG (ZONE_OPEN|ZONE_ERROR, (TEXT("HW Open failed.\r\n")));
            goto OpenFail;
        }

        pHWObj->pFuncTbl->HWPurgeComm(pSerialHead->pHWHead, PURGE_RXCLEAR);
        if ( pHWObj->BindFlags & THREAD_IN_MDD ) {
            CeSetThreadPriority(pSerialHead->pDispatchThread,
                                pSerialHead->Priority256);
        }
    }

    ++(pSerialHead->OpenCnt);

    // OK, we are finally back in a stable state.  Release the CS.
    LeaveCriticalSection(&(pSerialHead->OpenCS));

    DEBUGMSG (ZONE_OPEN|ZONE_FUNCTION, (TEXT("-SERDMA_Open handle x%X, x%X, Ref x%X\r\n"),
                                        pOpenHead, pOpenHead->pSerialHead, pSerialHead->OpenCnt));

    return pOpenHead;

OpenFail :
    DEBUGMSG (ZONE_OPEN|ZONE_FUNCTION, (TEXT("-SERDMA_Open handle x%X, x%X, Ref x%X\r\n"),
                                        NULL, pOpenHead->pSerialHead, pSerialHead->OpenCnt));

    SetLastError(ERROR_OPEN_FAILED);

    // If this was the handle with access permission, remove pointer
    if ( pOpenHead == pSerialHead->pAccessOwner )
        pSerialHead->pAccessOwner = NULL;

    // Remove the Open entry from the linked list
    RemoveEntryList(&pOpenHead->llist);

    // OK, everything is stable so release the critical section
    LeaveCriticalSection(&(pSerialHead->OpenCS));

    // Free all data allocated in open
    if ( pOpenHead->CommEvents.hCommEvent )
        CloseHandle(pOpenHead->CommEvents.hCommEvent);
        DeleteCriticalSection(&(pOpenHead->CommEvents.EventCS));
    LocalFree( pOpenHead );

    return NULL;
}

// ****************************************************************
//
//      @doc EXTERNAL
//
//      @func BOOL      | SERDMA_Close | close the serial device.
//
//      @parm DWORD | pHead             | Context pointer returned from SERDMA_Open
//
//      @rdesc TRUE if success; FALSE if failure
//
//      @remark This routine is called by the device manager to close the device.
//
//
//
BOOL
SERDMA_Close(PHW_OPEN_INFO pOpenHead)
{
    PHW_INDEP_INFO  pSerialHead = pOpenHead->pSerialHead;
    PHWOBJ          pHWObj;
    int i;
    BOOL            RetCode = TRUE;

    DEBUGMSG (ZONE_CLOSE|ZONE_FUNCTION, (TEXT("+SERDMA_Close\r\n")));

    if ( !pSerialHead ) {
        DEBUGMSG (ZONE_ERROR, (TEXT("!!SERDMA_Close: pSerialHead == NULL!!\r\n")));
        SetLastError(ERROR_INVALID_HANDLE);
        return(FALSE);
    }
    pHWObj = (PHWOBJ)pSerialHead->pHWObj;

    // Use the OpenCS to make sure we don't collide with an in-progress open.
    EnterCriticalSection(&(pSerialHead->OpenCS));

    if ( pSerialHead->OpenCnt ) {
        --(pSerialHead->OpenCnt);

        DEBUGMSG (1, (TEXT("SERDMA_Close: (%d handles)\r\n"), pSerialHead->OpenCnt));

        // In multi open case, do we need to restore state later on or something????
        if ( pHWObj && (pHWObj->BindFlags & THREAD_IN_MDD) &&
             pSerialHead->pDispatchThread ) {
            SetThreadPriority(pSerialHead->pDispatchThread,
                              THREAD_PRIORITY_NORMAL);
        }

        // Free up any threads which may be blocked waiting for events.
        // Monitor users count to make sure no one is still in one of
        // the SERDMA_ functions before proceeding (wait up to 2 s).
        for ( i=0; i<20 && pOpenHead->StructUsers; i++ ) {
            DEBUGMSG(ZONE_INIT|ZONE_CLOSE,
                     (TEXT("SERDMA_Close: %d users in MDD functions\n\r"),pOpenHead->StructUsers));

            // For any open handle, we must free pending waitcommevents
            EnterCriticalSection(&(pOpenHead->CommEvents.EventCS));
            pOpenHead->CommEvents.fEventMask = 0;
            pOpenHead->CommEvents.fAbort = 1;
            SetEvent(pOpenHead->CommEvents.hCommEvent);
            LeaveCriticalSection(&(pOpenHead->CommEvents.EventCS));

            // And only for the handle with access permissions do we
            // have to worry about read, write, etc being blocked.
            if ( pOpenHead->AccessCode & (GENERIC_READ | GENERIC_WRITE) ) {
                SetEvent(pSerialHead->hReadEvent);
            }

            /* Sleep, let threads finish */
            Sleep(100);
        }
        if ( i==20 )
            DEBUGMSG(ZONE_CLOSE|ZONE_INIT|ZONE_ERROR,
                     (TEXT("SERDMA_Close: Waited 2s for serial users to exit, %d left\n\r"),
                      pOpenHead->StructUsers));

        // If we are closing the last open handle, then close PDD also
        if ( !pSerialHead->OpenCnt ) {
            DEBUGMSG (ZONE_CLOSE, (TEXT("About to call HWClose\r\n")));
            if ( pHWObj )
                pHWObj->pFuncTbl->HWClose(pSerialHead->pHWHead);
            DEBUGMSG (ZONE_CLOSE, (TEXT("Returned from HWClose\r\n")));

            // And if thread was spun in open, kill it now.
            if ( pSerialHead->pHWObj->BindFlags & THREAD_AT_OPEN ) {
                DEBUGMSG (ZONE_CLOSE, (TEXT("SERDMA_Close : Stopping Dispatch Thread\r\n")));
                StopDispatchThread( pSerialHead );
            }
        }

        // If this was the handle with access permission, remove pointer
        if ( pOpenHead == pSerialHead->pAccessOwner ) {
            DEBUGMSG(ZONE_INIT|ZONE_CLOSE,
                     (TEXT("SERDMA_Close: Closed access owner handle\n\r"),
                      pOpenHead));

            pSerialHead->pAccessOwner = NULL;
        }

        // Remove the entry from the linked list
        RemoveEntryList(&pOpenHead->llist);

        // Free all data allocated in open
        DeleteCriticalSection(&(pOpenHead->CommEvents.EventCS));
        if ( pOpenHead->CommEvents.hCommEvent )
            CloseHandle(pOpenHead->CommEvents.hCommEvent);
        LocalFree( pOpenHead );
    } else {
        DEBUGMSG (ZONE_ERROR, (TEXT("!!Close of non-open serial port\r\n")));
        SetLastError(ERROR_INVALID_HANDLE);
        RetCode = FALSE;
    }

    // OK, other inits/opens can go ahead.
    LeaveCriticalSection(&(pSerialHead->OpenCS));

    DEBUGMSG (ZONE_CLOSE|ZONE_FUNCTION, (TEXT("-SERDMA_Close\r\n")));

    return RetCode;
}

/*
 @doc EXTERNAL
 @func  BOOL | SERDMA_Deinit | De-initialize serial port.
 @parm DWORD | pSerialHead | Context pointer returned from SERDMA_Init
 *
 @rdesc None.
 */
BOOL
SERDMA_Deinit(PHW_INDEP_INFO pSerialHead)
{
    DEBUGMSG (ZONE_INIT|ZONE_FUNCTION, (TEXT("+SERDMA_Deinit\r\n")));

    if ( !pSerialHead ) {
        /* Can't do much without this */
        DEBUGMSG (ZONE_INIT|ZONE_ERROR,
                  (TEXT("SERDMA_Deinit can't find pSerialHead\r\n")));
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    // If we have an interrupt handler thread, kill it
    if ( pSerialHead->pHWObj->BindFlags & THREAD_IN_MDD ) {
        StopDispatchThread( pSerialHead );
    }

    /*
    ** Call close, if we have a user.  Note that this call will ensure that
    ** all users are out of the serial routines before it returns, so we can
    ** go ahead and free our internal memory.
    */
    if ( pSerialHead->OpenCnt ) {
        PLIST_ENTRY     pEntry;
        PHW_OPEN_INFO   pOpenHead;

        pEntry = pSerialHead->OpenList.Flink;
        while ( pEntry != &pSerialHead->OpenList ) {
            pOpenHead = CONTAINING_RECORD( pEntry, HW_OPEN_INFO, llist);
            pEntry = pEntry->Flink;  // advance to next

            DEBUGMSG (ZONE_INIT | ZONE_CLOSE, (TEXT(" Deinit - Closing Handle 0x%X\r\n"),
                                               pOpenHead ));
            SERDMA_Close(pOpenHead);
        }
    }

    /* Free our resources */
    if ( pSerialHead->hKillDispatchThread )
        CloseHandle(pSerialHead->hKillDispatchThread);
    if ( pSerialHead->hReadEvent )
        CloseHandle(pSerialHead->hReadEvent);
    if ( pSerialHead->hReceiveBufferEmpty )
        CloseHandle(pSerialHead->hReceiveBufferEmpty);
    if ( pSerialHead->hReceiveDataReady )
        CloseHandle(pSerialHead->hReceiveDataReady);

    DeleteCriticalSection(&(pSerialHead->ReceiveCritSec1));
    DeleteCriticalSection(&(pSerialHead->OpenCS));

    /* Now, call HW specific deinit function */
    if ( pSerialHead->pHWObj && pSerialHead->pHWObj->pFuncTbl ) {
        DEBUGMSG (ZONE_INIT, (TEXT("About to call HWDeinit\r\n")));
        pSerialHead->pHWObj->pFuncTbl->HWDeinit(pSerialHead->pHWHead);
        DEBUGMSG (ZONE_INIT, (TEXT("Returned from HWDeinit\r\n")));
    }

    LocalFree(pSerialHead);

    DEBUGMSG (ZONE_INIT|ZONE_FUNCTION, (TEXT("-SERDMA_Deinit\r\n")));
    return TRUE;
}

/*
   @doc EXTERNAL
   @func        ULONG | SERDMA_Read | Allows application to receive characters from
   *    serial port. This routine sets the buffer and bufferlength to be used
   *    by the reading thread. It also enables reception and controlling when
   *    to return to the user. It writes to the referent of the fourth argument
   *    the number of bytes transacted. It returns the status of the call.
   *
   *    Exported to users.
   @rdesc This routine returns: -1 if error, or number of bytes read.
   */
ULONG
SERDMA_Read(
        HANDLE      pHead,          //@parm [IN]         HANDLE returned by SERDMA_Open
        PUCHAR      pTargetBuffer,  //@parm [IN,OUT] Pointer to valid memory.
        ULONG       BufferLength    //@parm [IN]         Size in bytes of pTargetBuffer.
        )
{
    PHW_OPEN_INFO   pOpenHead = (PHW_OPEN_INFO)pHead;
    PHW_INDEP_INFO  pSerialHead = pOpenHead->pSerialHead;
    PHW_VTBL        pFuncTbl;
    PVOID           pHWHead;
    ULONG           BytesRead = 0; // this initial value is returned if there is a timeout
    int             ReadTries;
    ULONG           Ticks;
    ULONG           Timeout;
    ULONG           IntervalTimeout;    // The interval timeout
    ULONG           AddIntervalTimeout;
    ULONG           TotalTimeout;       // The Total Timeout
    ULONG           TimeSpent = 0;      // How much time have we been waiting?

    DEBUGMSG (ZONE_USR_READ|ZONE_FUNCTION,
              (TEXT("+SERDMA_Read(0x%X,0x%X,%d)\r\n"),
               pHead, pTargetBuffer, BufferLength));

    // Check to see that the call is valid.
    if ( !pSerialHead || !pSerialHead->OpenCnt ) {
        DEBUGMSG (ZONE_USR_READ|ZONE_ERROR,
                  (TEXT("SERDMA_Read, device not open\r\n") ));
        SetLastError (ERROR_INVALID_HANDLE);
        return (ULONG)-1;
    }
    pFuncTbl       = pSerialHead->pHWObj->pFuncTbl;
    pHWHead        = pSerialHead->pHWHead;

    // Make sure the caller has access permissions
    if ( !(pOpenHead->AccessCode & GENERIC_READ) ) {
        DEBUGMSG(ZONE_USR_READ|ZONE_ERROR,
                 (TEXT("SERDMA_Read: Access permission failure x%X\n\r"),
                  pOpenHead->AccessCode));
        SetLastError (ERROR_INVALID_ACCESS);
        return (ULONG)-1;
    }

    if ( BufferLength > DMA_PACKET_SIZE) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return (ULONG)-1;
    }

    pTargetBuffer = MapCallerPtr(pTargetBuffer, BufferLength);
    if (pTargetBuffer == NULL) {
        BytesRead = (ULONG)-1;
        SetLastError(ERROR_INVALID_PARAMETER);
        return (ULONG)-1;
    }

    COM_INC_USAGE_CNT(pOpenHead);

    /* Practice safe threading.
     */
    EnterCriticalSection(&(pSerialHead->ReceiveCritSec1));

    /* Compute total time to wait. Take product and add constant.
     */
    if ( MAXDWORD != pSerialHead->CommTimeouts.ReadTotalTimeoutMultiplier ) {
        TotalTimeout = pSerialHead->CommTimeouts.ReadTotalTimeoutMultiplier*BufferLength +
                       pSerialHead->CommTimeouts.ReadTotalTimeoutConstant;
                // Because we are using FIFO and water level is set to 8, we have to do following
                AddIntervalTimeout=pSerialHead->CommTimeouts.ReadTotalTimeoutMultiplier*8;
    } else {
        TotalTimeout = pSerialHead->CommTimeouts.ReadTotalTimeoutConstant;
                AddIntervalTimeout=0;
    }
    IntervalTimeout = pSerialHead->CommTimeouts.ReadIntervalTimeout;
    if (IntervalTimeout < MAXDWORD  - AddIntervalTimeout) {
        IntervalTimeout +=AddIntervalTimeout;
    }

    DEBUGMSG (ZONE_USR_READ, (TEXT("TotalTimeout:%d\r\n"), TotalTimeout));

    for (ReadTries=0;ReadTries<2;ReadTries++) {
        if (pSerialHead->RxDataReady) {
            // A packet is already ready.  Copy it to the caller's buffer
            BytesRead = min(BufferLength, pSerialHead->ReceivePacketBuffer.u.PacketLength);
	    __try {
            	memcpy(pTargetBuffer,
                       pSerialHead->ReceivePacketBuffer.u.PacketContents, BytesRead);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                DEBUGMSG(ZONE_USR_READ|ZONE_ERROR,
                         (TEXT("SERDMA_Read: Buffer error\n\r")));
                SetLastError(ERROR_INVALID_PARAMETER);
                BytesRead = -1;
                break;
            }

            // Indicate that the ReceivePacketBuffer is available again
            pSerialHead->RxDataReady = 0;
            SetEvent(pSerialHead->hReceiveBufferEmpty);
            break;
        }

        // Else no packet is ready - wait until one arrives or we time out

        // Wait for a serial event?
        if ( (IntervalTimeout == MAXDWORD) && (TotalTimeout == 0) ) {
            // For some reason this means don't wait.
            break;
        }

        if ( (IntervalTimeout == 0) && (TotalTimeout == 0) ) {
            // Not completely clear but this could mean wait
            // for ever
            Timeout = INFINITE;
        } else if ( TotalTimeout == 0 ) {
            // On first character we only use total timeout
            Timeout = INFINITE;
        } else {
            // Total timeout is valid
            if ( TimeSpent >= TotalTimeout ) {
                // Timed out.
                break;
            }
            Timeout = TotalTimeout - TimeSpent;
            // On first byte we only use interval timeout
        }
        Ticks = GetTickCount();
        DEBUGMSG (ZONE_USR_READ, (TEXT("About to wait %dms\r\n"), Timeout));

        pSerialHead->fAbortRead = 0;
        if ( WAIT_TIMEOUT == WaitForSingleObject (pSerialHead->hReadEvent,
                                                  Timeout) ) {
            // Timeout
            break;
        }

        // In the absense of WaitForMultipleObjects, we use flags to
        // handle errors/aborts. Check for aborts or asynchronous closes.
        if ( pSerialHead->fAbortRead ) {
            DEBUGMSG(ZONE_USR_READ,(TEXT("SERDMA_Read - Aborting read\r\n")));
            break;
        }

        if ( !pSerialHead->OpenCnt ) {
            DEBUGMSG(ZONE_USR_READ|ZONE_ERROR,
                     (TEXT("SERDMA_Read - device was closed\n\r")));
            SetLastError(ERROR_INVALID_HANDLE);
            break;
        }
    }
    LeaveCriticalSection(&(pSerialHead->ReceiveCritSec1));
    COM_DEC_USAGE_CNT(pOpenHead);

    DEBUGMSG (ZONE_USR_READ|ZONE_FUNCTION,
              (TEXT("-SERDMA_Read: returning %d\r\n"),
               BytesRead));

    return BytesRead;
}

/*
   @doc EXTERNAL
   @func ULONG | SERDMA_Write | Allows application to transmit bytes to the serial port. Exported to users.
   *
   @rdesc It returns the number of bytes written or -1 if error.
   *
   *
   */
ULONG
SERDMA_Write(HANDLE pHead,      /*@parm [IN]  HANDLE returned by SERDMA_Open.*/
          PUCHAR pSourceBytes,  /*@parm [IN]  Pointer to bytes to be written.*/
          ULONG  NumberOfBytes  /*@parm [IN]  Number of bytes to be written. */
         )
{
    PHW_OPEN_INFO   pOpenHead = (PHW_OPEN_INFO)pHead;
    PHW_INDEP_INFO  pSerialHead = pOpenHead->pSerialHead;
    PHW_VTBL        pFuncTbl;
    PHWOBJ          pHWObj;
    PVOID           pHWHead;
    ULONG           Len;
    ULONG           ulRet = -1;
    DMA_PACKET_BUFFER *pTransmitPacketBuffer = NULL;

    // bugbug:  COM_Write in the SMDK2410 version begins with a Sleep(10).  Remove it!

    DEBUGMSG (ZONE_WRITE|ZONE_FUNCTION,
              (TEXT("+SERDMA_Write(0x%X, 0x%X, %d)\r\n"), pHead,
              pSourceBytes, NumberOfBytes));

    // Check validity of handle
    if ( !pSerialHead || !pSerialHead->OpenCnt ) {
        DEBUGMSG (ZONE_WRITE|ZONE_ERROR,
                  (TEXT("SERDMA_Write, device not open\r\n") ));
        SetLastError (ERROR_INVALID_HANDLE);
        return (ULONG)-1;
    }

    // Make sure the caller has access permissions
    if ( !(pOpenHead->AccessCode & GENERIC_WRITE) ) {
        DEBUGMSG(ZONE_USR_READ|ZONE_ERROR,
                 (TEXT("SERDMA_Write: Access permission failure x%X\n\r"),
                  pOpenHead->AccessCode));
        SetLastError (ERROR_INVALID_ACCESS);
        return (ULONG)-1;
    }

    pSourceBytes = MapCallerPtr(pSourceBytes, NumberOfBytes);
    if ( pSourceBytes == NULL ) {
        DEBUGMSG (ZONE_WRITE|ZONE_ERROR,
                  (TEXT("SERDMA_Write, bad read pointer\r\n") ));
        SetLastError(ERROR_INVALID_PARAMETER);
        return (ULONG)-1;
    }

    // Construct the DMA packet
    if (NumberOfBytes > sizeof(pTransmitPacketBuffer->u.PacketContents)) {
        DEBUGMSG (ZONE_WRITE|ZONE_ERROR,
                  (TEXT("SERDMA_Write, bad read pointer\r\n") ));
        SetLastError(ERROR_INVALID_PARAMETER);
        return (ULONG)-1;
    }

    pTransmitPacketBuffer = (DMA_PACKET_BUFFER*) 
        InterlockedExchangePointer(g_pPacketBufferCache, NULL);
    if ( pTransmitPacketBuffer == NULL ) {
        // Another thread is using the global PacketBuffer. Allocate a new one here.
        pTransmitPacketBuffer = LocalAlloc(LMEM_FIXED, sizeof(DMA_PACKET_BUFFER));
        if ( pTransmitPacketBuffer == NULL ) {
            SetLastError(ERROR_OUTOFMEMORY);
            return (ULONG)-1;
        }
    }

    pTransmitPacketBuffer->u.PacketLength = (USHORT)NumberOfBytes;
    __try {
        memcpy(pTransmitPacketBuffer->u.PacketContents, pSourceBytes, NumberOfBytes);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG (ZONE_WRITE|ZONE_ERROR,
                  (TEXT("SERDMA_Write, bad read pointer\r\n") ));
        SetLastError(ERROR_INVALID_PARAMETER);
        goto EXIT;
    }

    COM_INC_USAGE_CNT(pOpenHead);

    pHWObj   = pSerialHead->pHWObj;
    pHWHead  = pSerialHead->pHWHead;
    pFuncTbl = pHWObj->pFuncTbl;

    // Transmit the packet
    Len = sizeof(*pTransmitPacketBuffer);
    pFuncTbl->HWTxIntrHandler(pHWHead,
                              (PUCHAR)pTransmitPacketBuffer,
                              &Len);

    // Report the event to WaitCommEvent()
    EvaluateEventFlag(pSerialHead, EV_TXEMPTY);

    COM_DEC_USAGE_CNT(pOpenHead);

    ulRet = NumberOfBytes;

EXIT:
    if (pTransmitPacketBuffer == &g_PacketBuffer) {
        // We're using the global buffer. Release it now.
        g_pPacketBufferCache = &g_PacketBuffer;
    }
    else {
        LocalFree(pTransmitPacketBuffer);
    }
    
    DEBUGMSG (ZONE_WRITE|ZONE_FUNCTION,
              (TEXT("-SERDMA_Write, returning %d\n\r"),ulRet));
    
    return ulRet;
}

ULONG
SERDMA_Seek(
        HANDLE  pHead,
        LONG    Position,
        DWORD   Type
        )
{
    return(ULONG)-1;
}

/*
 @doc EXTERNAL
 @func  BOOL | SERDMA_PowerUp | Turn power on to serial device
 * Exported to users.
 @rdesc This routine returns a status of 1 if unsuccessful and 0 otherwise.
 */
BOOL
SERDMA_PowerUp(
           HANDLE      pHead       /*@parm Handle to device. */
           )
{
    return 0;
}

/*
 @doc EXTERNAL
 @func  BOOL | SERDMA_PowerDown | Turns off power to serial device.
 * Exported to users.
 @rdesc This routine returns a status of 1 if unsuccessful and 0 otherwise.
 */
BOOL
SERDMA_PowerDown(
             HANDLE      pHead       /*@parm Handle to device. */
             )
{
    return 0;
}

// Routine to handle the PROCESS_EXITING flag.  Should let free any threads blocked on pOpenHead,
// so they can be killed and the process closed.

BOOL
ProcessExiting(PHW_OPEN_INFO pOpenHead)
{
    PHW_INDEP_INFO  pSerialHead = pOpenHead->pSerialHead;
    PHWOBJ          pHWObj;
    int i;
    BOOL            RetCode = TRUE;

    DEBUGMSG (ZONE_CLOSE|ZONE_FUNCTION, (TEXT("+ProcessExiting\r\n")));

    if ( !pSerialHead ) {
        DEBUGMSG (ZONE_ERROR, (TEXT("!!ProcessExiting: pSerialHead == NULL!!\r\n")));
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    pHWObj = (PHWOBJ)pSerialHead->pHWObj;

    // Use the OpenCS to make sure we don't collide with an in-progress open.
    EnterCriticalSection(&(pSerialHead->OpenCS));

    if ( pSerialHead->OpenCnt ) {

        DEBUGMSG (1,
                  (TEXT("ProcessClose: (%d handles)\r\n"),
                   pSerialHead->OpenCnt));

        // In multi open case, do we need to restore state later on or something????
        if ( pHWObj && (pHWObj->BindFlags & THREAD_IN_MDD) &&
             pSerialHead->pDispatchThread ) {
            SetThreadPriority(pSerialHead->pDispatchThread,
                              THREAD_PRIORITY_NORMAL);
        }

        // Free up any threads which may be blocked waiting for events.
        // Monitor users count to make sure no one is still in one of
        // the SERDMA_ functions before proceeding (wait up to 2 s).
        for ( i=0; i<20 && pOpenHead->StructUsers; i++ ) {
            DEBUGMSG(ZONE_INIT|ZONE_CLOSE,
                     (TEXT("ProcessExiting: %d users in MDD functions\n\r"),pOpenHead->StructUsers));

            // For any open handle, we must free pending waitcommevents
            EnterCriticalSection(&(pOpenHead->CommEvents.EventCS));
            pOpenHead->CommEvents.fEventMask = 0;
            pOpenHead->CommEvents.fAbort = 1;
            SetEvent(pOpenHead->CommEvents.hCommEvent);
            LeaveCriticalSection(&(pOpenHead->CommEvents.EventCS));

            // And only for the handle with access permissions do we
            // have to worry about read, write, etc being blocked.
            if ( pOpenHead->AccessCode & (GENERIC_READ | GENERIC_WRITE) ) {
                SetEvent(pSerialHead->hReadEvent);
            }

            /* Sleep, let threads finish */
            Sleep(100);
        }
        if ( i==20 )
            DEBUGMSG(ZONE_CLOSE|ZONE_INIT|ZONE_ERROR,
                     (TEXT("ProcessExiting: Waited 2s for serial users to exit, %d left\n\r"),
                      pOpenHead->StructUsers));

    } else {
        DEBUGMSG (ZONE_ERROR, (TEXT("!!Close of non-open serial port\r\n")));
        SetLastError(ERROR_INVALID_HANDLE);
        RetCode = FALSE;
    }

    // OK, other inits/opens can go ahead.
    LeaveCriticalSection(&(pSerialHead->OpenCS));

    DEBUGMSG (ZONE_CLOSE|ZONE_FUNCTION, (TEXT("-ProcessExiting\r\n")));
    return RetCode;
}

/*
 *  @doc INTERNAL
 *      @func           BOOL | ApplyDCB | Apply the current DCB.
 *
 *      This function will apply the current DCB settings to the device.
 *      It will also call to the PDD to set the PDD values.
 */
BOOL
ApplyDCB (PHW_INDEP_INFO pSerialHead, DCB *pDCB, BOOL fOpen)
{
    PHWOBJ          pHWObj      = pSerialHead->pHWObj;

    if ( !pHWObj->pFuncTbl->HWSetDCB(pSerialHead->pHWHead,
                                     pDCB) ) {
        return FALSE;
    }

    if ( !fOpen ) {
        return TRUE;
    }

    // If PDD SetDCB was successful, save the supplied DCB and
    // configure port to match these settings.
    memcpy(&(pSerialHead->DCB), pDCB, sizeof(DCB));

    return TRUE;
}

// ****************************************************************
//
//      @func   VOID | EvaluateEventFlag | Evaluate an event mask.
//
//      @parm PHW_INDEP_INFO    | pHWIHead              | MDD context pointer
//      @parm ULONG                             | fdwEventMask  | Bitmask of events
//
//      @rdesc  No return
//
//      @remark This function is called by the PDD (and internally in the MDD
//                      to evaluate a COMM event.  If the user is waiting for a
//                      COMM event (see WaitCommEvent()) then it will signal the
//                      users thread.
//
VOID
EvaluateEventFlag(PVOID pHead, ULONG fdwEventMask)
{
    PHW_INDEP_INFO  pHWIHead = (PHW_INDEP_INFO)pHead;
    PLIST_ENTRY     pEntry;
    PHW_OPEN_INFO   pOpenHead;
    DWORD           dwTmpEvent, dwOrigEvent;
    BOOL            fRetCode;

    if ( !pHWIHead->OpenCnt ) {
        DEBUGMSG (ZONE_EVENTS|ZONE_ERROR,
                  (TEXT(" EvaluateEventFlag - device was closed\r\n")));
        SetLastError (ERROR_INVALID_HANDLE);
        return;
    }

    DEBUGMSG (ZONE_EVENTS, (TEXT(" CommEvent - Event 0x%X, Global Mask 0x%X\r\n"),
                            fdwEventMask,
                            pHWIHead->fEventMask));

    // Now that we support multiple opens, we must check mask for each open handle
    // To keep this relatively painless, we keep a per-device mask which is the
    // bitwise or of each current open mask.  We can check this first before doing
    // all the linked list work to figure out who to notify
    if ( pHWIHead->fEventMask & fdwEventMask ) {
        pEntry = pHWIHead->OpenList.Flink;
        while ( pEntry != &pHWIHead->OpenList ) {
            pOpenHead = CONTAINING_RECORD( pEntry, HW_OPEN_INFO, llist);
            pEntry = pEntry->Flink;  // advance to next

            EnterCriticalSection(&(pOpenHead->CommEvents.EventCS));
            // Don't do anything unless this event is of interest to the MDD.
            if ( pOpenHead->CommEvents.fEventMask & fdwEventMask ) {
                // Store the event data
                dwOrigEvent = pOpenHead->CommEvents.fEventData;
                do {
                    dwTmpEvent = dwOrigEvent;
                    dwOrigEvent = InterlockedExchange(&(pOpenHead->CommEvents.fEventData),
                                                      dwTmpEvent | fdwEventMask) ;

                } while ( dwTmpEvent != dwOrigEvent );

                // Signal the MDD that new event data is available.
                fRetCode = SetEvent(pOpenHead->CommEvents.hCommEvent);
                DEBUGMSG (ZONE_EVENTS, (TEXT(" CommEvent - Event 0x%X, Handle 0x%X Mask 0x%X (%X)\r\n"),
                                        dwTmpEvent | fdwEventMask,
                                        pOpenHead,
                                        pOpenHead->CommEvents.fEventMask,
                                        fRetCode));

            }
            LeaveCriticalSection(&(pOpenHead->CommEvents.EventCS));
        }
    }
}

/*
 @doc INTERNAL
 @func  BOOL | WaitCommEvent | See Win32 documentation.
 * Exported to users.
 */
BOOL
WINAPI
WaitCommEvent(
             PHW_OPEN_INFO   pOpenHead,      // @parm Handle to device.
             PULONG          pfdwEventMask,  // @parm Pointer to ULONG to receive CommEvents.fEventMask.
             LPOVERLAPPED    Unused          // @parm Pointer to OVERLAPPED not used.
             )
{
    PHW_INDEP_INFO  pHWIHead = pOpenHead->pSerialHead;
    DWORD           dwEventData;

    DEBUGMSG(ZONE_FUNCTION|ZONE_EVENTS,(TEXT("+WaitCommEvent x%X x%X, pMask x%X\n\r"),
                                        pOpenHead, pHWIHead , pfdwEventMask));

    if ( !pHWIHead || !pHWIHead->OpenCnt ) {
        DEBUGMSG (ZONE_ERROR|ZONE_EVENTS, (TEXT("-WaitCommEvent - device not open (x%X, %d) \r\n"),
                                           pHWIHead, (pHWIHead == NULL) ? 0 : pHWIHead->OpenCnt));
        *pfdwEventMask = 0;
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    // We should return immediately if mask is 0
    if ( !pOpenHead->CommEvents.fEventMask ) {
        DEBUGMSG (ZONE_ERROR|ZONE_EVENTS, (TEXT("-WaitCommEvent - Mask already clear\r\n")));
        *pfdwEventMask = 0;
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    COM_INC_USAGE_CNT(pOpenHead);

    // Abort should only affect us once we start waiting.  Ignore any old aborts
    pOpenHead->CommEvents.fAbort = 0;

    while ( pHWIHead->OpenCnt ) {
        // Read and clear any event bits
                EnterCriticalSection(&(pOpenHead->CommEvents.EventCS));
                ResetEvent(pOpenHead->CommEvents.hCommEvent);
        dwEventData = InterlockedExchange( &(pOpenHead->CommEvents.fEventData), 0 );
        DEBUGMSG (ZONE_EVENTS, (TEXT(" WaitCommEvent - Events 0x%X, Mask 0x%X, Abort %X\r\n"),
                                dwEventData,
                                pOpenHead->CommEvents.fEventMask,
                                pOpenHead->CommEvents.fAbort ));

        // See if we got any events of interest.
        if ( dwEventData & pOpenHead->CommEvents.fEventMask ) {
            *pfdwEventMask = dwEventData & pOpenHead->CommEvents.fEventMask;
            LeaveCriticalSection(&(pOpenHead->CommEvents.EventCS));
            break;
        } else {
            LeaveCriticalSection(&(pOpenHead->CommEvents.EventCS));
        }

        // Wait for an event from PDD, or from SetCommMask
        WaitForSingleObject(pOpenHead->CommEvents.hCommEvent,
                            (ULONG)-1);

        // We should return immediately if mask was set via SetCommMask.
        if ( pOpenHead->CommEvents.fAbort ) {
            // We must have been terminated by SetCommMask()
            // Return TRUE with a mask of 0.
            DEBUGMSG (ZONE_ERROR|ZONE_EVENTS, (TEXT(" WaitCommEvent - Mask was cleared\r\n")));
            *pfdwEventMask = 0;
            break;
        }

    }

    COM_DEC_USAGE_CNT(pOpenHead);

    // Check and see if device was closed while we were waiting
    if ( !pHWIHead->OpenCnt ) {
        // Device was closed.  Get out of here.
        DEBUGMSG (ZONE_EVENTS|ZONE_ERROR,
                  (TEXT("-WaitCommEvent - device was closed\r\n")));
        *pfdwEventMask = 0;
        SetLastError (ERROR_INVALID_HANDLE);
        return FALSE;
    } else {
        // We either got an event or a SetCommMask 0.
        DEBUGMSG (ZONE_EVENTS,
                  (TEXT("-WaitCommEvent - *pfdwEventMask 0x%X\r\n"),
                   *pfdwEventMask));
        return TRUE;
    }
}

// ****************************************************************
//
//      @func BOOL | SERDMA_IOControl | Device IO control routine
//      @parm DWORD | dwOpenData | value returned from SERDMA_Open call
//      @parm DWORD | dwCode | io control code to be performed
//      @parm PBYTE | pBufIn | input data to the device
//      @parm DWORD | dwLenIn | number of bytes being passed in
//      @parm PBYTE | pBufOut | output data from the device
//      @parm DWORD | dwLenOut |maximum number of bytes to receive from device
//      @parm PDWORD | pdwActualOut | actual number of bytes received from device
//
//      @rdesc          Returns TRUE for success, FALSE for failure
//
//      @remark         Routine exported by a device driver.  "COM" is the string
//                              passed in as lpszType in RegisterDevice

BOOL
SERDMA_IOControl(PHW_OPEN_INFO pOpenHead,
              DWORD dwCode, PBYTE pBufIn,
              DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut,
              PDWORD pdwActualOut)
{
    BOOL            RetVal           = TRUE;        // Initialize to success
    PHW_INDEP_INFO  pHWIHead = pOpenHead->pSerialHead;
    PHWOBJ          pHWObj   = NULL;
    PVOID           pHWHead  = NULL;
    PHW_VTBL        pFuncTbl = NULL;
    DWORD           dwFlags;
    PLIST_ENTRY     pEntry;

    DEBUGMSG (ZONE_IOCTL|ZONE_FUNCTION,
              (TEXT("+SERDMA_IOControl(0x%X, %d, 0x%X, %d, 0x%X, %d, 0x%X)\r\n"),
               pOpenHead, dwCode, pBufIn, dwLenIn, pBufOut,
               dwLenOut, pdwActualOut));

    if ( pHWIHead ) {
        pHWObj   = (PHWOBJ)pHWIHead->pHWObj;
        pFuncTbl = pHWObj->pFuncTbl;
        pHWHead  = pHWIHead->pHWHead;
    } else {
        return FALSE;
    }

    DEBUGMSG (ZONE_IOCTL|ZONE_FUNCTION|(ZONE_ERROR && (RetVal == FALSE)),
              (TEXT("-SERDMA_IOControl %s Ecode=%d (len=%d)\r\n"),
               (RetVal == TRUE) ? TEXT("Success") : TEXT("Error"),
               GetLastError(), (NULL == pdwActualOut) ? 0 : *pdwActualOut));

    if ( !pHWIHead->OpenCnt ) {
        DEBUGMSG (ZONE_IOCTL|ZONE_ERROR,
                  (TEXT("SERDMA_IOControl - device was closed\r\n")));
        SetLastError (ERROR_INVALID_HANDLE);
        return FALSE;
    }

    if ( dwCode == IOCTL_PSL_NOTIFY ) {
        PDEVICE_PSL_NOTIFY pPslPacket = (PDEVICE_PSL_NOTIFY)pBufIn;
        if ( (pPslPacket->dwSize == sizeof(DEVICE_PSL_NOTIFY)) && (pPslPacket->dwFlags == DLL_PROCESS_EXITING) ) {
            DEBUGMSG(ZONE_IOCTL, (TEXT("Process is exiting.\r\n")));
            ProcessExiting(pOpenHead);
        }
        return TRUE;
    }
    // Make sure the caller has access permissions
    // NOTE : Pay attention here.  I hate to make this check repeatedly
    // below, so I'll optimize it here.  But as you add new ioctl's be
    // sure to account for them in this if check.
    if ( !( (dwCode == IOCTL_SERIAL_GET_WAIT_MASK) ||
            (dwCode == IOCTL_SERIAL_SET_WAIT_MASK) ||
            (dwCode == IOCTL_SERIAL_WAIT_ON_MASK) ||
            (dwCode == IOCTL_SERIAL_GET_MODEMSTATUS) ||
            (dwCode == IOCTL_SERIAL_GET_PROPERTIES) ||
            (dwCode == IOCTL_SERIAL_GET_TIMEOUTS) ||
            (dwCode == IOCTL_POWER_CAPABILITIES) ||
            (dwCode == IOCTL_POWER_QUERY) ||
            (dwCode == IOCTL_POWER_SET)) ) {
        // If not one of the above operations, then read or write
        // access permissions are required.
        if ( !(pOpenHead->AccessCode & (GENERIC_READ | GENERIC_WRITE) ) ) {
            DEBUGMSG(ZONE_IOCTL|ZONE_ERROR,
                     (TEXT("SERDMA_Ioctl: Ioctl %x access permission failure x%X\n\r"),
                      dwCode, pOpenHead->AccessCode));
            SetLastError (ERROR_INVALID_ACCESS);
            return FALSE;
        }

    }

    COM_INC_USAGE_CNT(pOpenHead);

    switch ( dwCode ) {
    // ****************************************************************
    //
    //  @func BOOL      | IOCTL_SERIAL_SET_BREAK_ON |
    //                          Device IO control routine to set the break state.
    //
    //  @parm DWORD | dwOpenData | value returned from SERDMA_Open call
    //  @parm DWORD | dwCode | IOCTL_SERIAL_SET_BREAK_ON
    //  @parm PBYTE | pBufIn | Ignored
    //  @parm DWORD | dwLenIn | Ignored
    //  @parm PBYTE | pBufOut | Ignored
    //  @parm DWORD | dwLenOut | Ignored
    //  @parm PDWORD | pdwActualOut | Ignored
    //
    //  @rdesc          Returns TRUE for success, FALSE for failure (and
    //                          sets thread error code)
    //
    //  @remark Sets the transmission line in a break state until
    //                          <f IOCTL_SERIAL_SET_BREAK_OFF> is called.
    //
    case IOCTL_SERIAL_SET_BREAK_ON :
        DEBUGMSG (ZONE_IOCTL,
                  (TEXT(" IOCTL_SERIAL_SET_BREAK_ON\r\n")));
        // A no-op for SERDMA
        break;

        // ****************************************************************
        //
        //      @func BOOL      | IOCTL_SERIAL_SET_BREAK_OFF |
        //                              Device IO control routine to clear the break state.
        //
        //      @parm DWORD | dwOpenData | value returned from SERDMA_Open call
        //      @parm DWORD | dwCode | IOCTL_SERIAL_SET_BREAK_OFF
        //      @parm PBYTE | pBufIn | Ignored
        //      @parm DWORD | dwLenIn | Ignored
        //      @parm PBYTE | pBufOut | Ignored
        //      @parm DWORD | dwLenOut | Ignored
        //      @parm PDWORD | pdwActualOut | Ignored
        //
        //      @rdesc          Returns TRUE for success, FALSE for failure (and
        //                              sets thread error code)
        //
        //      @remark         Restores character transmission for the communications
        //                              device and places the transmission line in a nonbreak state
        //                              (called after <f IOCTL_SERIAL_SET_BREAK_ON>).
        //
    case IOCTL_SERIAL_SET_BREAK_OFF :
        DEBUGMSG (ZONE_IOCTL, (TEXT(" IOCTL_SERIAL_SET_BREAK_OFF\r\n")));
        // A no-op for SERDMA
        break;

        // ****************************************************************
        //
        //      @func BOOL      | IOCTL_SERIAL_SET_DTR |
        //                              Device IO control routine to set DTR high.
        //
        //      @parm DWORD | dwOpenData | value returned from SERDMA_Open call
        //      @parm DWORD | dwCode | IOCTL_SERIAL_SET_DTR
        //      @parm PBYTE | pBufIn | Ignored
        //      @parm DWORD | dwLenIn | Ignored
        //      @parm PBYTE | pBufOut | Ignored
        //      @parm DWORD | dwLenOut | Ignored
        //      @parm PDWORD | pdwActualOut | Ignored
        //
        //      @rdesc          Returns TRUE for success, FALSE for failure (and
        //                              sets thread error code)
        //
        //      @xref           <f IOCTL_SERIAL_CLR_DTR>
        //
    case IOCTL_SERIAL_SET_DTR :
        DEBUGMSG (ZONE_IOCTL, (TEXT(" IOCTL_SERIAL_SET_DTR\r\n")));
        /* It's an error to call this if DCB uses DTR_CONTROL_HANDSHAKE.
         */
        if ( pHWIHead->DCB.fDtrControl != DTR_CONTROL_HANDSHAKE ) {
            // HWSetDTR is a no-op for SERDMA
        } else {
            SetLastError (ERROR_INVALID_PARAMETER);
            RetVal = FALSE;
        }
        break;

        // ****************************************************************
        //
        //      @func BOOL      | IOCTL_SERIAL_CLR_DTR |
        //                              Device IO control routine to set DTR low.
        //
        //      @parm DWORD | dwOpenData | value returned from SERDMA_Open call
        //      @parm DWORD | dwCode | IOCTL_SERIAL_CLR_DTR
        //      @parm PBYTE | pBufIn | Ignored
        //      @parm DWORD | dwLenIn | Ignored
        //      @parm PBYTE | pBufOut | Ignored
        //      @parm DWORD | dwLenOut | Ignored
        //      @parm PDWORD | pdwActualOut | Ignored
        //
        //      @rdesc          Returns TRUE for success, FALSE for failure (and
        //                              sets thread error code)
        //
        //      @xref           <f IOCTL_SERIAL_SET_DTR>
        //
    case IOCTL_SERIAL_CLR_DTR :
        DEBUGMSG (ZONE_IOCTL, (TEXT(" IOCTL_SERIAL_CLR_DTR\r\n")));
        /* It's an error to call this if DCB uses DTR_CONTROL_HANDSHAKE.
         */
        if ( pHWIHead->DCB.fDtrControl != DTR_CONTROL_HANDSHAKE ) {
            // HWClearDTR is a no-op for SERDMA
        } else {
            SetLastError (ERROR_INVALID_PARAMETER);
            RetVal = FALSE;
        }
        break;

        // ****************************************************************
        //
        //      @func   BOOL | IOCTL_SERIAL_SET_RTS |
        //                              Device IO control routine to set RTS high.
        //
        //      @parm DWORD | dwOpenData | value returned from SERDMA_Open call
        //      @parm DWORD | dwCode | IOCTL_SERIAL_SET_RTS
        //      @parm PBYTE | pBufIn | Ignored
        //      @parm DWORD | dwLenIn | Ignored
        //      @parm PBYTE | pBufOut | Ignored
        //      @parm DWORD | dwLenOut | Ignored
        //      @parm PDWORD | pdwActualOut | Ignored
        //
        //      @rdesc          Returns TRUE for success, FALSE for failure (and
        //                              sets thread error code)
        //
        //      @xref           <f IOCTL_SERIAL_CLR_RTS>
        //
    case IOCTL_SERIAL_SET_RTS :
        DEBUGMSG (ZONE_IOCTL, (TEXT(" IOCTL_SERIAL_SET_RTS\r\n")));
        /* It's an error to call this if DCB uses RTS_CONTROL_HANDSHAKE.
         */
        if ( pHWIHead->DCB.fRtsControl != RTS_CONTROL_HANDSHAKE ) {
            // HWSetRTS is a no-op for SERDMA
        } else {
            SetLastError (ERROR_INVALID_PARAMETER);
            RetVal = FALSE;
        }
        break;

        // ****************************************************************
        //
        //      @func   BOOL | IOCTL_SERIAL_CLR_RTS |
        //                              Device IO control routine to set RTS low.
        //
        //      @parm DWORD | dwOpenData | value returned from SERDMA_Open call
        //      @parm DWORD | dwCode | IOCTL_SERIAL_CLR_RTS
        //      @parm PBYTE | pBufIn | Ignored
        //      @parm DWORD | dwLenIn | Ignored
        //      @parm PBYTE | pBufOut | Ignored
        //      @parm DWORD | dwLenOut | Ignored
        //      @parm PDWORD | pdwActualOut | Ignored
        //
        //      @rdesc          Returns TRUE for success, FALSE for failure (and
        //                              sets thread error code)
        //
        //      @xref           <f IOCTL_SERIAL_SET_RTS>
        //
    case IOCTL_SERIAL_CLR_RTS :
        DEBUGMSG (ZONE_IOCTL, (TEXT(" IOCTL_SERIAL_CLR_RTS\r\n")));
        /* It's an error to call this if DCB uses RTS_CONTROL_HANDSHAKE.
         */
        if ( pHWIHead->DCB.fRtsControl != RTS_CONTROL_HANDSHAKE ) {
            // HWClearRTS is a no-op for SERDMA
        } else {
            SetLastError (ERROR_INVALID_PARAMETER);
            RetVal = FALSE;
        }
        break;

        // ****************************************************************
        //
        //      @func BOOL      | IOCTL_SERIAL_SET_XOFF |
        //                              Device IO control routine to cause transmission
        //                              to act as if an XOFF character has been received.
        //
        //      @parm DWORD | dwOpenData | value returned from SERDMA_Open call
        //      @parm DWORD | dwCode | IOCTL_SERIAL_SET_XOFF
        //      @parm PBYTE | pBufIn | Ignored
        //      @parm DWORD | dwLenIn | Ignored
        //      @parm PBYTE | pBufOut | Ignored
        //      @parm DWORD | dwLenOut | Ignored
        //      @parm PDWORD | pdwActualOut | Ignored
        //
        //      @rdesc          Returns TRUE for success, FALSE for failure (and
        //                              sets thread error code)
        //
        //      @xref           <f IOCTL_SERIAL_SET_XON>
        //
    case IOCTL_SERIAL_SET_XOFF :
        DEBUGMSG (ZONE_IOCTL|ZONE_FLOW, (TEXT(" IOCTL_SERIAL_SET_XOFF\r\n")));
        // This is a no-op for SERDMA
        break;

        // ****************************************************************
        //
        //      @func BOOL      | IOCTL_SERIAL_SET_XON |
        //                              Device IO control routine to cause transmission
        //                              to act as if an XON character has been received.
        //
        //      @parm DWORD | dwOpenData | value returned from SERDMA_Open call
        //      @parm DWORD | dwCode | IOCTL_SERIAL_SET_XON
        //      @parm PBYTE | pBufIn | Ignored
        //      @parm DWORD | dwLenIn | Ignored
        //      @parm PBYTE | pBufOut | Ignored
        //      @parm DWORD | dwLenOut | Ignored
        //      @parm PDWORD | pdwActualOut | Ignored
        //
        //      @rdesc          Returns TRUE for success, FALSE for failure (and
        //                              sets thread error code)
        //
        //      @xref           <f IOCTL_SERIAL_SET_XOFF>
        //
    case IOCTL_SERIAL_SET_XON :
        DEBUGMSG (ZONE_IOCTL|ZONE_FLOW, (TEXT(" IOCTL_SERIAL_SET_XON\r\n")));
        // This is a no-op for SERDMA
        break;

        // ****************************************************************
        //
        //      @func           BOOL | IOCTL_SERIAL_GET_WAIT_MASK |
        //                              Device IO control routine to retrieve the value
        //                              of the event mask.
        //
        //      @parm DWORD | dwOpenData | value returned from SERDMA_Open call
        //      @parm DWORD | dwCode | IOCTL_SERIAL_GET_WAIT_MASK
        //      @parm PBYTE | pBufIn | Ignored
        //      @parm DWORD | dwLenIn | Ignored
        //      @parm PBYTE | pBufOut | Points to DWORD to place event mask
        //      @parm DWORD | dwLenOut | should be sizeof(DWORD) or larger
        //      @parm PDWORD | pdwActualOut | Points to DWORD to return length
        //                              of returned data (should be set to sizeof(DWORD) if no
        //                              error)
        //
        //      @rdesc          Returns TRUE for success, FALSE for failure (and
        //                              sets thread error code)
        //
        //      @xref           <f IOCTL_SERIAL_SET_WAIT_MASK>
        //                              <f IOCTL_SERIAL_WAIT_ON_MASK>
        //
    case IOCTL_SERIAL_GET_WAIT_MASK :
        DEBUGMSG (ZONE_IOCTL, (TEXT(" IOCTL_SERIAL_GET_WAIT_MASK\r\n")));
        if ( (dwLenOut < sizeof(DWORD)) || (NULL == pBufOut) ||
             (NULL == pdwActualOut) ) {
            SetLastError (ERROR_INVALID_PARAMETER);
            RetVal = FALSE;
            DEBUGMSG (ZONE_IOCTL, (TEXT(" Invalid parameter\r\n")));
            break;
        }

        // Set The Wait Mask
        *(DWORD *)pBufOut = pOpenHead->CommEvents.fEventMask;

        // Return the size
        *pdwActualOut = sizeof(DWORD);

        break;

        // ****************************************************************
        //
        //      @func BOOL      | IOCTL_SERIAL_SET_WAIT_MASK |
        //                              Device IO control routine to set the value
        //                              of the event mask.
        //
        //      @parm DWORD | dwOpenData | value returned from SERDMA_Open call
        //      @parm DWORD | dwCode | IOCTL_SERIAL_SET_WAIT_MASK
        //      @parm PBYTE | pBufIn | Pointer to the DWORD mask value
        //      @parm DWORD | dwLenIn | should be sizeof(DWORD)
        //      @parm PBYTE | pBufOut | Ignored
        //      @parm DWORD | dwLenOut | Ignored
        //      @parm PDWORD | pdwActualOut | Ignored
        //
        //      @rdesc          Returns TRUE for success, FALSE for failure (and
        //                              sets thread error code)
        //
        //      @xref           <f IOCTL_SERIAL_GET_WAIT_MASK>
        //                              <f IOCTL_SERIAL_WAIT_ON_MASK>
        //
    case IOCTL_SERIAL_SET_WAIT_MASK :
        if ( (dwLenIn < sizeof(DWORD)) || (NULL == pBufIn) ) {
            SetLastError (ERROR_INVALID_PARAMETER);
            RetVal = FALSE;
            DEBUGMSG (ZONE_IOCTL, (TEXT(" Invalid parameter\r\n")));
            break;
        }
        DEBUGMSG (ZONE_IOCTL, (TEXT(" IOCTL_SERIAL_SET_WAIT_MASK 0x%X\r\n"),
                               *(DWORD *)pBufIn));

        EnterCriticalSection(&(pOpenHead->CommEvents.EventCS));

        // OK, now we can actually set the mask
        pOpenHead->CommEvents.fEventMask = *(DWORD *)pBufIn;

        // NOTE: If there is an outstanding WaitCommEvent, it should
        // return an error when SET_WAIT_MASK is called.  We accomplish
        // this by generating an hCommEvent which will wake the WaitComm
        // and subsequently return error (since no event bits will be set )
        pOpenHead->CommEvents.fAbort = 1;
        SetEvent(pOpenHead->CommEvents.hCommEvent);

        // And calculate the OR of all masks for this port.  Use a temp
        // variable so that the other threads don't see a partial mask
        dwFlags = 0;
        pEntry = pHWIHead->OpenList.Flink;
        while ( pEntry != &pHWIHead->OpenList ) {
            PHW_OPEN_INFO   pTmpOpenHead;

            pTmpOpenHead = CONTAINING_RECORD( pEntry, HW_OPEN_INFO, llist);
            pEntry = pEntry->Flink;    // advance to next

            DEBUGMSG (ZONE_EVENTS, (TEXT(" SetWaitMask - handle x%X mask x%X\r\n"),
                                    pTmpOpenHead, pTmpOpenHead->CommEvents.fEventMask));
            dwFlags |= pTmpOpenHead->CommEvents.fEventMask;
        }
        pHWIHead->fEventMask = dwFlags;
        LeaveCriticalSection(&(pOpenHead->CommEvents.EventCS));
        DEBUGMSG (ZONE_EVENTS, (TEXT(" SetWaitMask - mask x%X, global mask x%X\r\n"),
                                pOpenHead->CommEvents.fEventMask, pHWIHead->fEventMask));


        break;

        // ****************************************************************
        //
        //      @func BOOL      | IOCTL_SERIAL_WAIT_ON_MASK |
        //                              Device IO control routine to wait for a communications
        //                              event that matches one in the event mask
        //
        //      @parm DWORD | dwOpenData | value returned from SERDMA_Open call
        //      @parm DWORD | dwCode | IOCTL_SERIAL_WAIT_ON_MASK
        //      @parm PBYTE | pBufIn | Ignored
        //      @parm DWORD | dwLenIn | Ignored
        //      @parm PBYTE | pBufOut | Points to DWORD to place event mask.
        //                              The returned mask will show the event that terminated
        //                              the wait.  If a process attempts to change the device
        //                              handle's event mask by using the IOCTL_SERIAL_SET_WAIT_MASK
        //                              call the driver should return immediately with (DWORD)0 as
        //                              the returned event mask.
        //      @parm DWORD | dwLenOut | should be sizeof(DWORD) or larger
        //      @parm PDWORD | pdwActualOut | Points to DWORD to return length
        //                              of returned data (should be set to sizeof(DWORD) if no
        //                              error)
        //
        //      @rdesc          Returns TRUE for success, FALSE for failure (and
        //                              sets thread error code)
        //
        //      @xref           <f IOCTL_SERIAL_GET_WAIT_MASK>
        //                              <f IOCTL_SERIAL_SET_WAIT_MASK>
        //
    case IOCTL_SERIAL_WAIT_ON_MASK :
        DEBUGMSG (ZONE_IOCTL, (TEXT(" IOCTL_SERIAL_WAIT_ON_MASK\r\n")));
        if ( (dwLenOut < sizeof(DWORD)) || (NULL == pBufOut) ||
             (NULL == pdwActualOut) ) {
            SetLastError (ERROR_INVALID_PARAMETER);
            RetVal = FALSE;
            DEBUGMSG (ZONE_IOCTL, (TEXT(" Invalid parameter\r\n")));
            break;
        }

        if ( WaitCommEvent(pOpenHead, (DWORD *)pBufOut, NULL) == FALSE ) {
            // Device may have been closed or removed while we were waiting
            DEBUGMSG (ZONE_IOCTL|ZONE_ERROR,
                      (TEXT(" SERDMA_IOControl - Error in WaitCommEvent\r\n")));
            RetVal = FALSE;
        }
        // Return the size
        *pdwActualOut = sizeof(DWORD);
        break;

        // ****************************************************************
        //
        //      @func BOOL      | IOCTL_SERIAL_GET_COMMSTATUS |
        //                              Device IO control routine to clear any pending
        //                              communications errors and return the current communication
        //                              status.
        //
        //      @parm DWORD | dwOpenData | value returned from SERDMA_Open call
        //      @parm DWORD | dwCode | IOCTL_SERIAL_GET_COMMSTATUS
        //      @parm PBYTE | pBufIn | Ignored
        //      @parm DWORD | dwLenIn | Ignored
        //      @parm PBYTE | pBufOut | Points to a <f SERIAL_DEV_STATUS>
        //                              structure for the returned status information
        //      @parm DWORD | dwLenOut | should be sizeof(SERIAL_DEV_STATUS)
        //                              or larger
        //      @parm PDWORD | pdwActualOut | Points to DWORD to return length
        //                              of returned data (should be set to
        //                              sizeof(SERIAL_DEV_STATUS) if no error)
        //
        //      @rdesc          Returns TRUE for success, FALSE for failure (and
        //                              sets thread error code)
        //
        //
    case IOCTL_SERIAL_GET_COMMSTATUS :
        {
            PSERIAL_DEV_STATUS pSerialDevStat;

            DEBUGMSG (ZONE_IOCTL, (TEXT(" IOCTL_SERIAL_GET_COMMSTATUS\r\n")));
            if ( (dwLenOut < sizeof(SERIAL_DEV_STATUS)) || (NULL == pBufOut) ||
                 (NULL == pdwActualOut) ) {
                SetLastError (ERROR_INVALID_PARAMETER);
                RetVal = FALSE;
                DEBUGMSG (ZONE_IOCTL, (TEXT(" Invalid parameter\r\n")));
                break;
            }

            pSerialDevStat = (PSERIAL_DEV_STATUS)pBufOut;

            // Set The Error Mask
            pSerialDevStat->Errors = 0;

            // Clear the ComStat structure & get PDD related status
            memset ((char *) &(pSerialDevStat->ComStat), 0, sizeof(COMSTAT));
            ((PSERIAL_DEV_STATUS)pBufOut)->Errors =
            pFuncTbl->HWGetStatus(pHWHead, &(pSerialDevStat->ComStat));

            // PDD set fCtsHold, fDsrHold, fRLSDHold, and fTXim.  The MDD then
            // needs to set fXoffHold, fXoffSent, cbInQue, and cbOutQue.
            // For SERDMA, they are all zeroes.

            // Return the size
            *pdwActualOut = sizeof(SERIAL_DEV_STATUS);

        }

        break;

        // ****************************************************************
        //
        //      @func BOOL      | IOCTL_SERIAL_GET_MODEMSTATUS |
        //                              Device IO control routine to retrieve current
        //                              modem control-register values
        //
        //      @parm DWORD | dwOpenData | value returned from SERDMA_Open call
        //      @parm DWORD | dwCode | IOCTL_SERIAL_GET_MODEMSTATUS
        //      @parm PBYTE | pBufIn | Ignored
        //      @parm DWORD | dwLenIn | Ignored
        //      @parm PBYTE | pBufOut | Points to a DWORD for the returned
        //                              modem status information
        //      @parm DWORD | dwLenOut | should be sizeof(DWORD)
        //                              or larger
        //      @parm PDWORD | pdwActualOut | Points to DWORD to return length
        //                              of returned data (should be set to sizeof(DWORD)
        //                              if no error)
        //
        //      @rdesc          Returns TRUE for success, FALSE for failure (and
        //                              sets thread error code)
        //
        //
    case IOCTL_SERIAL_GET_MODEMSTATUS :
        DEBUGMSG (ZONE_IOCTL, (TEXT(" IOCTL_SERIAL_GET_MODEMSTATUS\r\n")));
        if ( (dwLenOut < sizeof(DWORD)) || (NULL == pBufOut) ||
             (NULL == pdwActualOut) ) {
            SetLastError (ERROR_INVALID_PARAMETER);
            RetVal = FALSE;
            DEBUGMSG (ZONE_IOCTL, (TEXT(" Invalid parameter\r\n")));
            break;
        }

        // Set the Modem Status dword
        *(DWORD *)pBufOut = 0;

        pFuncTbl->HWGetModemStatus(pHWHead, (PULONG)pBufOut);

        // Return the size
        *pdwActualOut = sizeof(DWORD);
        break;

        // ****************************************************************
        //
        //      @func BOOL      | IOCTL_SERIAL_GET_PROPERTIES |
        //                              Device IO control routine to retrieve information
        //                              about the communications properties for the device.
        //
        //      @parm DWORD | dwOpenData | value returned from SERDMA_Open call
        //      @parm DWORD | dwCode | IOCTL_SERIAL_GET_PROPERTIES
        //      @parm PBYTE | pBufIn | Ignored
        //      @parm DWORD | dwLenIn | Ignored
        //      @parm PBYTE | pBufOut | Points to a <f COMMPROP> structure
        //                              for the returned information.
        //      @parm DWORD | dwLenOut | should be sizeof(COMMPROP)
        //                              or larger
        //      @parm PDWORD | pdwActualOut | Points to DWORD to return length
        //                              of returned data (should be set to sizeof(COMMPROP)
        //                              if no error)
        //
        //      @rdesc          Returns TRUE for success, FALSE for failure (and
        //                              sets thread error code)
        //
        //
    case IOCTL_SERIAL_GET_PROPERTIES :
        DEBUGMSG (ZONE_IOCTL, (TEXT(" IOCTL_SERIAL_GET_PROPERTIES\r\n")));
        if ( (dwLenOut < sizeof(COMMPROP)) || (NULL == pBufOut) ||
             (NULL == pdwActualOut) ) {
            SetLastError (ERROR_INVALID_PARAMETER);
            RetVal = FALSE;
            DEBUGMSG (ZONE_IOCTL, (TEXT(" Invalid parameter\r\n")));
            break;
        }

        // Clear the ComMProp structure
        memset ((char *) ((COMMPROP *)pBufOut), 0,
                sizeof(COMMPROP));

        pFuncTbl->HWGetCommProperties(pHWHead, (LPCOMMPROP)pBufOut);

        // Return the size
        *pdwActualOut = sizeof(COMMPROP);
        break;

        // ****************************************************************
        //
        //      @func BOOL      | IOCTL_SERIAL_SET_TIMEOUTS |
        //                              Device IO control routine to set the time-out parameters
        //                              for all read and write operations on a specified
        //                              communications device
        //
        //      @parm DWORD | dwOpenData | value returned from SERDMA_Open call
        //      @parm DWORD | dwCode | IOCTL_SERIAL_SET_TIMEOUTS
        //      @parm PBYTE | pBufIn | Pointer to the <f COMMTIMEOUTS> structure
        //      @parm DWORD | dwLenIn | should be sizeof(COMMTIMEOUTS)
        //      @parm PBYTE | pBufOut | Ignored
        //      @parm DWORD | dwLenOut | Ignored
        //      @parm PDWORD | pdwActualOut | Ignored
        //
        //      @rdesc          Returns TRUE for success, FALSE for failure (and
        //                              sets thread error code)
        //
        //      @xref           <f IOCTL_SERIAL_GET_TIMEOUTS>
        //
    case IOCTL_SERIAL_SET_TIMEOUTS :
        if ( (dwLenIn < sizeof(COMMTIMEOUTS)) || (NULL == pBufIn) ) {
            SetLastError (ERROR_INVALID_PARAMETER);
            RetVal = FALSE;
            DEBUGMSG (ZONE_IOCTL, (TEXT(" Invalid parameter\r\n")));
            break;
        }
        DEBUGMSG (ZONE_IOCTL,
                  (TEXT(" IOCTL_SERIAL_SET_COMMTIMEOUTS (%d,%d,%d,%d,%d)\r\n"),
                   ((COMMTIMEOUTS *)pBufIn)->ReadIntervalTimeout,
                   ((COMMTIMEOUTS *)pBufIn)->ReadTotalTimeoutMultiplier,
                   ((COMMTIMEOUTS *)pBufIn)->ReadTotalTimeoutConstant,
                   ((COMMTIMEOUTS *)pBufIn)->WriteTotalTimeoutMultiplier,
                   ((COMMTIMEOUTS *)pBufIn)->WriteTotalTimeoutConstant));

        pHWIHead->CommTimeouts.ReadIntervalTimeout =
        ((COMMTIMEOUTS *)pBufIn)->ReadIntervalTimeout;
        pHWIHead->CommTimeouts.ReadTotalTimeoutMultiplier =
        ((COMMTIMEOUTS *)pBufIn)->ReadTotalTimeoutMultiplier;
        pHWIHead->CommTimeouts.ReadTotalTimeoutConstant =
        ((COMMTIMEOUTS *)pBufIn)->ReadTotalTimeoutConstant;
        pHWIHead->CommTimeouts.WriteTotalTimeoutMultiplier =
        ((COMMTIMEOUTS *)pBufIn)->WriteTotalTimeoutMultiplier;
        pHWIHead->CommTimeouts.WriteTotalTimeoutConstant =
        ((COMMTIMEOUTS *)pBufIn)->WriteTotalTimeoutConstant;

        pFuncTbl->HWSetCommTimeouts(pHWHead, (COMMTIMEOUTS *)pBufIn);
        break;

        // ****************************************************************
        //
        //      @func BOOL      | IOCTL_SERIAL_GET_TIMEOUTS |
        //                              Device IO control routine to get the time-out parameters
        //                              for all read and write operations on a specified
        //                              communications device
        //
        //      @parm DWORD | dwOpenData | value returned from SERDMA_Open call
        //      @parm DWORD | dwCode | IOCTL_SERIAL_GET_TIMEOUTS
        //      @parm PBYTE | pBufIn | Ignored
        //      @parm DWORD | dwLenIn | Ignored
        //      @parm PBYTE | pBufOut | Pointer to a <f COMMTIMEOUTS> structure
        //                              for the returned data
        //      @parm DWORD | dwLenOut | should be sizeof(COMMTIMEOUTS)
        //      @parm PDWORD | pdwActualOut | Points to DWORD to return length
        //                              of returned data (should be set to sizeof(COMMTIMEOUTS)
        //                              if no error)
        //
        //      @rdesc          Returns TRUE for success, FALSE for failure (and
        //                              sets thread error code)
        //
        //      @xref           <f IOCTL_SERIAL_GET_TIMEOUTS>
        //
    case IOCTL_SERIAL_GET_TIMEOUTS :
        DEBUGMSG (ZONE_IOCTL, (TEXT(" IOCTL_SERIAL_GET_TIMEOUTS\r\n")));
        if ( (dwLenOut < sizeof(COMMTIMEOUTS)) || (NULL == pBufOut) ||
             (NULL == pdwActualOut) ) {
            SetLastError (ERROR_INVALID_PARAMETER);
            RetVal = FALSE;
            DEBUGMSG (ZONE_IOCTL, (TEXT(" Invalid parameter\r\n")));
            break;
        }

        // Clear the structure
        memset ((char *) ((COMMTIMEOUTS *)pBufOut), 0,
                sizeof(COMMTIMEOUTS));

        memcpy((LPCOMMTIMEOUTS)pBufOut, &(pHWIHead->CommTimeouts),
               sizeof(COMMTIMEOUTS));

        // Return the size
        *pdwActualOut = sizeof(COMMTIMEOUTS);
        break;

        // ****************************************************************
        //
        //      @func BOOL      | IOCTL_SERIAL_PURGE |
        //                              Device IO control routine to discard characters from the
        //                              output or input buffer of a specified communications
        //                              resource.  It can also terminate pending read or write
        //                              operations on the resource
        //
        //      @parm DWORD | dwOpenData | value returned from SERDMA_Open call
        //      @parm DWORD | dwCode | IOCTL_SERIAL_PURGE
        //      @parm PBYTE | pBufIn | Pointer to a DWORD containing the action
        //      @parm DWORD | dwLenIn | Should be sizeof(DWORD)
        //      @parm PBYTE | pBufOut | Ignored
        //      @parm DWORD | dwLenOut | Ignored
        //      @parm PDWORD | pdwActualOut | Ignored
        //
        //      @rdesc          Returns TRUE for success, FALSE for failure (and
        //                              sets thread error code)
        //
        //
    case IOCTL_SERIAL_PURGE :
        if ( (dwLenIn < sizeof(DWORD)) || (NULL == pBufIn) ) {
            SetLastError (ERROR_INVALID_PARAMETER);
            RetVal = FALSE;
            DEBUGMSG (ZONE_IOCTL, (TEXT(" Invalid parameter\r\n")));
            break;
        }
        dwFlags = *((PDWORD) pBufIn);

        DEBUGMSG (ZONE_IOCTL, (TEXT(" IOCTL_SERIAL_PURGE 0x%X\r\n"),dwFlags));

        // HWPurgeComm is a no-op for SERDMA

        // Now, free up any threads blocked in MDD. Reads and writes are in
        // loops, so they also need a flag to tell them to abort.
        if ( dwFlags & PURGE_RXABORT ) {
            pHWIHead->fAbortRead = 1;
            PulseEvent(pHWIHead->hReadEvent);
        }

        break;

        // ****************************************************************
        //
        //      @func BOOL      | IOCTL_SERIAL_SET_QUEUE_SIZE |
        //                              Device IO control routine to set the queue sizes of of a
        //                              communications device
        //
        //      @parm DWORD | dwOpenData | value returned from SERDMA_Open call
        //      @parm DWORD | dwCode | IOCTL_SERIAL_SET_QUEUE_SIZE
        //      @parm PBYTE | pBufIn | Pointer to a <f SERIAL_QUEUE_SIZES>
        //                              structure
        //      @parm DWORD | dwLenIn | should be sizeof(<f SERIAL_QUEUE_SIZES>)
        //      @parm PBYTE | pBufOut | Ignored
        //      @parm DWORD | dwLenOut | Ignored
        //      @parm PDWORD | pdwActualOut | Ignored
        //
        //      @rdesc          Returns TRUE for success, FALSE for failure (and
        //                              sets thread error code)
        //
        //
    case IOCTL_SERIAL_SET_QUEUE_SIZE :
        if ( (dwLenIn < sizeof(SERIAL_QUEUE_SIZES)) || (NULL == pBufIn) ) {
            SetLastError (ERROR_INVALID_PARAMETER);
            RetVal = FALSE;
            DEBUGMSG (ZONE_IOCTL, (TEXT(" Invalid parameter\r\n")));
            break;
        }
        DEBUGMSG (ZONE_IOCTL,
                  (TEXT(" IOCTL_SERIAL_SET_QUEUE_SIZE (%d,%d,%d,%d,%d)\r\n"),
                   ((SERIAL_QUEUE_SIZES *)pBufIn)->cbInQueue,
                   ((SERIAL_QUEUE_SIZES *)pBufIn)->cbOutQueue));

        // No-op for SERDMA, since the real queue is in the emulator

        break;

        // ****************************************************************
        //
        //      @func BOOL      | IOCTL_SERIAL_IMMEDIATE_CHAR |
        //                              Device IO control routine to transmit a specified character
        //                              ahead of any pending data in the output buffer of the
        //                              communications device
        //
        //      @parm DWORD | dwOpenData | value returned from SERDMA_Open call
        //      @parm DWORD | dwCode | IOCTL_SERIAL_IMMEDIATE_CHAR
        //      @parm PBYTE | pBufIn | Pointer to a UCHAR to send
        //      @parm DWORD | dwLenIn | should be sizeof(UCHAR)
        //      @parm PBYTE | pBufOut | Ignored
        //      @parm DWORD | dwLenOut | Ignored
        //      @parm PDWORD | pdwActualOut | Ignored
        //
        //      @rdesc          Returns TRUE for success, FALSE for failure (and
        //                              sets thread error code)
        //
        //
    case IOCTL_SERIAL_IMMEDIATE_CHAR :
        if ( (dwLenIn < sizeof(UCHAR)) || (NULL == pBufIn) ) {
            SetLastError (ERROR_INVALID_PARAMETER);
            RetVal = FALSE;
            DEBUGMSG (ZONE_IOCTL, (TEXT(" Invalid parameter\r\n")));
            break;
        }
        DEBUGMSG (ZONE_IOCTL, (TEXT(" IOCTL_SERIAL_IMMEDIATE_CHAR 0x%X\r\n"),
                               (UCHAR *)pBufIn));
        // XmitComChar is not supported on SERDMA
        SetLastError(ERROR_INVALID_FUNCTION);
        RetVal = FALSE;
        break;

        // ****************************************************************
        //
        //      @func BOOL      | IOCTL_SERIAL_GET_DCB |
        //                              Device IO control routine to get the device-control
        //                              block from a specified communications device
        //
        //      @parm DWORD | dwOpenData | value returned from SERDMA_Open call
        //      @parm DWORD | dwCode | IOCTL_SERIAL_GET_DCB
        //      @parm PBYTE | pBufIn | Ignored
        //      @parm DWORD | dwLenIn | Ignored
        //      @parm PBYTE | pBufOut | Pointer to a <f DCB> structure
        //      @parm DWORD | dwLenOut | Should be sizeof(<f DCB>)
        //      @parm PDWORD | pdwActualOut | Pointer to DWORD to return length
        //                              of returned data (should be set to sizeof(<f DCB>) if
        //                              no error)
        //
        //      @rdesc          Returns TRUE for success, FALSE for failure (and
        //                              sets thread error code)
        //
        //
    case IOCTL_SERIAL_GET_DCB :
        DEBUGMSG (ZONE_IOCTL, (TEXT(" IOCTL_SERIAL_GET_DCB\r\n")));
        if ( (dwLenOut < sizeof(DCB)) || (NULL == pBufOut) ||
             (NULL == pdwActualOut) ) {
            SetLastError (ERROR_INVALID_PARAMETER);
            RetVal = FALSE;
            DEBUGMSG (ZONE_IOCTL, (TEXT(" Invalid parameter\r\n")));
            break;
        }

        memcpy((char *)pBufOut, (char *)&(pHWIHead->DCB), sizeof(DCB));

        // Return the size
        *pdwActualOut = sizeof(DCB);
        break;

        // ****************************************************************
        //
        //      @func BOOL      | IOCTL_SERIAL_SET_DCB |
        //                              Device IO control routine to set the device-control
        //                              block on a specified communications device
        //
        //      @parm DWORD | dwOpenData | value returned from SERDMA_Open call
        //      @parm DWORD | dwCode | IOCTL_SERIAL_SET_DCB
        //      @parm PBYTE | pBufIn | Pointer to a <f DCB> structure
        //      @parm DWORD | dwLenIn | should be sizeof(<f DCB>)
        //      @parm PBYTE | pBufOut | Ignored
        //      @parm DWORD | dwLenOut | Ignored
        //      @parm PDWORD | pdwActualOut | Ignored
        //
        //      @rdesc          Returns TRUE for success, FALSE for failure (and
        //                              sets thread error code)
        //
        //
    case IOCTL_SERIAL_SET_DCB :
        if ( (dwLenIn < sizeof(DCB)) || (NULL == pBufIn) ) {
            SetLastError (ERROR_INVALID_PARAMETER);
            RetVal = FALSE;
            DEBUGMSG (ZONE_IOCTL, (TEXT(" Invalid parameter\r\n")));
            break;
        }
        DEBUGMSG (ZONE_IOCTL, (TEXT(" IOCTL_SERIAL_SET_DCB\r\n")));

        if ( !ApplyDCB (pHWIHead, (DCB *)pBufIn, TRUE) ) {
            //
            // Most likely an unsupported baud rate was specified
            //
            SetLastError (ERROR_INVALID_PARAMETER);
            RetVal = FALSE;
            DEBUGMSG (ZONE_IOCTL, (TEXT(" ApplyDCB failed\r\n")));
            break;
        }

        break;

        // ****************************************************************
        //
        //      @func BOOL      | IOCTL_SERIAL_ENABLE_IR |
        //                              Device IO control routine to set the device-control
        //                              block on a specified communications device
        //
        //      @parm DWORD | dwOpenData | value returned from SERDMA_Open call
        //      @parm DWORD | dwCode | IOCTL_SERIAL_ENABLE_IR
        //      @parm PBYTE | pBufIn | Ignored
        //      @parm DWORD | dwLenIn | Ignored
        //      @parm PBYTE | pBufOut | Ignored
        //      @parm DWORD | dwLenOut | Ignored
        //      @parm PDWORD | pdwActualOut | Ignored
        //
        //      @rdesc          Returns TRUE for success, FALSE for failure (and
        //                              sets thread error code)
        //
        //
    case IOCTL_SERIAL_ENABLE_IR :
        DEBUGMSG (ZONE_IR,
                  (TEXT("IOCTL Enable IR\r\n")));
        // IR mode not supported on SERDMA
        SetLastError(ERROR_INVALID_FUNCTION);
        RetVal = FALSE;

        break;

        // ****************************************************************
        //
        //      @func BOOL      | IOCTL_SERIAL_DISABLE_IR |
        //                              Device IO control routine to set the device-control
        //                              block on a specified communications device
        //
        //      @parm DWORD | dwOpenData | value returned from SERDMA_Open call
        //      @parm DWORD | dwCode | IOCTL_SERIAL_DISABLE_IR
        //      @parm PBYTE | pBufIn | Ignored
        //      @parm DWORD | dwLenIn | Ignored
        //      @parm PBYTE | pBufOut | Ignored
        //      @parm DWORD | dwLenOut | Ignored
        //      @parm PDWORD | pdwActualOut | Ignored
        //
        //      @rdesc          Returns TRUE for success, FALSE for failure (and
        //                              sets thread error code)
        //
        //
    case IOCTL_SERIAL_DISABLE_IR :
        DEBUGMSG (ZONE_IR,
                  (TEXT("IOCTL Disable IR\r\n")));
        // A no-op for SERDMA
        break;


    case IOCTL_POWER_CAPABILITIES:
        if (!pBufOut || dwLenOut < sizeof(POWER_CAPABILITIES) || !pdwActualOut) {
            SetLastError(ERROR_INVALID_PARAMETER);
            RetVal = FALSE;
        } else {
            PPOWER_CAPABILITIES ppc = (PPOWER_CAPABILITIES)pBufOut;

            memset(ppc, 0, sizeof(POWER_CAPABILITIES));

            // TODO: get these from the PDD.
            ppc->DeviceDx = 0x11;    // D0 & D4

            // supported power & latency info
            ppc->Power[D0] = PwrDeviceUnspecified;     // mW
            ppc->Power[D4] = PwrDeviceUnspecified;
            ppc->Latency[D0] = 0;   // mSec
            ppc->Latency[D4] = 0;

            ppc->WakeFromDx = 0;     // no device wake
            ppc->InrushDx = 0;       // no inrush

            *pdwActualOut = sizeof(POWER_CAPABILITIES);

            RetVal = TRUE;
        }
        break;


    case IOCTL_POWER_QUERY:
        if (!pBufOut || dwLenOut < sizeof(CEDEVICE_POWER_STATE) || !pdwActualOut) {
            SetLastError(ERROR_INVALID_PARAMETER);
            RetVal = FALSE;
        } else {
            CEDEVICE_POWER_STATE Dx = *(PCEDEVICE_POWER_STATE)pBufOut;
            DEBUGMSG(1, (TEXT("COM: IOCTL_POWER_QUERY: D%d\r\n"), Dx));
            // return PwrDeviceUnspecified if the device needs to reject the query
            *pdwActualOut = sizeof(CEDEVICE_POWER_STATE);
            RetVal = TRUE;
        }
        break;


    case IOCTL_POWER_SET:
        if (!pBufOut || dwLenOut < sizeof(CEDEVICE_POWER_STATE) || !pdwActualOut) {
            SetLastError(ERROR_INVALID_PARAMETER);
            RetVal = FALSE;
        } else {
            CEDEVICE_POWER_STATE Dx = *(PCEDEVICE_POWER_STATE)pBufOut;

            DEBUGMSG(1, (TEXT("COM: IOCTL_POWER_SET: D%d\r\n"), Dx));

            if (Dx == D0)
                SERDMA_PowerUp(pHWIHead);
            else {
                SERDMA_PowerDown(pHWIHead);
                *(PCEDEVICE_POWER_STATE)pBufOut = D4;
            }

            *pdwActualOut = sizeof(Dx);
            RetVal = TRUE;
        }
        break;


    default :
        // Pass IOCTL through to PDD if hook is provided
        if ( (pFuncTbl->HWIoctl == NULL) ||
             (pFuncTbl->HWIoctl(pHWHead,dwCode,pBufIn,dwLenIn,pBufOut,dwLenOut,
                                pdwActualOut) == FALSE) ) {
            SetLastError (ERROR_INVALID_PARAMETER);
            RetVal = FALSE;
            DEBUGMSG (ZONE_IOCTL, (TEXT(" Invalid ioctl 0x%X\r\n"), dwCode));
        }
        break;
    }

    COM_DEC_USAGE_CNT(pOpenHead);

    DEBUGMSG (ZONE_IOCTL|ZONE_FUNCTION|(ZONE_ERROR && (RetVal == FALSE)),
              (TEXT("-SERDMA_IOControl %s Ecode=%d (len=%d)\r\n"),
               (RetVal == TRUE) ? TEXT("Success") : TEXT("Error"),
               GetLastError(), (NULL == pdwActualOut) ? 0 : *pdwActualOut));

    return RetVal;
}


