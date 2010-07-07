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

    Samsung SMDK2410 PCMCIA Socket Services Object for PCMCIA(16-bit) Interface.

Notes: 
--*/
#include <windows.h>
#include <types.h>
#include <socksv2.h>
#include <memory.h>
#include <ceddk.h>
#include <debug.h>
#define ZONE_SOCKET ZONE_PDD

#include "PCMSock.h"

//--------------------Pcmcia Socket Implementation--------------------------------------------
const SS_SOCKET_INFO CPCMSocket::ms_SocketInitInfo =
{
    SOCK_CAP_MEM_CARD |
    SOCK_CAP_IO_MEMORY_CARD,      //dwSocketCaps
    SOCK_CAP_PWRCYCLE |
    SOCK_CAP_CD |
    SOCK_CAP_SUSPEND_RESUME |
    SOCK_CAP_WP |
    SOCK_CAP_BVD1 |
    SOCK_CAP_BVD2,  // Socket Interrupt Capability Set follow bitmap.
    SOCK_CAP_PWRCYCLE |
    SOCK_CAP_CD |
    SOCK_CAP_SUSPEND_RESUME |
    SOCK_CAP_WP |
    SOCK_CAP_BVD1 |
    SOCK_CAP_BVD2,  // Socket status report capability
    0,              // Socket status indicate capability
    0,              // Number of power entry this socket have.
    0,              // Number of Window supported in this Socket.
    Internal,       // Bus Type is Internal.
    (DWORD) -1,     // Bus Number.
    { - 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}
};
const SS_SOCKET_STATE CPCMSocket::ms_SocketInitState =
{
    SOCK_EVENT_PWRCYCLE |
    SOCK_EVENT_CD |
    SOCK_EVENT_SUSPEND_RESUME |
    SOCK_EVENT_WP |
    SOCK_EVENT_BVD1 |
    SOCK_EVENT_BVD2, // Initial EventMask
    0, // Event Changed.
    0, // Event Status
    CFG_IFACE_MEMORY,  //default interface type
    0,  // Interrupt Enable.
    0,  // Vcc
    0,  // Vpp1
    0   // Vpp2

};

CPCMSocket::CPCMSocket( CPcmciaBusBridge* pBridge ) : CPCMCIASocketBase<CPcmciaMemWindows, CPcmciaIoWindows, CPcmciaCardSocket, CPcmciaBusBridge>( pBridge )
{
    DEBUGMSG( ZONE_SOCKET, ( TEXT( "+CPCMSocket::CPCMSocket\r\n" ) ) );
    m_SocketState = ms_SocketInitState;
    m_SocketInfo = ms_SocketInitInfo;

    if( pBridge->GetFnIRQ() == -1 )
    {
        m_SocketInfo.dwSocketCaps |= SOCK_CAP_ONLY_SYSINTR;
    }

    for( DWORD dwIndex = 0; dwIndex < IRQ_ROUTINGTABLE_SIZE; dwIndex++ )
    {
        if( m_SocketInfo.dwSocketCaps & SOCK_CAP_ONLY_SYSINTR )
        {
            m_SocketInfo.bArrayIrqRouting[dwIndex] = ( BYTE )pBridge->GetFnSysInt();
        }
        else
        {
            m_SocketInfo.bArrayIrqRouting[dwIndex] = ( BYTE )pBridge->GetFnIRQ();
        }
    }
    m_SocketInfo.dwNumOfPowerEntry = NUM_POWER_ENTRIES;
    m_SocketInfo.dwNumOfWindows = pBridge->GetMemWindowCount() +
                                          pBridge->GetIoWindowCount();
    m_SocketInfo.dwBusNumber = 0;

    if( !pBridge->PollingMode() )
    {
        PCICRegisterWrite( REG_STATUS_CHANGE_INT_CONFIG,
                        CFG_BATTERY_DEAD_ENABLE |
                        CFG_BATTERY_WARNING_ENABLE |
                        CFG_CARD_DETECT_ENABLE |
                        CFG_READY_ENABLE );
    }
    m_bResuming = FALSE;

    DEBUGMSG( ZONE_SOCKET, ( TEXT( "-CPCMSocket::CPCMSocket\r\n" ) ) );
}
CPCMSocket::~CPCMSocket()
{
    DEBUGMSG( ZONE_SOCKET, ( TEXT( "+CPCMSocket::Deinitialize()\r\n" ) ) );
    Lock();
    BOOL bInitWindow = CardDeInitWindow();
    DEBUGCHK( bInitWindow );
    PCICRegisterWrite( REG_POWER_CONTROL,
                       PCICRegisterRead( REG_POWER_CONTROL ) & ~PWR_OUTPUT_ENABLE );
    Unlock();
    DEBUGMSG( ZONE_SOCKET, ( TEXT( "-CPCMSocket::Deinitialize()\r\n" ) ) );
}

