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

Module Name:  

Abstract:

    Platform dependent PCMCIA initialization functions

Notes: 
--*/
#include <windows.h>
#include <types.h>
#include <socksv2.h>
#include <memory.h>
#include <ceddk.h>
#include <nkintr.h>

#include "PDSocket.h"

//////////////////////////////////////////////////////////////////////////

//#define PCMCIA_DRIVER_KEY TEXT("Drivers\\PCMCIA")
#define FN_SYSINTR_VALUE_NAME TEXT("FnSysIntr")
#define FN_IRQ_VALUE_NAME TEXT("FnIrq")
#define FORCE_FN_SYSINTR_NAME TEXT("ForceFnSysIntr")
#define CONTROLLER_FN_INT_VALUE_NAME TEXT("ControllerFnInt")
#define CSC_SYSINTR_VALUE_NAME TEXT("CSCSysIntr")
#define CSC_IRQ_VALUE_NAME TEXT("CSCIrq")
#define POLLING_MODE_NAME TEXT("PollingMode")
#define POLL_TIMEOUT_NAME TEXT("PollTimeout")
#define DISABLE_SOCKET_NAME TEXT("DisableSocket")
#define CSC_INTERRUPT_DELAY_NAME TEXT("CSCInterruptDelay")

#define CHIPSET_ID_CNT 32
#define CHIPSET_ID_LEN ((CHIPSET_ID_CNT * 9) + 1)

const SS_POWER_ENTRY CPcmciaCardSocket::m_rgPowerEntries[NUM_POWER_ENTRIES] =
{
    { 0,    PWR_SUPPLY_VCC | PWR_SUPPLY_VPP1 | PWR_SUPPLY_VPP2 },
    { 33,   PWR_SUPPLY_VCC                                     },
    { 50,   PWR_SUPPLY_VCC | PWR_SUPPLY_VPP1 | PWR_SUPPLY_VPP2 },
    { 120,                   PWR_SUPPLY_VPP1 | PWR_SUPPLY_VPP2 }
};
DWORD CPcmciaCardSocket::ms_dwSocketLastIndex = 1;

CPcmciaCardSocket::CPcmciaCardSocket( CPcmciaBusBridge* pBridge )
{
    m_pBridge = pBridge;
    m_dwSocketIndex = 0;
    while( m_dwSocketIndex == 0 )
    {
        m_dwSocketIndex = ( DWORD )
                          InterlockedIncrement( ( LONG * ) &ms_dwSocketLastIndex ); 
        //Make it is it does not exist.
        CPcmciaCardSocket* pSocket = GetSocket( ( HANDLE ) m_dwSocketIndex );
        if( pSocket != NULL )
        {
            // Duplicated , Retry.
            m_dwSocketIndex = 0;
            pSocket->DeRef();
        }
    }

    DEBUGCHK( m_pBridge );
    while( m_pBridge->LockOwner( 1000 ) != TRUE )
    {
        DEBUGCHK( FALSE );
    }
    DEBUGMSG( ZONE_INIT,
              ( TEXT( "CARDBUS: CPcmciaCardSocket (Socket=%d) Created\r\n" ),
                GetSocketHandle() ) );
}

CPcmciaCardSocket::~CPcmciaCardSocket()
{
    m_pBridge->ReleaseOwner();
    DEBUGMSG( ZONE_INIT,
              ( TEXT( "CARDBUS: CPcmciaCardSocket (Socket=%d) Deleted\r\n" ),
                GetSocketHandle() ) );
}

STATUS CPcmciaCardSocket::GetPowerEntry( PDWORD pdwNumOfEnery,
                                         PSS_POWER_ENTRY pPowerEntry )
{
    STATUS status = CERR_BAD_ARGS;
    if( pdwNumOfEnery != NULL && pPowerEntry != NULL )
    {
        DWORD dwNumOfCopied = min( *pdwNumOfEnery, NUM_POWER_ENTRIES );
        if( dwNumOfCopied != 0 )
        {
            memcpy( pPowerEntry,
                    m_rgPowerEntries,
                    dwNumOfCopied * sizeof( SS_POWER_ENTRY ) );
            *pdwNumOfEnery = dwNumOfCopied;
            status = CERR_SUCCESS;
        }
    }
    return status;
}

