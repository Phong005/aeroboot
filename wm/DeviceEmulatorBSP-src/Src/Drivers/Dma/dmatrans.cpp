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
    File:		DMATrans.cpp
    
    Contains:	Stream-interface CE driver for the Emulator's
                DMATransportDevice. This driver provides a stream
                interface for applications running in the emulator
                to communicate with applications running on the
                host machine (outside the emulator).
                
                Communication on the other side of the device
                is done via the Emulator's IVirtualTransport COM
                interface.
                
    Written by:	Paul Pearcy
    
    Copyright:	2001 Connectix Corporation
==================================================================*/

//#include "stdafx.h"
#include <windows.h>        // For later builds of Talisker, remove the "stdafx.h" include and add this line
#include <bsp.h>
#include <devload.h> //for OpenDeviceKey
#include "dmatrans.h"
#include <ceddk.h> // for READ_PORT_XXXX, WRITE_PORT_XXXX

/*------------------------------------------------------------------
    Defines and const
------------------------------------------------------------------*/

// These two configuration options are quite important. The
// Emulator's DMATransportDevice is the device this driver
// communicates with. The device can use either IRQ 14 or 15.
// The Emulator KITL is normally configured to use IRQ 14, so
// this driver must use IRQ 15 to avoid a conflict with the KITL.
// The Base address is the physical memory address (in the
// emulated machine) that we'll use to DMA packets to and from.
// The address here must match the address in the platform's
// Config.bib file (which is where we reserve physical
// memory for the DMATransportDevice.)

// IRQ
ULONG   kDMATransport_IRQ;

// Global
const ULONG kDMATrasportHandshake           = 1 << 0;
const ULONG kDMATransportInterruptEnabled   = 1 << 8;

// flags
const ULONG kDMATransportDataReady          = 1 << 0;

// channel base address on the bus
#define kDMATransportBase               (vInputBuffer+0x2080)

// Channel size
const ULONG kDMATransportChannelSize        = 0x20;

// DMATransportDevice register offsets.
enum
{
    kDMATransportGlobalRegister         = 0x00,
    kDMATransportFlagsRegister          = 0x04,
    kDMATransportIOInputRegister        = 0x08,
    kDMATransportIOOutputRegister       = 0x0C,
    kDMATransportIRQRegister            = 0x10,
    kDMATransportIRQAcknowledgeRegister	= 0x14,
};

// Regisgter macros, given the channel returns specific register
#define DMA_GLOBAL_REG(a)               ((ULONG*)(kDMATransportBase+(a*kDMATransportChannelSize)+kDMATransportGlobalRegister))
#define DMA_FLAGS_ADDRESS_REG(a)        ((ULONG*)(kDMATransportBase+(a*kDMATransportChannelSize)+kDMATransportFlagsRegister))
#define DMA_IO_INPUT_REG(a)             ((ULONG*)(kDMATransportBase+(a*kDMATransportChannelSize)+kDMATransportIOInputRegister))
#define DMA_IO_OUTPUT_REG(a)            ((ULONG*)(kDMATransportBase+(a*kDMATransportChannelSize)+kDMATransportIOOutputRegister))
#define DMA_IRQ_REG(a)                  ((ULONG*)(kDMATransportBase+(a*kDMATransportChannelSize)+kDMATransportIRQRegister))
#define DMA_IRQ_ACK_REG(a)              ((ULONG*)(kDMATransportBase+(a*kDMATransportChannelSize)+kDMATransportIRQAcknowledgeRegister))

// Virtual channel operation register offsets.
enum
{
    kDMAVirtualChannel          = 0x00,
    kDMAVirtualOperation        = 0x04,
    kDMAVirtualStatus           = 0x08,
    kDMAFlagsChannel            = 0x0C,
    kDMAReadChannel             = 0x10,
    kDMAWriteChannel            = 0x14,
};

#define DMA_CURR_V_CHANNEL              ((ULONG*)(kDMATransportBase+(NUM_DMA_CHANNELS*kDMATransportChannelSize)+kDMAVirtualChannel))
#define DMA_CURR_V_OPERATION            ((ULONG*)(kDMATransportBase+(NUM_DMA_CHANNELS*kDMATransportChannelSize)+kDMAVirtualOperation))
#define DMA_CURR_V_STATUS               ((ULONG*)(kDMATransportBase+(NUM_DMA_CHANNELS*kDMATransportChannelSize)+kDMAVirtualStatus))
#define DMA_V_CHANNEL_FLAGS             ((ULONG*)(kDMATransportBase+(NUM_DMA_CHANNELS*kDMATransportChannelSize)+kDMAFlagsChannel))
#define DMA_V_CHANNEL_READ              ((ULONG*)(kDMATransportBase+(NUM_DMA_CHANNELS*kDMATransportChannelSize)+kDMAReadChannel))
#define DMA_V_CHANNEL_WRITE             ((ULONG*)(kDMATransportBase+(NUM_DMA_CHANNELS*kDMATransportChannelSize)+kDMAWriteChannel))

enum
{
    kDMAVirtOperationAttach         = 0x01,
    kDMAVirtOperationDetach         = 0x02,
    kDMAVirtOpetationIsInUse        = 0x04,
    kDMAVirtOperationNewVirtChannel = 0x08,
    kDMAVirtOperationNewAddrChannel = 0x10,
};

#define VIRTUAL_BASE    0x80000000
// Names of events which allow us to signal calling app that data is available
// These match up with the names in the header file included by applications using the transport
const LPTSTR    channelEventName[]  = { {L"channel4"}, 
                                        {L"channel5"},
                                        {L"channel6"}, 
                                        {L"channel7"} };

// Number of channels
#define NUM_DMA_CHANNELS                        4

#define NUM_DMA_FIRST_CHANNEL                   4
#define NUM_DMA_LAST_CHANNEL                    7