//
// GetSocket - return Socket state
// [In/Out] pBuf - pointer to SS_SOCKET_STATE struct
// [In/Out] pnBuf - pointer to size of struct pointed to by pBuf
// return CERR_SUCCESS on success, CERR_UNSUPPORTED_SERVICE if size is anomolous
//
STATUS CPCMSocket::CardGetSocket( PSS_SOCKET_STATE pState )
{
    DWORD dwReg;

    DEBUGMSG( ZONE_SOCKET, ( TEXT( "+CPCMSocket::GetSocket()\r\n" ) ) );
    Lock();
    if( pState )
    {
        // Read voltage information
        dwReg = PCICRegisterRead( REG_POWER_CONTROL );
        if( dwReg & PWR_VCC_POWER )
        {
            dwReg = PCICRegisterRead( REG_GENERAL_CONTROL );
            if( dwReg & MISC1_VCC_33 ) // Vcc = 3V
            {
                // Vcc = 3V
                m_SocketState.fVcc = 0x1;
            }
            else
            {
                // Vcc = 5V
                m_SocketState.fVcc = 0x2;
            }
        }
        else
        {
            // Vcc = 0V
            m_SocketState.fVcc = 0x0;
        }
        m_SocketState.uVpp1 = m_SocketState.uVpp2 = 0; //m_SocketState.fVcc;

        BYTE uStatusChange = 0;
        m_SocketState.dwEventChanged |= ( ( uStatusChange & CSC_DETECT_CHANGE ) !=
                                        0 ?
                                        SOCK_EVENT_CD :
                                        0 );
        m_SocketState.dwEventChanged |= ( ( uStatusChange & CSC_BATTERY_DEAD_OR_STS_CHG ) !=
                                        0 ?
                                        SOCK_EVENT_BVD1 :
                                        0 );
        m_SocketState.dwEventChanged |= ( ( uStatusChange & CSC_BATTERY_WARNING ) !=
                                        0 ?
                                        SOCK_EVENT_BVD2 :
                                        0 );
        m_SocketState.dwEventChanged |= ( ( uStatusChange & CSC_READY_CHANGE ) !=
                                        0 ?
                                        SOCK_EVENT_READY :
                                        0 );

        BYTE uCurrentStatus = PCICRegisterRead( REG_INTERFACE_STATUS );
        m_SocketState.dwEventStatus = 0;
        m_SocketState.dwEventStatus |= ( ( uCurrentStatus & ( STS_CD1 | STS_CD2 ) ) ==
                                       ( STS_CD1 | STS_CD2 ) ?
                                       SOCK_EVENT_CD :
                                       0 );
        m_SocketState.dwEventStatus |= ( ( uCurrentStatus & STS_BVD1 ) ==
                                       0 ?
                                       SOCK_EVENT_BVD1 :
                                       0 );
        m_SocketState.dwEventStatus |= ( ( uCurrentStatus & STS_BVD2 ) ==
                                       0 ?
                                       SOCK_EVENT_BVD2 :
                                       0 );
        m_SocketState.dwEventStatus |= ( ( uCurrentStatus & STS_CARD_READY ) !=
                                       0 ?
                                       SOCK_EVENT_READY :
                                       0 );
        m_SocketState.dwEventStatus |= ( ( uCurrentStatus & STS_WRITE_PROTECT ) !=
                                       0 ?
                                       SOCK_EVENT_WP :
                                       0 );

        m_SocketState.dwEventStatus &= ~SOCK_EVENT_CARDBUS_CARD;
        *pState = m_SocketState;
        pState->dwEventChanged &= pState->dwEventMask;

        DEBUGMSG( ZONE_SOCKET,
                  ( TEXT( "-CPCMSocket::GetSocket(): dwEventMask: %x dwEventChanged: %x dwEventStatus: %x\r\n" ),
                    pState->dwEventMask,
                    pState->dwEventChanged,
                    pState->dwEventStatus ) );
    }
    Unlock();
    return CERR_SUCCESS;
}
void CPCMSocket::PowerMgrCallback( BOOL bPowerOff )
{
    SS_SOCKET_STATE sSocketState = m_SocketState;
    sSocketState.dwEventChanged = SOCK_EVENT_SUSPEND_RESUME;
    sSocketState.dwEventStatus = ( bPowerOff ?
                                   SOCK_EVENT_CD :
                                   ( SOCK_EVENT_CD |
                                     SOCK_EVENT_SUSPEND_RESUME ) );
    m_pBridge->CallBackToCardService( GetSocketHandle(),
                                      &sSocketState );
}