UINT16 CPcmciaCardSocket::GetSocketNo()
{
    return m_pBridge->GetSocketNo();
};

CPcmciaBusBridge::CPcmciaBusBridge( LPCTSTR RegPath ) : CPCCardBusBridgeBase( RegPath ),
                                                        CMiniThread( 0,
                                                                     TRUE )
{
    m_dwFnIrq = -1;
    m_fPCICCritSecInitialized = false;
    m_pCardBusResource = NULL;
    m_hISTEvent = NULL;
    m_uSocketNum = ( WORD ) - 1;
    m_pCardSocket = NULL;
    m_fPowerCycleEvent = FALSE;
    m_fCardInjectEvent = FALSE;
    m_fPollingMode = TRUE;
	m_dwCSCInterruptDelay = 0;
};


CPcmciaBusBridge::~CPcmciaBusBridge()
{
    // Terminate IST
    m_bTerminated = TRUE;
    if( m_hISTEvent )
    {
        SetEvent( m_hISTEvent );
        ThreadTerminated( 1000 );
        if( !m_fPollingMode )
        {
            InterruptDisable( m_dwCSCSysIntr );
        }
        CloseHandle( m_hISTEvent );
    };

    if( m_uSocketNum != ( UINT16 ) - 1 )
    {
        GetSocketNumberFromCS( FALSE );
        m_uSocketNum = ( UINT16 ) - 1;
    }

    RemovePcmciaCardSocket();

    if( m_pCardBusResource != NULL )
    {
        delete  m_pCardBusResource;
    }
    if( m_fPCICCritSecInitialized )
    {
        DeleteCriticalSection( &m_PCICCritSec );
    }
}

BOOL CPcmciaBusBridge::InstallIsr()
{
    UINT8 tmp;

    //
    // Disable interrupts
    //
    WritePCICRegister( REG_INTERRUPT_AND_GENERAL_CONTROL, 0 );

	// Management int -> edge triggering(PULSE), System int -> LEVEL triggering 
    WritePCICRegister( REG_GENERAL_CONTROL, MISC1_VCC_33|MISC1_PM_IRQ|MISC1_SPK_ENABLE );
    UINT8 bPat = ReadPCICRegister( REG_GENERAL_CONTROL );
	// 25Mhz_bypass,low_power_dynamic,IRQ12=drive_LED
	WritePCICRegister( REG_GLOBAL_CONTROL, MISC2_LOW_POWER_MODE|MISC2_LED_ENABLE);

	// before configuring timing register, FIFO should be cleared.
	WritePCICRegister( REG_FIFO_CTRL, FIFO_EMPTY_WRITE);    //Flush FIFO

	//default access time is 300ns
	WritePCICRegister( REG_SETUP_TIMING0, 5);                   //80ns(no spec)
	WritePCICRegister( REG_CMD_TIMING0, 20);                  //320ns(by spec,25Mhz clock)
	WritePCICRegister( REG_RECOVERY_TIMING0, 5);                   //80ns(no spec)

	//default access time is 300ns
	WritePCICRegister( REG_SETUP_TIMING1, 2);                   //80ns(no spec)
	WritePCICRegister( REG_CMD_TIMING1, 8);                   //320ns(by spec,25Mhz clock)
	WritePCICRegister( REG_RECOVERY_TIMING1, 2);                   //80ns(no spec)

    if( !m_fPollingMode )
    {
        // PD6710 specific code to enable CSC interrupt (routed to -INTR)
        tmp = CFG_CARD_DETECT_ENABLE | CFG_READY_ENABLE;
        WritePCICRegister( REG_STATUS_CHANGE_INT_CONFIG, tmp );

        // Enable Manage Interrupt
        tmp = ReadPCICRegister( REG_INTERRUPT_AND_GENERAL_CONTROL );
        tmp |= INT_ENABLE_MANAGE_INT;
        WritePCICRegister( REG_INTERRUPT_AND_GENERAL_CONTROL, tmp );
    }
    else
    {
        WritePCICRegister( REG_STATUS_CHANGE_INT_CONFIG, 0 );
    }

    // CreateIST Event
    m_hISTEvent = CreateEvent( 0, FALSE, FALSE, NULL );

    if( !m_fPollingMode )
    {
        // Run IST
        BOOL r = InterruptInitialize( m_dwCSCSysIntr, m_hISTEvent, 0, 0 );
        ASSERT( r );
    }

    return TRUE;
}