// Packet size: 4KB
#define DMA_PACKET_SIZE                         0x1000

// Virtual channels
#define VIRT_CHANNEL_REQUEST                    8
#define ADDR_CHANNEL_REQUEST                    9

// Size of the memory window for accessing the DMA Transport peripheral device.
// The x86 dmatrans.cpp uses "(DMA_PACKET_SIZE * 2) + (NUM_DMA_CHANNELS * sizeof(ULONG)) + (2 * sizeof(IOParamBlock)+0x20)"
// But that size is smaller than what is actually used.  The value, below, is based on examination of
// the dmatrans.cpp sources.
#define DMA_DEVICE_SIZE                         0x2120

/*------------------------------------------------------------------
    Types
------------------------------------------------------------------*/

// Parameter block structure used to communicate with Emulated hardware.
struct IOParamBlock
{
    volatile ULONG	ioDataLength; //NOTE: non-cached, not an issue on an emulated system.
    volatile ULONG	ioDataPtr;
    volatile ULONG	ioStatus;
};


// Holds data for the transport
struct DMATransportInfo
{
    DMATransportInfo()				// Constructor
    {
        fDMAInterruptEvent = 0;		// Event signaled when DMA interrupt occurs
        fInterruptID = 0;			// IRQ of the interrupt
        fIntrServiceThread = NULL;	// Handle to the interrupt servicing thread (IST)
        fKillISTEvent = NULL;		// Event signaled when IST should be ended.
        fKillIST = false;			// Set to true when we should signal the killISTEvent.
    }			
    
    HANDLE		fDMAInterruptEvent;	// fDMAInterruptEvent signaled when interrupt occurs

    HANDLE		fIntrServiceThread;	// Handle to InterruptServiceThread
    HANDLE		fKillISTEvent;		// Signalled when we want to kill the IST
    BOOL		fKillIST;			// Set to true when we want to stop the IST
    
    DWORD		fInterruptID;		// Logical interrupt ID
};


// Holds info for DMA channel, an instance per channel
struct DMAChannel
{
    DMAChannel()
    {
        open = FALSE;
        vPFlags = NULL;
        pFlags = NULL;
        access = 0;
        dataReadyEvent = NULL;
        next = NULL;
        channelNumber = 0;
    }
    
    BOOL				open;				// True if channel is open

    PULONG				vPFlags;			// Virtual pointer to flags
    PULONG				pFlags;				// Physical pointer to flags

    ULONG				access;				// Read/Write access
    
    HANDLE				dataReadyEvent;		// Event is created in calling application, app passes event name to DeviceIOControl, and driver gets a handle to it
                                            // Fired when data is ready to be read
    ULONG               channelNumber;
    DMAChannel *        next;
};


/*------------------------------------------------------------------
    Globals
------------------------------------------------------------------*/

// Critical sections
// Locks are required because different processes in CE can call read or write simultaneously
CRITICAL_SECTION writeMutex;
CRITICAL_SECTION readMutex;
CRITICAL_SECTION vchannelAccess;

// Global parameter block variables

// physical
IOParamBlock*       inputPB;
IOParamBlock*       outputPB;

// virtual
IOParamBlock*       vInputPB;
IOParamBlock*       vOutputPB;

// Global buffers
PUCHAR              vInputBuffer;
PUCHAR              vOutputBuffer;

PUCHAR              inputBuffer;
PUCHAR              outputBuffer;

// Global structure to access transport info
DMATransportInfo    dmaTransportInfo;
DMAChannel          channels[NUM_DMA_CHANNELS];
DMAChannel *        virtChannels;

BOOL                initialized = FALSE;

/*------------------------------------------------------------------
    Local Prototypes
------------------------------------------------------------------*/

static DWORD WINAPI DMAInterruptServiceThread( PVOID Empty );
static BOOL StartIST();
static BOOL InitializeGlobals();

DMAChannel * getVirtualChannel(DWORD channelNumber);
DMAChannel * addVirtualChannel(DWORD channelNumber);
void deleteVirtualChannel(DMAChannel * channel);

/*------------------------------------------------------------------
    DllMain
    
    Entry point called when a process hooks up to the DLL.
------------------------------------------------------------------*/

BOOL APIENTRY DllMain(HANDLE hinstDLL, DWORD fdwReason, LPVOID lpv)
{

    switch ( fdwReason )
    {
        case DLL_PROCESS_ATTACH:
            break;
        case DLL_PROCESS_DETACH:
            break;
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            break;
    }
    
    return TRUE;
}


/*------------------------------------------------------------------
    DMAInterruptServiceThread
    
    When interrupt occurs on IRQ 15, this thread is triggered and
    in turn sets the dataReadyEvent for the appropriate channels
------------------------------------------------------------------*/