void CPCMSocket::PowerMgr( BOOL bPowerDown )
{
    if( bPowerDown )
    {
        // Power Off Socket If it is applied.
        PowerMgrCallback( bPowerDown );
        if( ( m_SocketState.dwInteruptStatus & SOCK_INT_FUNC_IRQ_WAKE ) == 0 ) // We can shut down this function card
        {
            m_bBackupPCICPwrCtrlReg = PCICRegisterRead( REG_POWER_CONTROL );
            // Disable Socket Power.
            PCICRegisterWrite( REG_POWER_CONTROL, 0 );
            m_bResuming = TRUE;
        }
    }
    else
    {
        if( m_bResuming )
        {
            PCICRegisterWrite( REG_POWER_CONTROL, m_bBackupPCICPwrCtrlReg );
            m_pBridge->NeedPowerResuming();
        }
        else
        {
            PowerMgrCallback( bPowerDown );
        }
    }
}
BOOL CPCMSocket::Resuming()
{
    if( m_bResuming )
    {
        Lock();
        DWORD dwStartTickCount = GetTickCount();
        // Wait for 100MS for power set .
        while( GetTickCount() - dwStartTickCount < 100 )
            Sleep( 0 ); //Yeild among same priority thread.
        //Assert Resetting.
        dwStartTickCount = GetTickCount();
        while( GetTickCount() - dwStartTickCount < 20 )
            Sleep( 0 ); //Yeild among same priority thread.
        // Deassert Reset.
        PCICRegisterWrite( REG_INTERRUPT_AND_GENERAL_CONTROL,
                           INT_CARD_NOT_RESET/*|INT_ENABLE_MANAGE_INT*/ ); 
        dwStartTickCount = GetTickCount();
        while( GetTickCount() - dwStartTickCount < 20 )
            Sleep( 0 ); //Yeild among same priority thread.
        m_bResuming = FALSE;
        dwStartTickCount = GetTickCount();

        PowerMgrCallback( FALSE );
        Unlock();
        return TRUE;
    }
    return FALSE;
}