#ifdef DEBUG
VOID CPcmciaBusBridge::DumpAllRegisters()
{
    DEBUGMSG( ZONE_FUNCTION, ( TEXT( "Dumping all PCIC registers\r\n" ) ) );
    for( UINT8 nRegNum = 0; nRegNum < 0x40; nRegNum++ )
    {
        UINT8 val;
		if( nRegNum == REG_CARD_STATUS_CHANGE )
			val = -1;
		else
			val = ReadPCICRegister( nRegNum );
        DEBUGMSG( ZONE_FUNCTION,
                  ( TEXT( "%02x: %02x\r\n" ), nRegNum, val ) );
    }
    DEBUGMSG( ZONE_FUNCTION, ( TEXT( "Dump completed.\r\n" ) ) );
}
#endif

//
// Function to set the PCIC index register
//
VOID CPcmciaBusBridge::PCICIndex( UINT8  register_num )
{
    WRITE_PORT_UCHAR( m_PCICIndex, ( UINT8 )( register_num ) );
}

//
// Function to write to the PCIC data register
//
VOID CPcmciaBusBridge::PCICDataWrite( UINT8 value )
{
    WRITE_PORT_UCHAR( m_PCICData, value );
}

//
// Function to read the PCIC data register
//
UINT8 CPcmciaBusBridge::PCICDataRead( VOID )
{
    return READ_PORT_UCHAR( m_PCICData );
}


//
// Verify the PCIC's REG_CHIP_REVISION
//
// This bit of code looks in the 82365 chip revision register (PCIC index 0)
// to see if a valid 82365 is in the system.  The original code only
// recognized the 83h silicon revision.  This indicates REV C silicon from
// Intel.  However, Intel also had a very popular rev B version, and that's
// what the integrated PCMCIA controller on the AMD ElanSC400 emulated.  The
// silicon revision register for that version returned 82h.
//
BOOL CPcmciaBusBridge::IsValidPCICSig( void )
{
    switch( m_vRevision = ReadPCICRegister( REG_CHIP_REVISION ) )
    {
      case 0x82:
      case 0x83:
      case 0x84:
        // for newer chip - can handle 3.3v
        DEBUGMSG( 1,
                  ( TEXT( "PCMCIA:IsValidPCICSig Valid CHIP_REVISION detected = 0x%x at 0x%x\r\n" ),
                    m_vRevision,
                    m_PCICIndex ) );
        return TRUE;
    }
    DEBUGMSG( 1,
              ( TEXT( "PCMCIA:IsValidPCICSig Invalid CHIP_REVISION = 0x%x at 0x%x!!!\r\n" ),
                m_vRevision,
                m_PCICIndex ) );
    return FALSE;
}