static DWORD WINAPI
DMAInterruptServiceThread( PVOID /*ignore*/ )   // NULL pointer does nothing.
{
    while ( TRUE )		// loop forever
    {
        // Wait for DMA event that is triggered by a the host sending data to the guest, so the guest reads
        if ( WaitForSingleObject( dmaTransportInfo.fDMAInterruptEvent, INFINITE ) == WAIT_FAILED ) // Something bad happened
        {
            continue;
        }
        
        //DEBUGMSG( TRUE, (TEXT("DMATransport Interrupt: Fired!\n")) );
        
        // Acknowledge the interrupt. Note we write to index 0, which is actually
        // Channel 4 in our case. We're sharing one interrupt for all four channels,
        // so we only need to ack on one of the channels to pull the IRQ line low.
        // It doesn't matter what we write, the data is ignored.
        WRITE_PORT_ULONG( DMA_IRQ_ACK_REG(0), 0 );
        
        if ( dmaTransportInfo.fKillIST )		// Event was fired from stopIST()
        {
            SetEvent( dmaTransportInfo.fKillISTEvent );
            ExitThread(0);
        }
        
        // Loop through channels
        for ( USHORT channelIndex = 0;  channelIndex < NUM_DMA_CHANNELS; ++channelIndex )
        {
            if ( channels[channelIndex].open )	// Channel is open
            {
                //DEBUGMSG( TRUE, (TEXT("DMATransport Interrupt: Channel %d is open\n"), channelIndex) );
                
                if ( *(channels[channelIndex].vPFlags) & kDMATransportDataReady )	// Channel has data to read
                {
                    //DEBUGMSG( TRUE, (TEXT("DMATransport Interrupt: Found data on channel %d.\n"), channelIndex) );
                    SetEvent( channels[channelIndex].dataReadyEvent );
                }
            }
        }
        
        // Loop through virtual channels
        EnterCriticalSection( &vchannelAccess );

        DMAChannel * currChannel = virtChannels;
        while (currChannel != NULL )
        {
            if ( currChannel->open)
            {
                WRITE_PORT_ULONG( DMA_V_CHANNEL_FLAGS, currChannel->channelNumber );
                // We can read any of the four flags registers - pick base one
                if ( *(DMA_V_CHANNEL_FLAGS) & kDMATransportDataReady )	// Channel has data to read
                {
                    //DEBUGMSG( TRUE, (TEXT("DMATransport Interrupt: Found data on channel %d.\n"), channelIndex) );
                    SetEvent( currChannel->dataReadyEvent );
                }
            }
            currChannel = currChannel->next;
        }

        LeaveCriticalSection( &vchannelAccess );

        // Tell kernel we're done processing the interrupt
        InterruptDone( dmaTransportInfo.fInterruptID );
    }
    
    return(0);
}


/*------------------------------------------------------------------
    StartIST
    
    Starts the interrupt service thread
    Returns: Success(true) or failure(false)
------------------------------------------------------------------*/

BOOL
StartIST()
{
    dmaTransportInfo.fKillIST = FALSE;
    
    // Initialize the interrupt to be associated with the fDMAInterruptEvent event
    if ( !InterruptInitialize(dmaTransportInfo.fInterruptID, dmaTransportInfo.fDMAInterruptEvent, NULL, 0) ) 
    {
        DEBUGMSG( TRUE, (TEXT("DMATransport:StartIST() failed on InterruptInitialize.\n")) );
        return FALSE;
    }
    
    // Create the thread
    dmaTransportInfo.fIntrServiceThread = CreateThread(NULL, 0, DMAInterruptServiceThread, NULL, 0, NULL);
    
    if ( dmaTransportInfo.fIntrServiceThread == NULL ) // Failure
    {
        DEBUGMSG( TRUE, (TEXT("DMATransport:StartIST() failed to create interrupt service thread.\n")) );
        return FALSE;
    }
    
    // Set thread priority above normal
    //CeSetThreadPriority( dmaTransportInfo.fIntrServiceThread, THREAD_PRIORITY_ABOVE_NORMAL );
    SetThreadPriority(dmaTransportInfo.fIntrServiceThread, THREAD_PRIORITY_HIGHEST);
    
    // Clear any interrupts lingering
    InterruptDone( dmaTransportInfo.fInterruptID );
    
    DEBUGMSG( TRUE, (TEXT("DMATransport:StartIST() succeeded.\n")) );
    
    return TRUE;
}



/*------------------------------------------------------------------
    InitializeGlobals
    
    Initializations that should only occur once for the
    entire life of the driver
------------------------------------------------------------------*/

BOOL InitializeGlobals()
{
    DEBUGMSG( TRUE, (TEXT("DMA Transport:Enter InitializeGlobals\n")) );
    
    // Initialize critical sections
    InitializeCriticalSection( &readMutex );
    InitializeCriticalSection( &writeMutex );
    InitializeCriticalSection( &vchannelAccess );

    // No virtual channels have been allocated 
    virtChannels = NULL;

    // Set up physical buffer
    // Allocate 2 4KB buffers, 4 4-byte IOFlags, 2 12-byte IOParam
    vInputBuffer = (PUCHAR)VirtualAlloc( 0, 
                                        DMA_DEVICE_SIZE,
                                        MEM_RESERVE,
                                        PAGE_NOACCESS );
    
    // Check virtualalloc succeded
    if (!vInputBuffer)  
    {
        return FALSE;
    }
    
    // Map physical address to virtual address
    if (!VirtualCopy( (LPVOID) vInputBuffer,
                   (LPVOID)(BSP_BASE_REG_PA_DMA >> 8),
                   DMA_DEVICE_SIZE,
                   PAGE_READWRITE | PAGE_NOCACHE | PAGE_PHYSICAL))
    {
        return FALSE;
    }
    
    // Set up virtual addresses we've allocated
    vOutputBuffer		= vInputBuffer + DMA_PACKET_SIZE;
    
    channels[0].vPFlags	= (PULONG)(vOutputBuffer + DMA_PACKET_SIZE);
    channels[1].vPFlags	= (channels[0].vPFlags + 1);
    channels[2].vPFlags	= (channels[1].vPFlags + 1);
    channels[3].vPFlags	= (channels[2].vPFlags + 1);
    
    vInputPB			= (IOParamBlock*)(channels[3].vPFlags + 1);
    vOutputPB			= (IOParamBlock*)((PUCHAR)vInputPB + sizeof(IOParamBlock));
    
    // Set physical addresses we've allocated
    inputBuffer			= ( PUCHAR )( BSP_BASE_REG_PA_DMA );
    outputBuffer		= inputBuffer + DMA_PACKET_SIZE;
    
    channels[0].pFlags	= (PULONG)(outputBuffer + DMA_PACKET_SIZE);
    channels[1].pFlags	= (channels[0].pFlags + 1);
    channels[2].pFlags	= (channels[1].pFlags + 1);
    channels[3].pFlags	= (channels[2].pFlags + 1);
    
    inputPB				= (IOParamBlock*)(channels[3].pFlags + 1);
    outputPB			= (IOParamBlock*)((PUCHAR)inputPB + sizeof(IOParamBlock));

    // Create events
    dmaTransportInfo.fDMAInterruptEvent				= CreateEvent(0, FALSE, FALSE, NULL);
    dmaTransportInfo.fKillISTEvent			= CreateEvent(0, FALSE, FALSE, NULL);
    
    // Make sure events are valid
    if ( !dmaTransportInfo.fDMAInterruptEvent	|| 
         !dmaTransportInfo.fKillISTEvent	   )
    {
        return (FALSE);
    }
    
    // Request a SYSINTR corresponding to the IRQ
    if (!KernelIoControl(IOCTL_HAL_REQUEST_SYSINTR, &kDMATransport_IRQ, sizeof(DWORD), &dmaTransportInfo.fInterruptID, sizeof(DWORD), NULL))
    {
        RETAILMSG(1, (TEXT("DMATransport:InitializeGlobals Failed to obtain sysintr value for data interrupt.\r\n")));
        dmaTransportInfo.fInterruptID = SYSINTR_UNDEFINED;
        return (0);
    }       

    // Hook the interrupt and start the associated thread.
    if ( !StartIST() ) 
    {
        return FALSE;	
    }

    DEBUGMSG( TRUE, (TEXT("DMA Transport:Leaving InitializeGlobals\n")) );
    return TRUE;
}


