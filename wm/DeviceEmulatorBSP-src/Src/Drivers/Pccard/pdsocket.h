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
#ifndef __PDSOCKET_H_
#define __PDSOCKET_H_

#include "debug.h"
#include "CRegEdit.h"
#include "CRefCon.h"
#include "CMThread.h"
#include "resource.h"
#include <socksv2.h>
#include <PcmciaCardLib.h>
#include <s3c2410x.h> 
#include <pd6710.h>

// Special Registry Value for this drivers.
#define RegPriority256 TEXT("Priority256")
#define RegPowerOption TEXT("RegPowerOption")
#define REG_POWER_KEEP_DRIVER 1
#define REG_POWER_KEEP_POWER 2

#define REG_SOCKET_EVENT                     0x00
#define REG_SOCKET_MASK                      0x04

#define SPS_POWERCYCLE                       0x00000008

class CCardBusResource;
class CPcmciaBusBridge;

typedef struct _PDCARD_SOCKET_STATE
{
    UINT8   fSocketCaps;          // @field Socket capabilities
    UINT8   fInterruptEvents;     // @field Status change interrupt mask.  The initial state of this field
    //        indicates which events can cause a status change interrupt.
    UINT8   fNotifyEvents;        // @field Latched socket state
    UINT8   fControlCaps;         // @field Control capabilities
    UINT8   fInterfaceType;       // @field Memory-only or memory and I/O
    UINT8   fIREQRouting;         // @field Enable/disable IREQ
    UINT8   fVcc;                 // @field Vcc power entry index and status
    UINT8   uVpp1;                // @field Vpp1 power entry index
    UINT8   uVpp2;                // @field Vpp2 power entry index
} PDCARD_SOCKET_STATE, * PPDCARD_SOCKET_STATE;

#define NUM_POWER_ENTRIES 4
#define REG_CHIP_REVISION 0x00

class CPcmciaCardSocket : public CRefObject, public CLockObject
{
    // Interface Class
public:
                    CPcmciaCardSocket( CPcmciaBusBridge* pBriedge ) ;
    virtual         ~CPcmciaCardSocket() ;

    virtual void    SocketEventHandle( DWORD dwEvent, DWORD dwPresentStateReg ) = 0; // Event Handle from Interrupt.

    // Socket
    virtual STATUS  CardInquireSocket( PSS_SOCKET_INFO pSocketInfo ) = 0 ;
    virtual STATUS  CardGetSocket( PSS_SOCKET_STATE pState ) = 0;
    virtual STATUS  CardSetSocket( PSS_SOCKET_STATE pState ) = 0;
    virtual STATUS  CardResetSocket() = 0;

    // Socket Windows
    virtual STATUS  CardInquireWindow( UINT32 uWindow,
                                       PSS_WINDOW_INFO pWinInfo ) = 0;
    virtual STATUS  CardSetWindow( UINT32 uWindow,
                                   PSS_WINDOW_STATE pWindowState ) = 0;
    virtual STATUS  CardGetWindow( UINT32 uWindow,
                                   PSS_WINDOW_STATE pWindowState ) = 0;

    virtual STATUS  GetPowerEntry( PDWORD pdwNumOfEnery,
                                   PSS_POWER_ENTRY pPowerEntry );

    virtual STATUS CardAccessMemory( PSS_MEMORY_ACCESS /*pMemoryAccess*/ )
    {
        DEBUGCHK( FALSE );
        return CERR_UNSUPPORTED_SERVICE;
    }
    // Service.
    // Power Down and Up
    virtual void    PowerMgr( BOOL bPowerDown ) = 0;
    virtual BOOL    Resuming() = 0;

    HANDLE GetSocketHandle()
    {
        return ( HANDLE ) m_dwSocketIndex;
    };
    UINT16                      GetSocketNo(); // returns the card socket number (as reported by the PF_CS_ADD_SOCKET call

    static const SS_POWER_ENTRY m_rgPowerEntries[NUM_POWER_ENTRIES];

    CPcmciaBusBridge*           m_pBridge;
    DWORD                       m_dwSocketIndex;

    //private:
    static DWORD    ms_dwSocketLastIndex;
};

#define NUM_SLOTS 1