//
// Function to get the initial settings from the registry
//
// NOTE: lpRegPath is assumed to be under HKEY_LOCAL_MACHINE
//
// Returns ERROR_SUCCESS on success or a Win32 error code on failure
//
DWORD CPcmciaBusBridge::GetRegistryConfig()
{
    DWORD dwRet = 1;
    DWORD dwSize, dwType, dwData;

    // get the PCMCIA windows configuration
    if( !LoadWindowsSettings() )
    {
        dwRet = ERROR_INVALID_DATA;
        goto grc_fail;
    }

    // get the polling mode value
    dwSize = sizeof( DWORD );
    if( !RegQueryValueEx( POLLING_MODE_NAME,
                          &dwType,
                          ( PUCHAR ) & dwData,
                          &dwSize ) )
    {
        m_fPollingMode = FALSE; // RegQueryValueEx failed, default to FALSE
    }
    else
    {
        m_fPollingMode = dwData ? TRUE : FALSE;
    }

    // get the function interrupt routing configuration

    // Get the controller function interrupt number
    dwSize = sizeof( DWORD );
    if( RegQueryValueEx( CONTROLLER_FN_INT_VALUE_NAME,
                          &dwType,
                          ( PUCHAR ) & dwData,
                          &dwSize ) )
    {
        m_dwControllerFnIntNo = dwData;
    }
    else
    {
        dwRet = FALSE;
        goto grc_fail;
    }

    // Get the function interrupt SYSINTR or IRQ value

    // First try to get the IRQ value
    dwSize = sizeof( DWORD );
    if( RegQueryValueEx( FN_IRQ_VALUE_NAME,
                        &dwType,
                        ( PUCHAR ) & dwData,
                        &dwSize ) )
    {
        m_dwFnSysIntr = SYSINTR_UNDEFINED;
        m_dwFnIrq = dwData;

        // check if we should automatically convert the IRQ to SYSINTR value
        dwSize = sizeof( DWORD );
        if( RegQueryValueEx( FORCE_FN_SYSINTR_NAME,
                              &dwType,
                              ( PUCHAR ) & dwData,
                              &dwSize ) )
        {
            if( dwData )
            {
                // Convert the interrupt to a logical sysintr value.
                if (!KernelIoControl(IOCTL_HAL_REQUEST_SYSINTR, &m_dwFnIrq, sizeof(DWORD), &m_dwFnSysIntr, sizeof(DWORD), NULL))
                {
                    RETAILMSG(1, (TEXT("PCMCIA: Failed to obtain function interrupt information!\r\n")));
                    m_dwFnSysIntr = SYSINTR_UNDEFINED;
                    dwRet = ERROR_INVALID_DATA;
                    goto grc_fail;
                }
                m_dwFnIrq = -1;
            }
        }
    }
    else
    {
        // IRQ value was not specified, try to get the SYSINTR value
        dwSize = sizeof( DWORD );
        if( RegQueryValueEx( FN_SYSINTR_VALUE_NAME,
                              &dwType,
                              ( PUCHAR ) & dwData,
                              &dwSize ) )
        {
            m_dwFnSysIntr = dwData;
            m_dwFnIrq = -1;
        }
        else
        {
            RETAILMSG(1, (TEXT("PCMCIA: Failed to obtain function interrupt information!\r\n")));
            dwRet = ERROR_INVALID_DATA;
            goto grc_fail;
        }
    }

    // get card status change interrupt routing configuration
    m_dwCSCSysIntr = SYSINTR_UNDEFINED;

    dwSize = sizeof( DWORD );
    if( RegQueryValueEx( CSC_SYSINTR_VALUE_NAME,
                            &dwType,
                            ( PUCHAR ) & dwData,
                            &dwSize ) )
    {
        m_dwCSCSysIntr = dwData;
    }
    else
    {
        dwSize = sizeof( DWORD );
        if( !RegQueryValueEx( CSC_IRQ_VALUE_NAME,
                                &dwType,
                                ( PUCHAR ) & dwData,
                                &dwSize ) )
        {
            m_fPollingMode = TRUE;
        }
        else
        {
            // Convert the interrupt to a logical sysintr value.
            if (!KernelIoControl(IOCTL_HAL_REQUEST_SYSINTR, &dwData, sizeof(DWORD), &m_dwCSCSysIntr, sizeof(DWORD), NULL))
            {
                RETAILMSG(1, (TEXT("PCMCIA: Failed to obtain sysintr value for CSC interrupt.\r\n")));
                m_dwCSCSysIntr = SYSINTR_UNDEFINED;
                m_fPollingMode = TRUE;
            }
        }
    }

    // get the polling timeout value
    dwSize = sizeof( DWORD );
    if( !RegQueryValueEx( POLL_TIMEOUT_NAME,
                          &dwType,
                          ( PUCHAR ) & dwData,
                          &dwSize ) )
    {
        // RegQueryValueEx failed; if polling, set the timeout to 0.5 sec, otherwise set to INFINITE
        m_dwPollTimeout = m_fPollingMode ? 500 : INFINITE;
    }
    else
    {
        m_dwPollTimeout = dwData;
    }

    // get the CSC Interrupt delay
    dwSize = sizeof( DWORD );
    if( RegQueryValueEx( CSC_INTERRUPT_DELAY_NAME,
                          &dwType,
                          ( PUCHAR ) & dwData,
                          &dwSize ) )
    {
		m_dwCSCInterruptDelay = dwData;
    }
	else
	{
		m_dwCSCInterruptDelay = 200;
	}

    dwRet = ERROR_SUCCESS;

grc_fail: 
    return dwRet;
}   // GetRegistryConfig