/*------------------------------------------------------------------
    DMA_Init
    
    Called for each channel as device manager enumerates through
    registry entries.
------------------------------------------------------------------*/

DWORD
DMA_Init( DWORD registryPath )
{
    DEBUGMSG( TRUE, (TEXT("DMA Transport:Enter DMA_Init\n")) );

    ULONG   index;

    // Figure out the DMA channel we're trying to Init by reading the registry
	HKEY	hRegKey	    = OpenDeviceKey( (LPCTSTR)registryPath );
    ULONG	dataSize	= sizeof(DWORD);
    ULONG	valueType	= 0;
    ULONG	channel		= 0;	// 4, 5, 6, 7 representing channel wanted
    ULONG	global		= 0; 
    
    ULONG   channelIRQ      = 0;
	
	// Index.
    if (ERROR_SUCCESS != RegQueryValueEx(hRegKey, 
                        L"Index", 
                        NULL, 
                        &valueType,
                        (LPBYTE)&channel, 
	                    &dataSize))
	{
		RETAILMSG(TRUE, (TEXT("DMATransport:DMA_Init() ERROR: failed to read INDEX value from the registry.\r\n")));
		return(0);
	}

	// Irq.
        if (ERROR_SUCCESS != RegQueryValueEx(hRegKey, 
						                 L"Irq", 
						                 NULL, 
						                 &valueType,
						                 (LPBYTE)&channelIRQ, 
						                 &dataSize))
	{
		RETAILMSG(TRUE, (TEXT("DMATransport:DMA_Init() ERROR: failed to read IRQ value from the registry.\r\n")));
		return(0);
	}

	RegCloseKey(hRegKey); 

	// Initialize global driver state.
	//
	if ( initialized == FALSE )
	{
	        kDMATransport_IRQ = channelIRQ;
		InitializeGlobals();
		for ( index = 0; index < NUM_DMA_CHANNELS; index++ )
		{
			WRITE_PORT_ULONG( DMA_GLOBAL_REG(index), kDMATrasportHandshake );
		}
		initialized = TRUE;
	}
	else if ( channelIRQ != kDMATransport_IRQ )
	{
		RETAILMSG(TRUE, (TEXT("DMATransport:DMA_Init() ERROR: mismatched IRQ values.\r\n")));
		return(0);         
	}
    
    DEBUGMSG( TRUE, (TEXT("DMATransport:DMA_Init() initializing channel %d.\n"), channel) );
    
    // If we found a valid channel number, initialize the hardware.
    if ( channel >= NUM_DMA_FIRST_CHANNEL && channel <= NUM_DMA_LAST_CHANNEL )
    {
        // Channels go from 4-7, but access to channel array needs indecies 0-3
		index = channel - NUM_DMA_FIRST_CHANNEL;
        
        // Read the "global" port for this channel from the DMATransportDevice.
        global = READ_PORT_ULONG( DMA_GLOBAL_REG(index) );
        
        // If the handshake succeeded,
        if ( global & kDMATrasportHandshake )
        {
            // Set flags address.
            WRITE_PORT_ULONG( DMA_FLAGS_ADDRESS_REG(index), (ULONG)channels[index].pFlags );
            
            // Init interrupt request line.
			WRITE_PORT_ULONG( DMA_IRQ_REG(index), kDMATransport_IRQ );
            
            // Enable interrupts.
            global |= kDMATransportInterruptEnabled;
            WRITE_PORT_ULONG( DMA_GLOBAL_REG(index), global );
        }
        else
        {
            // Error initializing hardware. Tear down.
            DEBUGMSG( TRUE, (TEXT("DMA Transport:DMA_Init() failed!\n")) );
            DMA_Deinit( channel );
            return 0;
        }
    }

    DEBUGMSG( TRUE, (TEXT("DMA Transport:Leaving DMA_Init\n")) );
    return channel;
}


/*------------------------------------------------------------------
    DMA_Deinit
    
    Deinitializes indicated channel for DMA, called by device
    manager when
------------------------------------------------------------------*/

BOOL
DMA_Deinit( DWORD channel )
{   
    // Make sure channel is closed
    DMA_Close(channel);
    return TRUE;
}


/*------------------------------------------------------------------
    DMA_Open
    
    Opens the desired channel for an application, returns the
    channel opened, or zero for failure (if the channel is
    already open.)
------------------------------------------------------------------*/

