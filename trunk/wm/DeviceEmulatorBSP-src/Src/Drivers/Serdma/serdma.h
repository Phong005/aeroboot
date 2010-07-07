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
        File:           SerDMA.h

        Contains:       Serial-over-DMA driver.

==================================================================*/

#ifndef SERDMA_H__
#define SERDMA_H__

#define DMA_PACKET_SIZE 4096

#define DEFAULT_CE_THREAD_PRIORITY 103


// We use a callback for serial events
typedef VOID (*EVENT_FUNC)(PVOID Arg1, ULONG Arg2);


typedef struct {
    // We have an event callback into the MDD
    EVENT_FUNC EventCallback; // This callback exists in MDD
    PVOID        pMddHead;  // This is the first parm to callback

    // Keep a copy of DCB since we rely on may of its parms
    DCB         dcb;        // @field Device Control Block (copy of DCB in MDD)
    // And the same thing applies for CommTimeouts
    COMMTIMEOUTS CommTimeouts;  // @field Copy of CommTimeouts structure

    // Misc fields used by ser16550 library
    ULONG OpenCount;	    // @field Count of simultaneous opens. 

    DWORD DMAChannelNumber;
    HANDLE           hDMAChannel;       // @field DMA device handle

    // Fields for detecting disconnect
    BOOL  DSRIsHigh;
    BYTE  ControlFlags;

} SERDMA_UART_INFO, * PSERDMA_UART_INFO;


typedef struct __SER_INFO {
    // Put lib struct first so we can easily cast pointers
    SERDMA_UART_INFO SERDMA_COM;   // UART H/W register

    // now hardware specific fields
    DWORD       dwDevIndex;     // @field Index of device

    UINT8       cOpenCount;     // @field Count of concurrent opens
    COMMPROP    CommProp;       // @field CommProp structure.
    PVOID       pMddHead;       // @field First arg to mdd callbacks.
    PHWOBJ      pHWObj;         // @field Pointer to PDDs HWObj structure
} SER_INFO, *PSER_INFO;


// Note:  this structure layout is shared with the desktop-side SERDMAASPlugin.DLL
typedef union {
    BYTE            PacketBuffer[DMA_PACKET_SIZE];
    struct {
        USHORT PacketLength;
        BYTE PacketContents[DMA_PACKET_SIZE-sizeof(USHORT)];
        } u;
} DMA_PACKET_BUFFER;


// Forward declare for use below
typedef struct __HWOBJ HWOBJ, *PHWOBJ;
typedef struct __HW_OPEN_INFO HW_OPEN_INFO, *PHW_OPEN_INFO;

// @struct	HW_INDEP_INFO | Hardware Independent Serial Driver Head Information.
typedef struct __HW_INDEP_INFO {
    CRITICAL_SECTION ReceiveCritSec1;   // @field Protects rx action
    PHWOBJ           pHWObj;            // @field Represents PDD object.
    PVOID            pHWHead;           // @field Device context for PDD.
    LIST_ENTRY       OpenList;          // @field Head of linked list of OPEN_INFOs    
    CRITICAL_SECTION OpenCS;            // @field Protects Open Linked List + ref counts

    ULONG            Priority256;       // @field CeThreadPriority of Dispatch Thread.
    HANDLE           pDispatchThread;   // @field ReceiveThread
    HANDLE           hKillDispatchThread; // set by the pDispatchThread when it knows it needs to exit
    HANDLE           hReadEvent;        // set when ReceivePacketBuffer contains a DMA packet, ready for COM_Read()
    HANDLE           hReceiveBufferEmpty; // set when the RxDataReady is 1, clear when it is zero
    HANDLE           hReceiveDataReady; // set by the DMA driver when data is ready for receive

    ULONG           fEventMask;         // @field Sum of event mask for all opens
    PHW_OPEN_INFO   pAccessOwner;       // @field Points to whichever open has acess permissions

    DCB             DCB;                // @field DCB (see Win32 Documentation)
    COMMTIMEOUTS    CommTimeouts;       // @field Time control field. 
    DWORD           OpenCnt;            // @field Protects use of this port 

    DWORD           KillRxThread:1;     // @field Flag to terminate SerialDispatch thread.
    DWORD           RxDataReady:1;      // @field 1 if ReceivePacketBuffer contains data, 0 if not
    DWORD           fAbortRead:1;       // @field Used for PURGE

    DMA_PACKET_BUFFER ReceivePacketBuffer;

} HW_INDEP_INFO, *PHW_INDEP_INFO;

//
// The serial event structure contains fields used to pass serial
// event information from the PDD to the MDD.  This information
// is passed via a callback, so the scope of this structure remains
// limited to the MDD.
//
typedef struct __COMM_EVENTS	{
	HANDLE  hCommEvent;			// @field Indicates serial events from PDD to MDD
	ULONG	fEventMask;			// @field Event Mask requestd by application 
	ULONG	fEventData;			// @field Event Mask Flag. 
    ULONG   fAbort;             // @field TRUE after SetCommMask( 0 )
	CRITICAL_SECTION EventCS;	    //  CommEvent access and related sign atom
	} COMM_EVENTS, *PCOMM_EVENTS;




// @struct	HW_OPEN_INFO | Info pertaining to each open instance of a device
typedef struct __HW_OPEN_INFO {
    PHW_INDEP_INFO  pSerialHead;    // @field Pointer back to our HW_INDEP_INFO
    DWORD	        AccessCode;     // @field What permissions was this opened with
    DWORD	        ShareMode;      // @field What Share Mode was this opened with
    DWORD           StructUsers;    // @field Count of threads currently using struct.
    COMM_EVENTS		CommEvents;		// @field Contains all info for serial event handling
    LIST_ENTRY      llist;          // @field Linked list of OPEN_INFOs
} HW_OPEN_INFO, *PHW_OPEN_INFO;


#endif // SERDMA_H__