BOOL CPcmciaBusBridge::InitCardBusBridge( void )
{
    DWORD dwRet;

    // Create critical section protecting the PCIC registers
    __try
    {
        InitializeCriticalSection( &m_PCICCritSec );
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        DEBUGMSG( ZONE_PDD,
                  ( TEXT( "PDCardInitServices InitializeCriticalSection failed %d\r\n" ) ) );
        return FALSE;
    }
    m_fPCICCritSecInitialized = true;

    // Get registry information
    if( ( dwRet = GetRegistryConfig() ) != ERROR_SUCCESS )
    {
        DEBUGMSG( ZONE_PDD,
                  ( TEXT( "PDCardInitServices GetRegistryConfig failed %d\r\n" ),
                    dwRet ) );
        return FALSE;
    }

	// Allocate PCMCIA buffers.
	m_vpIOPRegs = (S3C2410X_IOPORT_REG*)VirtualAlloc(0, sizeof(S3C2410X_IOPORT_REG), MEM_RESERVE, PAGE_NOACCESS);
	if (m_vpIOPRegs == NULL) 
	{
		DEBUGMSG (1,(TEXT("m_vpIOPRegs is not allocated\n\r")));
		goto pcis_fail;
	}
	if (!VirtualCopy((PVOID)m_vpIOPRegs, (PVOID)(S3C2410X_BASE_REG_PA_IOPORT >> 8), sizeof(S3C2410X_IOPORT_REG), PAGE_PHYSICAL|PAGE_READWRITE|PAGE_NOCACHE)) {
		DEBUGMSG (1,(TEXT("m_vpIOPRegs is not mapped\n\r")));
		goto pcis_fail;
	}
	DEBUGMSG (1,(TEXT("m_vpIOPRegs is mapped to %x\n\r"), m_vpIOPRegs));
	
	m_vpMEMRegs = (S3C2410X_MEMCTRL_REG*)VirtualAlloc(0,sizeof(S3C2410X_MEMCTRL_REG), MEM_RESERVE,PAGE_NOACCESS);
	if(m_vpMEMRegs == NULL) 
	{
		DEBUGMSG (1,(TEXT("m_vpMEMRegs is not allocated\n\r")));
		goto pcis_fail;
	}
	if(!VirtualCopy((PVOID)m_vpMEMRegs,(PVOID)(S3C2410X_BASE_REG_PA_MEMCTRL >> 8),sizeof(S3C2410X_MEMCTRL_REG), PAGE_PHYSICAL|PAGE_READWRITE|PAGE_NOCACHE)) {

		DEBUGMSG (1,(TEXT("m_vpMEMRegs is not mapped\n\r")));
		goto pcis_fail;
	}    
	DEBUGMSG (1,(TEXT("m_vpMEMRegs is mapped to %x\n\r"), m_vpMEMRegs));

	m_vpPCMCIAPort = (PUCHAR*)VirtualAlloc(0, 0x0400, MEM_RESERVE,PAGE_NOACCESS);
	if(m_vpPCMCIAPort == NULL) 
	{
		DEBUGMSG (1,(TEXT("m_vpPCMCIAPort is not allocated\n\r")));
		goto pcis_fail;
	}
	if(!VirtualCopy((PVOID)m_vpPCMCIAPort,(PVOID)PD6710_IO_BASE_ADDRESS, 0x0400, PAGE_READWRITE|PAGE_NOCACHE)) {
		DEBUGMSG (1,(TEXT("m_vpPCMCIAPort is not mapped\n\r")));
		goto pcis_fail;
	}    
	DEBUGMSG (1,(TEXT("m_vpPCMCIAPort is mapped to %x\n\r"), m_vpPCMCIAPort));
	
	// Initialize S3C2410 for PD6710           
	// EINT3(GPF3) is enabled.
	m_vpIOPRegs->GPFCON = (m_vpIOPRegs->GPFCON & ~(0x3<<6)) | (0x2<<6); 
	// EINT3 is PULLUP enabled.
	m_vpIOPRegs->GPFUP = (m_vpIOPRegs->GPFUP & ~(0x1<<3));              

	// EINT8(GPG0) is enabled.
	m_vpIOPRegs->GPGCON = (m_vpIOPRegs->GPGCON & ~(0x3<<0)) | (0x2<<0); 
	// EINT8 is *not* PULLUP enabled.
	m_vpIOPRegs->GPGUP = (m_vpIOPRegs->GPGUP | (0x1<<0));

	
	// nGCS2=nUB/nLB(nSBHE),nWAIT,16-bit
	m_vpMEMRegs->BWSCON = (m_vpMEMRegs->BWSCON & ~(0xf<<8)) | (0xd<<8); 

	// BANK2 access timing
	m_vpMEMRegs->BANKCON2 = ((B6710_Tacs<<13)+(B6710_Tcos<<11)+(B6710_Tacc<<8)+(B6710_Tcoh<<6)\
		+(B6710_Tah<<4)+(B6710_Tacp<<2)+(B6710_PMC));

	
	// EINT8=Level-high triggered, IRQ3.
	// EINT3=Falling Edge triggering -> connected INTR(controller)
	m_vpIOPRegs->EXTINT1=(m_vpIOPRegs->EXTINT1 & ~(0xf<<0)) | (0x1<<0); 
	m_vpIOPRegs->EXTINT0=(m_vpIOPRegs->EXTINT0 & ~(0xf<<12)) | (0x2<<12); 
		
	m_PCICIndex = ((PUCHAR)((ULONG)m_vpPCMCIAPort+0x3e0));
	m_PCICData = ((PUCHAR)((ULONG)m_vpPCMCIAPort+0x3e1));

	DEBUGMSG(1, (TEXT("PDCardInitServices m_PCICIndex = 0x%x, m_PCICData = 0x%x\r\n"),
		          m_PCICIndex, m_PCICData));

    if( !IsValidPCICSig() )
    {
        return FALSE;
    }

    DEBUGMSG( ZONE_PDD,
              ( TEXT( "PDCardInitServices m_PCICIndex = 0x%x, m_PCICData = 0x%x\r\n" ),
                m_PCICIndex,
                m_PCICData ) );

    UINT8 tmp = ReadPCICRegister( REG_CHIP_REVISION );
    DEBUGMSG( ZONE_PDD,
              ( TEXT( "PDCardInitServices REG_CHIP_REVISION = 0x%x\r\n" ),
                tmp ) );

    InstallIsr();

    return TRUE;

pcis_fail:
    return FALSE;
}