extern "C" DWORD
DMA_Open( DWORD channelNumber,  // In: Channel number opening, 4 - 7
          DWORD access,			// In: Access, read, write, or read | write
          DWORD shareMode		// In: not used in this driver
                                )
{
    if ( channelNumber >= VIRT_CHANNEL_REQUEST )
        goto NewOpenCode;
    else 
    {
    DWORD index = channelNumber - NUM_DMA_FIRST_CHANNEL;	// Channels go from 4 - 7, but access to channel array needs indexes 0-3
    if ( channels[index].open )		// Channel already been opened
    {
        return 0; 
    }
    
    channels[index].dataReadyEvent	= CreateEvent( NULL, FALSE, FALSE, channelEventName[index] );
    if ( !channels[index].dataReadyEvent ) // Create event failed
    {
        return 0;
    }

    channels[index].open			= TRUE;
    channels[index].access			= access;

    // Return actual channel
    return channelNumber;
    }

NewOpenCode:
    DEBUGMSG( TRUE, (TEXT("DMA Transport:Enter DMA_Open() for %x\n"), channelNumber) );
    DWORD index = channelNumber - NUM_DMA_FIRST_CHANNEL;	// Channels go from 4 - 7, but access to channel array needs indexes 0-3
    DMAChannel * channel = NULL;

    EnterCriticalSection( &vchannelAccess );
    // Check if we need to generate the channel number
    if ( channelNumber == VIRT_CHANNEL_REQUEST ||
         channelNumber == ADDR_CHANNEL_REQUEST)
    {
        DEBUGMSG( TRUE, (TEXT("DMA Transport:Generating channel number\n")) );
        // Notify the emulator that the channel has been opened on the device
        WRITE_PORT_ULONG(DMA_CURR_V_CHANNEL, 0);
        if ( channelNumber == VIRT_CHANNEL_REQUEST )
            WRITE_PORT_ULONG(DMA_CURR_V_OPERATION, kDMAVirtOperationNewVirtChannel);
        else
            WRITE_PORT_ULONG(DMA_CURR_V_OPERATION, kDMAVirtOperationNewAddrChannel);
        // Read the new channel number
        channelNumber = *DMA_CURR_V_STATUS;
        DEBUGMSG( TRUE, (TEXT("DMA Transport:Selected channel %x\n"), channelNumber) );
    }
    // Select the right channel
    if ( index < NUM_DMA_CHANNELS )
        channel = &channels[index];
    else if ( channelNumber >= VIRTUAL_BASE )
    {
        channel = getVirtualChannel(channelNumber);
        if (!channel)
            channel = addVirtualChannel(channelNumber);
        if (!channel)
        {
            DEBUGMSG( TRUE, (TEXT("DMA Transport:Could not allocate new virt channel\n")) );
            goto Error;   // out of memory
        }
    }
    else
    {
        DEBUGMSG( TRUE, (TEXT("DMA Transport:Invalid channel passed in\n")) );
        goto Error;   // invalid channel number passed in
    }

    if ( channel->open )		// Channel already been opened
    {
        DEBUGMSG( TRUE, (TEXT("DMA Transport:Channel already open\n")) );
        goto Error; 
    }
    if ( index < NUM_DMA_CHANNELS )
        channel->dataReadyEvent = CreateEvent( NULL, FALSE, FALSE, channelEventName[index] );
    else
        channel->dataReadyEvent = CreateEvent( NULL, FALSE, FALSE, NULL );

    if ( !channel->dataReadyEvent ) // Create event failed
    {
        if ( channelNumber >= VIRTUAL_BASE )
            deleteVirtualChannel(channel);
        DEBUGMSG( TRUE, (TEXT("DMA Transport:Could not create event for channel %x\n"), channelNumber) );
        goto Error;
    }

    // Notify the emulator that the channel has been opened on the device
    WRITE_PORT_ULONG(DMA_CURR_V_CHANNEL, (channelNumber >= VIRTUAL_BASE ? channelNumber: index));
    WRITE_PORT_ULONG(DMA_CURR_V_OPERATION, kDMAVirtOperationAttach);

    // Check if the emulator was able to create an endpoint for this channel
    if (*DMA_CURR_V_STATUS)
    {
        DMA_Close(channelNumber);
        DEBUGMSG( TRUE, (TEXT("DMA Transport:Emulator returned failure\n")) );
        goto Error;
    }

    channel->open          = TRUE;
    channel->access        = access;
    LeaveCriticalSection( &vchannelAccess );

    // Return actual channel
    DEBUGMSG( TRUE, (TEXT("DMA Transport:SUCCESS DMA_Open() for %x\n"), channelNumber) );
    return channelNumber;
Error:
    DEBUGMSG( TRUE, (TEXT("DMA Transport:FAILED DMA_Open() for %x\n"), channelNumber) );
    LeaveCriticalSection( &vchannelAccess );
    return 0;
}


/*------------------------------------------------------------------
    DMA_Close
    
    Closes indicated DMA channel.
------------------------------------------------------------------*/