#ifdef DEBUG
VOID CPCMSocket::DumpAllRegisters()
{
    DEBUGMSG( ZONE_FUNCTION,
        ( TEXT( "Dumping all PCIC registers:\r\n" ) ) );
    for( UINT8 nRegNum = 0; nRegNum < 0x40; nRegNum++ )
    {
        UINT8 val = PCICRegisterRead( nRegNum );
        DEBUGMSG( ZONE_FUNCTION,
                  ( TEXT( "PCIC Register %02x: %02x\r\n" ), nRegNum, val ) );
    }
}
#endif DEBUG

//
// PDCardSetSocket
//
// @func    STATUS | PDCardSetSocket | Set the socket state of the specified socket.
// @rdesc   Returns one of the CERR_* return codes in cardserv.h.
//
// @comm    This function sets the specified socket's state and adjusts the socket
//          controller appropriately.
//          PDCardGetSocketState will usually be called first and adjustments will
//          be made to the PDCARD_SOCKET_STATE structure before PDCardSetSocketState
//          is called.  This avoids duplicated socket state on different layers and
//          it avoids unintentionally setting socket parameters.
//
// @xref <f PDCardGetSocketState>
//

STATUS CPCMSocket::CardSetSocket( PSS_SOCKET_STATE pState )
{
    Lock();
    STATUS status = CERR_SUCCESS;
    if( m_pBridge && pState )
    {
        m_SocketState.dwEventMask = pState->dwEventMask;
        m_SocketState.dwEventChanged &= ~pState->dwEventChanged;

        BYTE tmp = PCICRegisterRead( REG_INTERRUPT_AND_GENERAL_CONTROL ) & ( INT_RING_INDICATE_ENABLE |
                                                                             INT_ENABLE_MANAGE_INT |
                                                                             INT_CARD_NOT_RESET |
                                                                             0x0f );
        tmp |= INT_CARD_NOT_RESET ;
        if( ( pState->dwInteruptStatus & SOCK_INT_FUNC_IRQ_ROUTING ) !=
            ( m_SocketState.dwInteruptStatus & SOCK_INT_FUNC_IRQ_ROUTING ) )
        {
            if( pState->dwInteruptStatus & SOCK_INT_FUNC_IRQ_ROUTING )
            {
                tmp |= GetPCCardBusBridge()->GetFnInterruptNo(); // enable interrupt routing

            }
            else
            {
                tmp &= 0xf0; // disable interrupt routing
            }

            m_SocketState.dwInteruptStatus = pState->dwInteruptStatus;
            m_SocketState.fIREQRouting = pState->fIREQRouting;
        };

        if( ( pState->dwInterfaceType & CFG_IFACE_MEMORY_IO ) != 0 )
        {
            tmp |= INT_CARD_IS_IO;
        }

        m_SocketState.dwInterfaceType = pState->dwInterfaceType;
        PCICRegisterWrite( REG_INTERRUPT_AND_GENERAL_CONTROL, tmp );

        if( ( pState->dwEventStatus & SOCK_EVENT_EJECT ) != 0 ||
            ( pState->dwEventStatus & SOCK_EVENT_INSERT ) != 0 )
        {
            // Ask for ejection of the card.
            DEBUGMSG( ZONE_PDD,
                      ( TEXT( "PDCardSetSocket  CPCMSocket. Gernerate Artificial Eject\r\n" ) ) );
            CardInjectEvent();
        }
        else if( ( pState->fVcc & SOCK_VCC_LEVEL_MASK ) >=
                 NUM_POWER_ENTRIES ||
                 !( m_rgPowerEntries[pState->fVcc & SOCK_VCC_LEVEL_MASK].fSupply & PWR_SUPPLY_VCC ) )
        {
            pState->fVcc = m_SocketState.fVcc;
            status = CERR_BAD_VCC;
        }
        else
        {
            BYTE oldfVcc = m_SocketState.fVcc;
            BYTE olduVpp = m_SocketState.uVpp1;
            if( ( pState->fVcc & SOCK_VCC_LEVEL_MASK ) !=
                ( m_SocketState.fVcc & SOCK_VCC_LEVEL_MASK ) )
            {
                m_SocketState.fVcc = pState->fVcc;
            }
            if( ( pState->uVpp1 & SOCK_VCC_LEVEL_MASK ) !=
                ( m_SocketState.uVpp1 & SOCK_VCC_LEVEL_MASK ) )
            {
                if( ( pState->uVpp1 & SOCK_VCC_LEVEL_MASK ) >=
                    NUM_POWER_ENTRIES ||
                    !( m_rgPowerEntries[pState->uVpp1 & SOCK_VCC_LEVEL_MASK].fSupply & PWR_SUPPLY_VPP1 ) )
                    pState->uVpp1 = m_SocketState.uVpp1;
                m_SocketState.uVpp1 = pState->uVpp1;
                if( m_rgPowerEntries[m_SocketState.uVpp1 & SOCK_VCC_LEVEL_MASK].uPowerLevel !=
                    0 &&
                    m_rgPowerEntries[m_SocketState.uVpp1 & SOCK_VCC_LEVEL_MASK].uPowerLevel !=
                    120 )
                {
                    // VPP is same as VCC.
                    m_SocketState.uVpp1 = m_SocketState.fVcc;
                }
            }
            if( oldfVcc != m_SocketState.fVcc || olduVpp != m_SocketState.uVpp1 )
            {
                if( m_SocketState.fVcc == 0 )
                {
                    DEBUGMSG( ZONE_PDD,
                              ( TEXT( "PDCardSetSocket  CPCMSocket. Socket is Power Down! Function Interrupt Routing will force to Off\r\n" ) ) );
                    m_SocketState.dwInteruptStatus &= ~SOCK_INT_FUNC_IRQ_ROUTING;
                    PCICRegisterWrite( REG_POWER_CONTROL, 0 );
                }
                else
                {
                    PowerOnProcedure( m_SocketState.fVcc, m_SocketState.uVpp1 );
                    DEBUGMSG( ZONE_PDD,
                              ( TEXT( "PDCardSetSocket  CPCMSocket. Socket is Power Up with fVcc=%d\r\n" ),
                                m_SocketState.fVcc ) );
                }
            }
            else
            {
                DEBUGMSG( ZONE_PDD,
                          ( TEXT( "PDCardSetSocket  CPCMSocket. Same Power with fVcc=%d and uVpp1\r\n" ),
                            m_SocketState.fVcc,
                            m_SocketState.uVpp1 ) );
            }
        }

        DEBUGMSG( ZONE_PDD,
                  ( TEXT( "PDCardSetSocket Vcc = %d, Vpp = %d\r\n" ),
                    m_rgPowerEntries[m_SocketState.fVcc & SOCK_VCC_LEVEL_MASK].uPowerLevel,
                    m_rgPowerEntries[m_SocketState.uVpp1].uPowerLevel ) );

        DEBUGMSG( ZONE_PDD,
                  ( TEXT( "PDCardSetSocket REG_POWER_CONTROL = %x,Present Register=%x,Socket_Event=%x\r\n" ),
                    PCICRegisterRead( REG_POWER_CONTROL ), 0, 0 ) );
        PCICRegisterWrite( REG_INTERRUPT_AND_GENERAL_CONTROL,
                           PCICRegisterRead( REG_INTERRUPT_AND_GENERAL_CONTROL ) |
                           INT_CARD_NOT_RESET );
    }
    else
    {
        status = CERR_BAD_SOCKET;
    }
    Unlock();

#ifdef DEBUG
    bool fFlag = false;
    if( fFlag )
    {
        DumpAllRegisters();
    }
#endif
    return status;
}