#define DEFAULT_PRIORITY 101

BOOL CPcmciaBusBridge::Init()
{
    if( !loadPcCardEntry() )
    {
        return FALSE;
    }

    if( !InitCardBusBridge() )
    {
        return FALSE;
    }

    if( !GetSocketNumberFromCS( TRUE ) )
    {
        return FALSE;
    }

    // Set Tread Priority.
    m_uPriority = DEFAULT_PRIORITY;
    DWORD dwRegPriority = 0;
    if( GetRegValue( RegPriority256,
                     ( PBYTE ) & dwRegPriority,
                     sizeof( dwRegPriority ) ) )
    {
        m_uPriority = ( UINT ) dwRegPriority;
    }
    // Get Thread Priority from Registry and Set it.
    CeSetPriority( m_uPriority );
    m_bTerminated = FALSE;

    ThreadStart();
    return TRUE;
}
BOOL CPcmciaBusBridge::GetRegPowerOption( PDWORD pOption )
{
    if( pOption )
    {
        return GetRegValue( RegPowerOption,
                            ( PBYTE ) pOption,
                            sizeof( DWORD ) );
    }
    return TRUE;
}
void CPcmciaBusBridge::PowerMgr( BOOL bPowerDown )
{
    if( bPowerDown )
    {
        if( m_pCardSocket )
        {
            // Disable client interrupts
            WritePCICRegister( REG_INTERRUPT_AND_GENERAL_CONTROL,
                ReadPCICRegister( REG_INTERRUPT_AND_GENERAL_CONTROL ) & 0xf0 );
            // Shut down socket any way.
            WritePCICRegister( REG_POWER_CONTROL, 0 );
        }
    }
    else
    {
        // Initialize S3C2410 for PD6710           
        // EINT3(GPF3) is enabled.
        m_vpIOPRegs->GPFCON = (m_vpIOPRegs->GPFCON & ~(0x3<<6)) | (0x2<<6); 

        // EINT3 is PULLUP enabled.
        m_vpIOPRegs->GPFUP = (m_vpIOPRegs->GPFUP & ~(0x1<<3));              

        // EINT8(GPG0) is enabled.
        m_vpIOPRegs->GPGCON = (m_vpIOPRegs->GPGCON & ~(0x3<<0)) | (0x2<<0); 
        // EINT8 is *not* PULLUP enabled.
        m_vpIOPRegs->GPGUP = (m_vpIOPRegs->GPGUP | (0x1<<0));


        // nGCS2=nUB/nLB(nSBHE),nWAIT,16-bit
        m_vpMEMRegs->BWSCON = (m_vpMEMRegs->BWSCON & ~(0xf<<8)) | (0xd<<8); 

        // BANK2 access timing
        m_vpMEMRegs->BANKCON2 = ((B6710_Tacs<<13)+(B6710_Tcos<<11)+(B6710_Tacc<<8)+(B6710_Tcoh<<6)\
            +(B6710_Tah<<4)+(B6710_Tacp<<2)+(B6710_PMC));


        // EINT8=Level-high triggered, IRQ3.
        // EINT3=Falling Edge triggering -> connected INTR(controller)
        m_vpIOPRegs->EXTINT1=(m_vpIOPRegs->EXTINT1 & ~(0xf<<0)) | (0x1<<0); 
        m_vpIOPRegs->EXTINT0=(m_vpIOPRegs->EXTINT0 & ~(0xf<<12)) | (0x2<<12); 

        if( m_pCardSocket )
        {
            m_fCardInjectEvent = TRUE;
            SetEvent( m_hISTEvent );
        }
    } 
}