class CPcmciaBusBridge : public CPCCardBusBridgeBase,
                         public CRefObject,
                         public CLockObject,
                         public CMiniThread
{
public:
    CPcmciaBusBridge( LPCTSTR RegPath );
    ~CPcmciaBusBridge( void );
    BOOL    Init();

    void WritePCICRegister( UINT uOffset, BYTE bValue )
    {
        EnterCriticalSection( &m_PCICCritSec );
        PCICIndex( uOffset );
        PCICDataWrite( bValue );
        LeaveCriticalSection( &m_PCICCritSec );
    }

    BYTE ReadPCICRegister( UINT uOffset )
    {
        EnterCriticalSection( &m_PCICCritSec );
        PCICIndex( uOffset );
        BYTE    bRetVal = PCICDataRead();
        LeaveCriticalSection( &m_PCICCritSec );
        return bRetVal;
    }

    BOOL NeedPowerResuming();
    BOOL NeedReinitAfterPowerDown()
    {
        return FALSE;
    };
    BOOL    GetRegPowerOption( PDWORD pOption );

    void    PowerCycleEvent();
    void    CardInjectEvent();

    void    CallBackToCardService( HANDLE hSocket,
                                   PSS_SOCKET_STATE pSocketState );

    DWORD   RequestSocketNumber( PSS_SOCKET_SERVICE pSocketService );

    UINT16 GetSocketNo()
    {
        return m_uSocketNum;
    };
    DWORD GetFnInterruptNo()
    {
        return m_dwControllerFnIntNo;
    }
    DWORD GetFnSysInt()
    {
        return m_dwFnSysIntr;
    }
    DWORD GetFnIRQ()
    {
        return m_dwFnIrq;
    }
    BOOL PollingMode()
    {
        return m_fPollingMode;
    }
    BOOL LockOwner( DWORD dwTimeout = INFINITE )
    {
        return m_OwnerLock.Lock( dwTimeout );
    };
    BOOL ReleaseOwner()
    {
        return m_OwnerLock.Unlock();
    };
    void                            PowerMgr( BOOL bPowerDown );
#ifdef DEBUG
    VOID                            DumpAllRegisters();
#endif

private:
    volatile S3C2410X_IOPORT_REG *m_vpIOPRegs;
    volatile S3C2410X_MEMCTRL_REG *m_vpMEMRegs;
    volatile PUCHAR *m_vpPCMCIAPort;  

    CMuTexObject                    m_OwnerLock;

    UINT                            m_uPriority; // IST Thread Priority.
    virtual DWORD                   ThreadRun(); // IST
    void                            InsertPcmciaCardSocket( CPcmciaCardSocket* pSocket );
    void                            RemovePcmciaCardSocket();

    STATUS                          PDCardGetSocketState( UINT32 uSocket,
                                                          PPDCARD_SOCKET_STATE pState );

    UINT8                           m_vRevision;
    volatile PUCHAR                 m_PCICIndex;
    volatile PUCHAR                 m_PCICData;
    CRITICAL_SECTION                m_PCICCritSec;
    BOOL                            m_fPCICCritSecInitialized; // critical section protecting access to the PCIC registers

    DWORD                           m_dwFnSysIntr; // Function Interrupt ID
    DWORD                           m_dwFnIrq; // Function Interrupt IRQ
    DWORD                           m_dwControllerFnIntNo; // Function interrupt number
    DWORD                           m_dwCSCSysIntr; // CSC Interrupt ID
    HANDLE                          m_hISTEvent;
    DWORD                           m_dwPollTimeout;
	DWORD							m_dwCSCInterruptDelay;

    BOOL                            m_fPowerCycleEvent;
    BOOL                            m_fCardInjectEvent;
    CPcmciaCardSocket*              m_pCardSocket;
    UINT16                          m_uSocketNum;

    // IST 
    BOOL                            m_bTerminated; // IST Thread Control
    BOOL                            m_bResumeFlag;
    static const SS_SOCKET_SERVICE  SMDK2410SocketServiceStatic;

    // Private Function
    BOOL                            InstallIsr();
    BOOL                            InitCardBusBridge( void );
    void                            ProcessCDD( DWORD dwPStateReg );

    VOID                            PCICIndex( UINT8  register_num );
    VOID                            PCICDataWrite( UINT8 value );
    UINT8                           PCICDataRead( VOID );
    BOOL                            IsValidPCICSig( void );
    DWORD                           GetRegistryConfig();
    BOOL                            GetSocketNumberFromCS( BOOL bGet );

    CCardBusResource*               m_pCardBusResource;
    BOOL                            m_fPollingMode;
};
CPcmciaCardSocket* CreatePCMCIASocket( CPcmciaBusBridge* pBridge );
CPcmciaCardSocket* GetSocket( HANDLE hSocket );
#endif