BOOL
DMA_Close( DWORD channelNumber )
{
    if ( channelNumber >= VIRTUAL_BASE )
        goto NewCloseCode;
    else 
    {
    DWORD index = channelNumber - NUM_DMA_FIRST_CHANNEL;
    
    // Make sure tha channel is open
    if ( channels[index].open )
    {
        channels[index].open = FALSE;
        CloseHandle( channels[index].dataReadyEvent );
    }

    return TRUE;
    }

NewCloseCode:
    DWORD index = channelNumber - NUM_DMA_FIRST_CHANNEL;
    DMAChannel * channel = NULL;

    EnterCriticalSection( &vchannelAccess );
    // Select the right channel
    if ( index < NUM_DMA_CHANNELS )
        channel = &channels[index];
    else if ( channelNumber >= VIRTUAL_BASE )
    {
        channel = getVirtualChannel(channelNumber);
        if (!channel)
        {
            LeaveCriticalSection( &vchannelAccess );
            return FALSE;   // invalid channel number passed in
        }
    }
    else
    {
        LeaveCriticalSection( &vchannelAccess );
        return FALSE;   // invalid channel number passed in
    }

    // Make sure tha channel is open
    if ( channel->open )
    {
        channel->open = FALSE;
        if ( channel->dataReadyEvent != NULL )
            CloseHandle( channel->dataReadyEvent );
    }

    // Remove the node from the linked list for virtual channels
    if ( channelNumber >= VIRTUAL_BASE )
        deleteVirtualChannel(channel);

    // Notify the emulator that the channel has been opened on the device
    WRITE_PORT_ULONG(DMA_CURR_V_CHANNEL, (channelNumber >= VIRTUAL_BASE ? channelNumber: index));
    WRITE_PORT_ULONG(DMA_CURR_V_OPERATION, kDMAVirtOperationDetach);

    // Check if the emulator was able to create an endpoint for this channel
    if (*DMA_CURR_V_STATUS)
    {
        DEBUGMSG( TRUE, (TEXT("DMA Transport:Emulator returned failure closing channel %x\n"), channelNumber) );
    }

    DEBUGMSG( TRUE, (TEXT("DMA Transport:SUCCESS DMA_Close() for %x\n"), channelNumber) );
    LeaveCriticalSection( &vchannelAccess );

    return TRUE;
}


/*------------------------------------------------------------------
    DMA_Write
    
    Writes indicated number of bytes to DMA device, up to a
    maximum of 4K. (The DMATransport device transmits data in 4K
    chunks. We leave it up to the client of this driver to handle
    breaking up packets.)
    
    Returns failure, or the number of bytes actually written.
------------------------------------------------------------------*/

DWORD
DMA_Write( DWORD    channel,    // In: channel number: 4-7
           LPCVOID	writeBytes,	// In: Data to write
           DWORD	numBytes	// In: number of bytes to write
                                )
{   
    if ( channel >= VIRTUAL_BASE )
        goto NewWriteCode;
    else 
    {
    // Enter critical section so no other write call can interfere
    EnterCriticalSection( &writeMutex );
    
    vOutputPB->ioStatus		= 0;
    vOutputPB->ioDataPtr	= (ULONG)outputBuffer; // specify physical address
    
    if ( numBytes > DMA_PACKET_SIZE )	// Trying to write too many bytes, can only write at the max DMA_PACKET_SIZE
    {
        vOutputPB->ioDataLength = DMA_PACKET_SIZE;
    }
    else	// Writing numBytes
    {
        vOutputPB->ioDataLength = numBytes;
    }
    
    memcpy( vOutputBuffer, (PUCHAR)writeBytes, vOutputPB->ioDataLength );
    
    DWORD index = channel - NUM_DMA_FIRST_CHANNEL;	// offset channel to match array indices
    
    // Write physical address to port to trigger an actual write
    WRITE_PORT_ULONG( DMA_IO_OUTPUT_REG(index), ( ULONG )outputPB );
    
    if ( vOutputPB->ioStatus != 0 )	// Error, return failure
    {
        vOutputPB->ioDataLength = -1;
    }
    
    LeaveCriticalSection( &writeMutex );
    
    return vOutputPB->ioDataLength;

    }
NewWriteCode:
    // Enter critical section so no other write call can interfere
    EnterCriticalSection( &writeMutex );
    DWORD index = 0;
    DWORD bytesWritten = 0;

    if (channel < VIRTUAL_BASE )
    {
        index = channel - NUM_DMA_FIRST_CHANNEL; // offset channel to match array indices
    }
    else
    {
        DEBUGMSG( TRUE, (TEXT("DMA Transport:WRITE virtual on channel %x\n"), channel) );
        WRITE_PORT_ULONG( DMA_V_CHANNEL_WRITE, channel );
    }

    vOutputPB->ioStatus		= 0;
    vOutputPB->ioDataPtr	= (ULONG)outputBuffer; // specify physical address
    
    if ( numBytes > DMA_PACKET_SIZE )	// Trying to write too many bytes, can only write at the max DMA_PACKET_SIZE
    {
        vOutputPB->ioDataLength = DMA_PACKET_SIZE;
    }
    else	// Writing numBytes
    {
        vOutputPB->ioDataLength = numBytes;
    }
    
    DEBUGMSG( TRUE, (TEXT("DMA Transport:WRITE virtual %x/%x bytes on channel %x \n"), numBytes, vOutputPB->ioDataLength, channel) );
    memcpy( vOutputBuffer, (PUCHAR)writeBytes, vOutputPB->ioDataLength );
    
    DEBUGMSG( TRUE, (TEXT("DMA Transport:WRITE wrote out the data on %x \n"), channel) );
    // Write physical address to port to trigger an actual write
    WRITE_PORT_ULONG( DMA_IO_OUTPUT_REG(index), ( ULONG )outputPB );
    
    bytesWritten = vOutputPB->ioDataLength;

    if ( vOutputPB->ioStatus != 0 )	// Error, return failure
    {
        DEBUGMSG( TRUE, (TEXT("DMA Transport:WRITE failed on channel %x with code %x\n"), channel, vOutputPB->ioStatus) );
        if (channel >= VIRTUAL_BASE )
        {
            // Propagate the error code
            SetLastError(vOutputPB->ioStatus);
        }
        bytesWritten = -1;
    }
    
    if (channel >= VIRTUAL_BASE )
    {
        DEBUGMSG( TRUE, (TEXT("DMA Transport:WRITE virtual on channel %x (Cleared W_REG)\n"), channel) );
        WRITE_PORT_ULONG( DMA_V_CHANNEL_WRITE, 0 );
    }

    LeaveCriticalSection( &writeMutex );
    
    return bytesWritten;
}