BOOL CPcmciaBusBridge::NeedPowerResuming()
{
    m_bResumeFlag = TRUE;
    SetInterruptEvent( m_dwCSCSysIntr );
    return TRUE;
}

void CPcmciaBusBridge::PowerCycleEvent()
{
    Lock();
    m_fPowerCycleEvent = TRUE;
    SetEvent( m_hISTEvent );
    Unlock();
}

void CPcmciaBusBridge::CardInjectEvent()
{
    Lock();
    m_fCardInjectEvent = TRUE;
    SetEvent( m_hISTEvent );
    Unlock();
}

DWORD CPcmciaBusBridge::ThreadRun() // THIS is CardBusBridge IST.
{
    // Test if a PCMCIA card is already inserted in one of the sockets
    UINT8 bCardStatusChange = 0 ;
    if( ( ReadPCICRegister( REG_INTERFACE_STATUS ) 
            & ( STS_CD1 | STS_CD2 ) ) == ( STS_CD1 | STS_CD2 ) )
    {
        bCardStatusChange |= CSC_DETECT_CHANGE;
        SetEvent( m_hISTEvent );
    }

    DEBUGCHK(m_hISTEvent!=NULL);

    // run until signalled to terminate
    while( !m_bTerminated && m_hISTEvent )
    {
        BOOL bInterrupt = (WaitForSingleObject(m_hISTEvent,m_dwPollTimeout)!=WAIT_TIMEOUT);
        if( m_fPollingMode ) // we are polling, fake an interrupt event
        {
            bInterrupt = true;
        }

        if (!bInterrupt) { // we have reached a timeout in non-polling mode or something bad has occured.
            continue;
        }

        Lock();

        UINT8 bSocketState; 
        UINT8 fNotifyEvents;

        while (TRUE) {
            Sleep(20);
            // See what changed and acknowledge any status change interrupts
            fNotifyEvents = 0;
            bCardStatusChange |= ReadPCICRegister( REG_CARD_STATUS_CHANGE );
            // if nothing has changed, continue until next event
            if( !m_fPowerCycleEvent &&
                !m_fCardInjectEvent &&
                bCardStatusChange == 0x0 )
            {
                break;
            }

            // Figure out the socket state
            bSocketState = ReadPCICRegister( REG_INTERFACE_STATUS );

            if( bCardStatusChange & CSC_DETECT_CHANGE )
            {
                fNotifyEvents |= EVENT_MASK_CARD_DETECT;
            }

            if( m_fCardInjectEvent )
            {
                fNotifyEvents |= EVENT_MASK_CARD_DETECT;
                m_fCardInjectEvent = FALSE;
            }

            if( m_fPowerCycleEvent )
            {
                fNotifyEvents |= SPS_POWERCYCLE;
                m_fPowerCycleEvent = FALSE;
            }

            if( fNotifyEvents & EVENT_MASK_CARD_DETECT )
            {
                // we're only processing Card Detection Signal.
                // If this happens, Something has been changed. 
                // Send out Card revmoval before doing any more process.
                RemovePcmciaCardSocket();
                ProcessCDD( bSocketState );
            }
            else
            {
                // Other Event left for type specific socket
                if( m_pCardSocket )
                    m_pCardSocket->SocketEventHandle( bCardStatusChange, fNotifyEvents );
            }
            bCardStatusChange = 0 ; 
        }
        Unlock();
        
        if( !m_fPollingMode )
        {
            DEBUGMSG( ZONE_FUNCTION,
                      ( L"CardBus: PCCardBus Call InterruptDone\r\n" ) );
            InterruptDone( m_dwCSCSysIntr );
        }
    } // while(!m_bTerminated) 
    return 0;
}