void CPCMSocket::PowerCycleEvent()
{
    GetPCCardBusBridge()->PowerCycleEvent();
}

void CPCMSocket::CardInjectEvent()
{
    GetPCCardBusBridge()->CardInjectEvent();
}

void CPCMSocket::PowerOnProcedure( UINT8 fVcc, UINT8 fVpp )
{
    DEBUGMSG( ZONE_SOCKET,
              ( TEXT( "+SMDK2410!CPCMSocket:PowerOnProcedure fVcc=%d,fVpp=%d\r\n" ),
                fVcc,
                fVpp ) );

    if( !GetPCCardBusBridge()->PollingMode() )
    {
        // PD6710 specific code to enable management interrupt(routed to -INTR)
        UINT8 tmp = PCICRegisterRead( REG_INTERRUPT_AND_GENERAL_CONTROL );
        tmp |= INT_ENABLE_MANAGE_INT;
        PCICRegisterWrite( REG_INTERRUPT_AND_GENERAL_CONTROL, tmp );
    }

    //
    // Power off the socket
    //
    PCICRegisterWrite( REG_POWER_CONTROL, 0 );

    //
    // Tri-state outputs for 310ms
    //
    Sleep( 310 );

    //
    // Power the socket to default Vcc=50 and Vpp = Vcc;
    //
    DEBUGCHK( fVcc < NUM_POWER_ENTRIES );
    DEBUGCHK( fVpp < NUM_POWER_ENTRIES );
    BYTE bPowerReg = 0;
    if( m_rgPowerEntries[fVcc].uPowerLevel == 33 )
    {
        bPowerReg |= PWR_OUTPUT_ENABLE | PWR_VCC_POWER;
        DEBUGMSG( ZONE_PDD, (TEXT(" set to 3.3V\r\n")));
        PCICRegisterWrite( REG_GENERAL_CONTROL, PCICRegisterRead( REG_GENERAL_CONTROL ) | MISC1_VCC_33 );
    }
    else if( m_rgPowerEntries[fVcc].uPowerLevel == 50 )
    {
        bPowerReg |= PWR_OUTPUT_ENABLE | PWR_VCC_POWER;
        if( PCICRegisterRead( REG_GENERAL_CONTROL ) & MISC1_5V_DETECT ) //REG_MISC_CONTROL_1;
        { // i.e., if we detect a 5V card:
            DEBUGMSG( ZONE_PDD, (TEXT(" set to 5V\r\n")) );
            PCICRegisterWrite( REG_GENERAL_CONTROL, PCICRegisterRead( REG_GENERAL_CONTROL ) & ~MISC1_VCC_33 ); // disable 3.3V (i.e., use 5V)
        }
        else
        {
            // this is a 3.3V ONLY card and mustn't be powered at 5V
            // so ignore the command; we'd assert here save that card
            // services will attempt to apply 5V to read the CIS.

            DEBUGMSG( ZONE_PDD, (TEXT(" set to 3.3V[5V]\r\n")));
            PCICRegisterWrite( REG_GENERAL_CONTROL, PCICRegisterRead( REG_GENERAL_CONTROL ) | MISC1_VCC_33 );  // enable 3.3V
        }
    }
    if( fVcc == fVpp )
    { 
        bPowerReg |= PWR_VPP1_BIT0; // set Vpp = Vcc
    }
    else
    { 
        bPowerReg |= PWR_VPP1_BIT0 | PWR_VPP1_BIT1; // set Vpp = 0
    }
    PCICRegisterWrite( REG_POWER_CONTROL, bPowerReg );

    Sleep( 100 );

    //
    // Assert RESET for 20 MS
    //
    UINT8 intctl = PCICRegisterRead( REG_INTERRUPT_AND_GENERAL_CONTROL );
    PCICRegisterWrite( REG_INTERRUPT_AND_GENERAL_CONTROL, intctl & ~ INT_CARD_NOT_RESET );
    Sleep( 20 );

    Sleep(20);

    //
    // Deassert RESET
    //
    PCICRegisterWrite( REG_INTERRUPT_AND_GENERAL_CONTROL,
                       intctl | INT_CARD_NOT_RESET );

    //
    // Allow the card 2 seconds to assert RDY
    //
    DWORD dwCount;
    for( dwCount = 0; dwCount < 2000; dwCount += 20 )
    {
        // Check for one second.
        if( ( PCICRegisterRead( REG_INTERFACE_STATUS ) & STS_CARD_READY ) !=
            0 )
        {
            break;
        }
        PCICRegisterWrite( REG_POWER_CONTROL, bPowerReg );
        Sleep( 20 );
    };

    if( dwCount >= 2000 )
    {
        DEBUGMSG( ZONE_FUNCTION,
                  ( TEXT( "PDCardResetSocket: CARD IN SOCKET NOT RDY AFTER %dms\r\n" ),
                    dwCount ) );
    }

    PowerCycleEvent();
}