/*------------------------------------------------------------------
    DMA_Read
    
    Reads packets from device. Can only read 4 KB chunks, will
    fail if another value is specified. (The DMATransport device
    transmits data in 4K chunks. We leave it up to the client
    of this driver to handle breaking up packets.)
------------------------------------------------------------------*/

DWORD
DMA_Read( DWORD     channel,    // In: Channel number
          LPVOID	buffer,		// In\Out: buffer to read data into
          DWORD		numRead		// In: Number of bytes to read into buffer
                            )
{
    if ( channel >= VIRTUAL_BASE )
        goto NewReadCode;
    else 
    {
    DWORD bytesRead = 0;

    if ( numRead != DMA_PACKET_SIZE )	// can only read 4KB chunks
    {
        return -1;
    }

    EnterCriticalSection( &readMutex );

    vInputPB->ioDataLength	= DMA_PACKET_SIZE;
    vInputPB->ioDataPtr		= (ULONG)inputBuffer; //  specify physical address
    vInputPB->ioStatus		= -1;

    DWORD index = channel - NUM_DMA_FIRST_CHANNEL;	// offset channel to match array indices
    
    if ( *(channels[index].vPFlags) & kDMATransportDataReady )
    {
        WRITE_PORT_ULONG((PULONG)DMA_IO_INPUT_REG( index ), ( ULONG )inputPB);
        
        // Data and no error
        if( vInputPB->ioDataLength != 0 && vInputPB->ioStatus == 0 )
        {
            // now copy that many bytes into output buffer.
            memcpy(buffer, vInputBuffer, vInputPB->ioDataLength );
        }
        else	// error
        {
            vInputPB->ioDataLength = -1;		// 0 indicates error
        }
    }
    else
    {
        vInputPB->ioDataLength = 0;	// Nothing to read, 0 indicates end of file
    }

    bytesRead = vInputPB->ioDataLength;

    LeaveCriticalSection( &readMutex );

    return bytesRead;
    }

NewReadCode:
    DWORD bytesRead = 0;

    if ( numRead != DMA_PACKET_SIZE )	// can only read 4KB chunks
    {
        return -1;
    }

    EnterCriticalSection( &readMutex );
    DWORD index = 0;

    if (channel < VIRTUAL_BASE )
    {
        index = channel - NUM_DMA_FIRST_CHANNEL; // offset channel to match array indices
    }
    else
    {
        DEBUGMSG( TRUE, (TEXT("DMA Transport:READ virtual on channel %x\n"), channel) );
        WRITE_PORT_ULONG( DMA_V_CHANNEL_READ, channel );
    }

    vInputPB->ioDataLength	= DMA_PACKET_SIZE;
    vInputPB->ioDataPtr		= (ULONG)inputBuffer; //  specify physical address
    vInputPB->ioStatus		= -1;

    if ( channel < VIRTUAL_BASE  && (*(channels[index].vPFlags) & kDMATransportDataReady) ||
         channel >= VIRTUAL_BASE && (*DMA_V_CHANNEL_READ & kDMATransportDataReady) )
    {
        WRITE_PORT_ULONG((PULONG)DMA_IO_INPUT_REG( index ), ( ULONG )inputPB);
        
        // Data and no error
        if( vInputPB->ioDataLength != 0 && vInputPB->ioStatus == 0 )
        {
            // now copy that many bytes into output buffer.
            DEBUGMSG( TRUE, (TEXT("DMA Transport:READ %x bytes SUCCESS on channel %d\n"), vInputPB->ioDataLength, channel) );
            memcpy(buffer, vInputBuffer, vInputPB->ioDataLength );
        }
        else	// error
        {
            DEBUGMSG( TRUE, (TEXT("DMA Transport:READ FAILED on channel %x\n"), channel) );
            vInputPB->ioDataLength = -1;		// 0 indicates error
        }
    }
    else
    {
        DEBUGMSG( TRUE, (TEXT("DMA Transport:READ nothing to read on channel %x\n"), channel) );
        vInputPB->ioDataLength = 0;	// Nothing to read, 0 indicates end of file
    }

    bytesRead = vInputPB->ioDataLength;

    if (channel >= VIRTUAL_BASE )
    {
        DEBUGMSG( TRUE, (TEXT("DMA Transport:READ virtual on channel %x (Cleared R_REG)\n"), channel) );
        WRITE_PORT_ULONG( DMA_V_CHANNEL_READ, 0 );
    }

    LeaveCriticalSection( &readMutex );

    return bytesRead;
}


/*------------------------------------------------------------------
    DMA_IOControl
    
    Standard stream interface IOControl function. Does nothing here.
------------------------------------------------------------------*/