void CPcmciaBusBridge::ProcessCDD( DWORD dwStateReg )
{
    switch( dwStateReg & ( STS_CD1 | STS_CD2 ) )
    {
      case 0x0:
        // Complete removal
        if( m_pCardSocket )
        {
            RemovePcmciaCardSocket();
        }
        // Socket Detached.

        break;
      case (STS_CD1|STS_CD2):
        // Complete Insertion.
        if( m_pCardSocket == NULL )
        {
            InsertPcmciaCardSocket( CreatePCMCIASocket( this ) );
        }
        break;
      default:
        break;
    }
}

void CPcmciaBusBridge::CallBackToCardService( HANDLE hSocket, PSS_SOCKET_STATE pSocketState )
{
    if( m_pCallBackToCardService )
    {
        __try
        {
            m_pCallBackToCardService( hSocket,
                                      m_uSocketNum,
                                      pSocketState );
        } __except( EXCEPTION_EXECUTE_HANDLER )
        {
            DEBUGCHK( FALSE );
        }
    }
    else
    {
        DEBUGCHK( FALSE );
    }
}
BOOL CPcmciaBusBridge::GetSocketNumberFromCS( BOOL bGet )
{
    STATUS status = CERR_BAD_ARGS;
    if( m_pRequestSocketNumber && m_pDeleteSocket )
    {
        __try
        {
            status = ( bGet ?
                       m_pRequestSocketNumber( &m_uSocketNum,
                                               sizeof( SMDK2410SocketServiceStatic ),
                                               &SMDK2410SocketServiceStatic,
                                               GetSocketName() ) :
                       m_pDeleteSocket( m_uSocketNum ) );
        } __except( EXCEPTION_EXECUTE_HANDLER )
        {
            DEBUGCHK( FALSE );
            status = CERR_BAD_ARGS;
        }
    }
    DEBUGCHK( status == CERR_SUCCESS );
    return ( status == CERR_SUCCESS );
}