//
// ResetSocket - do a PCMCIA reset
// return CERR_SUCCESS on success, CERR_UNSUPPORTED_SERVICE if size is anomolous
//
STATUS CPCMSocket::CardResetSocket()
{
    DEBUGMSG( ZONE_SOCKET, ( TEXT( "+SMDK2410!ResetPCMCIASocket()\r\n" ) ) );

    // set INT_CARD_NOT_RESET in INTERRUPT_AND_GENERAL_CONTROL register as directed
    Lock();
    STATUS status = CERR_SUCCESS;
    if( m_pBridge )
    {
        //
        // Power the socket
        //
        UINT8 uTmp = PCICRegisterRead( REG_INTERFACE_STATUS );
        if( ( uTmp & ( STS_CD1 | STS_CD2 ) ) == 0 )
        {
            // No card Inserted
            status = CERR_NO_CARD;
        }
        else
        {
            BYTE uVcc = 1;
            if( PCICRegisterRead( REG_GENERAL_CONTROL ) & MISC1_5V_DETECT )
            {
                // 5 Volt Card.
                uVcc = 2;
                DEBUGMSG( ZONE_SOCKET,
                          ( TEXT( "+SMDK2410!ResetPCMCIASocket: Initial Power on is 5 Volt\r\n" ) ) );
            }
            else
            {
                DEBUGMSG( ZONE_SOCKET,
                          ( TEXT( "+SMDK2410!ResetPCMCIASocket: Initial Power on is 3 Volt\r\n" ) ) );
            }
            PowerOnProcedure( uVcc, 0 );

            //
            // Power the socket
            //
            m_SocketState.fVcc = uVcc;
            m_SocketState.uVpp1 = 0;
            m_SocketState.uVpp2 = 0;
        }
    }
    else
    {
        status = CERR_BAD_SOCKET;
    }
    DEBUGMSG( ZONE_SOCKET,
              ( TEXT( "-SMDK2410!ResetPCMCIASocket() return status =%x\r\n" ),
                status ) );
    Unlock();
    return status;
}

void CPCMSocket::SocketEventHandle( DWORD dwEvent, DWORD dwPresentStateReg ) // Event Handle from Interrupt.
{
    SS_SOCKET_STATE sSockState;
    Lock();
    CardGetSocket( &sSockState );
    // Update the state from interrupt. But not card detection. Card detection event
    // generate during object createion.
    sSockState.dwEventChanged &= ~SOCK_EVENT_CD;
    if( ( dwPresentStateReg & SPS_POWERCYCLE ) != 0 )
    {
        sSockState.dwEventStatus |= SOCK_EVENT_PWRCYCLE;
        sSockState.dwEventChanged |= SOCK_EVENT_PWRCYCLE;
    }
    Unlock();
    m_pBridge->CallBackToCardService( GetSocketHandle(),
                                      &sSockState );
}


CPcmciaCardSocket* CreatePCMCIASocket( CPcmciaBusBridge* pBridge )
{
    CPCMSocket* pNewSocket = new CPCMSocket( pBridge );
    if( pNewSocket != NULL )
    {
        if( pNewSocket->CardInitWindow( pNewSocket ) )
        {
            return pNewSocket;
        }
        else
        {
            delete pNewSocket;
        }
    }
    return NULL;
}