extern "C" BOOL
DMA_IOControl( DWORD channel,       // In: channel number
               DWORD code,			// In: Holds the requested operation
               PBYTE inBuf,			// In: Buffer being passed in
               DWORD inLength,		// In: Length of the inBuf
               PBYTE outBuf,		// Out: Data sent from driver to application
               DWORD outLength,		// Out: length of the outBuf
               PDWORD actualOut		// Out: actual number of bytes received
                                    )
{
    if ( code == IOCTL_GET_DMA_CHANNEL_INFO ) 
    {
        DEBUGMSG( TRUE, (TEXT("DMA Transport:Processing IOCTL - %x \n"), IOCTL_GET_DMA_CHANNEL_INFO) );
        *actualOut = 0;
        // Verify arguments
        if (channel < VIRTUAL_BASE )
        {
            DEBUGMSG( TRUE, (TEXT("DMA Transport:FAILED IOCTL for channel %x - invalid channel number\n"), channel) );
            return FALSE;
        }
        if (outBuf == NULL || outLength < 8)
        {
            DEBUGMSG( TRUE, (TEXT("DMA Transport:FAILED IOCTL for channel %x - invalid buffer arguments\n"), channel) );
            return FALSE;
        }

        EnterCriticalSection( &vchannelAccess );
        // Retrieve the channel
        DMAChannel * channelObj = getVirtualChannel(channel);
        if (channelObj == NULL)
        {
            DEBUGMSG( TRUE, (TEXT("DMA Transport:FAILED IOCTL for channel %x - couldn't find the channel\n"), channel) );
            goto ErrorGetInfo;
        }

        if (!DuplicateHandle( GetCurrentProcess(), 
                              channelObj->dataReadyEvent, 
                              GetOwnerProcess(),
                              (void **)&outBuf[4],
                              0,
                              FALSE,
                              DUPLICATE_SAME_ACCESS ))
        {
            DEBUGMSG( TRUE, (TEXT("DMA Transport:Couldn't duplicate event handle. Last Error %x\n"), GetLastError()) );
            goto ErrorGetInfo;
        }

        // Write out the channel number
        *((DWORD *)outBuf) = channel;
        *actualOut = 8;
        LeaveCriticalSection( &vchannelAccess );

        DEBUGMSG( TRUE, (TEXT("DMA Transport:SUCCESS IOCTL for channel %x\n"), channel) );
        return TRUE;

ErrorGetInfo:
        LeaveCriticalSection( &vchannelAccess );
        return FALSE;
    }
    else if ( code == IOCTL_GET_DMA_CHANNEL_CONNECTED )
    {
        DEBUGMSG( TRUE, (TEXT("DMA Transport:Processing IOCTL - %x \n"), IOCTL_GET_DMA_CHANNEL_INFO) );
        *actualOut = 0;
        // Verify arguments
        if (channel < VIRTUAL_BASE )
        {
            DEBUGMSG( TRUE, (TEXT("DMA Transport:FAILED IOCTL for channel %x - invalid channel number\n"), channel) );
            return FALSE;
        }
        if ( outBuf == NULL || outLength < 4)
        {
            DEBUGMSG( TRUE, (TEXT("DMA Transport:FAILED IOCTL for channel %x - invalid buffer arguments\n"), channel) );
            return FALSE;
        }

        EnterCriticalSection( &vchannelAccess );
        // Retrieve the channel
        DMAChannel * channelObj = getVirtualChannel(channel);
        if (channelObj == NULL)
        {
            DEBUGMSG( TRUE, (TEXT("DMA Transport:FAILED IOCTL for channel %x - couldn't find the channel\n"), channel) );
            goto ErrorGetConnected;
        }
        // Get the channel status from the emulator
        WRITE_PORT_ULONG(DMA_CURR_V_CHANNEL, channel);
        WRITE_PORT_ULONG(DMA_CURR_V_OPERATION, kDMAVirtOpetationIsInUse);

        *((DWORD *)outBuf) = *DMA_CURR_V_STATUS;
        *actualOut = 4;

        DEBUGMSG( TRUE, (TEXT("DMA Transport:Channel status for %x is %x \n"),channel, *((DWORD *)outBuf)) );
        LeaveCriticalSection( &vchannelAccess );

        DEBUGMSG( TRUE, (TEXT("DMA Transport:SUCCESS IOCTL for channel %x\n"), channel) );
        return TRUE;

ErrorGetConnected:
        LeaveCriticalSection( &vchannelAccess );
        return FALSE;
    }
    return FALSE;
}


/*------------------------------------------------------------------
    DMA_PowerUp
    
    Standard stream interface PowerUp function. Does nothing here.
------------------------------------------------------------------*/

extern "C" void
DMA_PowerUp( DWORD hContext )
{   
    // do nothing
}


/*------------------------------------------------------------------
    DMA_PowerDown
    
    Standard stream interface PowerDown function. Does nothing here.
------------------------------------------------------------------*/
extern "C" void
DMA_PowerDown( DWORD hContext )
{
    // do nothing
}


/*------------------------------------------------------------------
    DMA_Seek
    
    Standard stream interface seek function. Does nothing here.
------------------------------------------------------------------*/

extern "C" DWORD
DMA_Seek( DWORD hOpenContext, 
          long Amount, 
          WORD Type )
{
    // Do nothing, return fail
    return -1;
}

DMAChannel * getVirtualChannel(DWORD channelNumber)
{
    DMAChannel * currChannel = virtChannels;
    while (currChannel != NULL )
    {
        if ( currChannel->channelNumber == channelNumber)
        {
           return currChannel;
        }
        currChannel = currChannel->next;
    }
    // Failed to find the channel
    return NULL;
}

DMAChannel * addVirtualChannel(DWORD channelNumber)
{
    DMAChannel * currChannel = virtChannels;
    DMAChannel * newChannel = new DMAChannel();

    if ( newChannel == NULL )
        return NULL;

    while (currChannel != NULL && currChannel->next != NULL)
    {
        currChannel = currChannel->next;
    }

    if ( currChannel != NULL )
        currChannel->next = newChannel;
    else
        virtChannels = newChannel;

    newChannel->channelNumber = channelNumber;
    return newChannel;
}

void deleteVirtualChannel(DMAChannel * channel)
{
    DMAChannel * currChannel = virtChannels;

    if ( channel == NULL )
        return;

    // Check with the head of the list
    if (channel == virtChannels)
    {
        virtChannels = virtChannels->next;
        delete channel;
        return;
    }

    // Check the body of the list
    while (currChannel != NULL && currChannel->next != channel)
    {
        currChannel = currChannel->next;
    }

    // If found in the body - relink around it.
    if (currChannel != NULL )
    {
        currChannel->next = currChannel->next->next;
        delete channel;
    }
}

