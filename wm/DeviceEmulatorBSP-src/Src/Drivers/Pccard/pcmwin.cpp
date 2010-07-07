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

    PCMCIA Memory and IO Window functions

Notes: 
--*/
// @doc DRIVERS
#include <windows.h>
#include <types.h>
#include <socksv2.h>
#include <memory.h>
#include <ceddk.h>
#include <resmgr.h>

#include "PCMSock.h"

#define PCMCIA_MEM_WINDIOWS_BLOCK 0x1000 // PCMCIA require 4 k Alignment for this controller.
#define PCMCIA_MEM_BLOCK_ADDR_MASK (~(PCMCIA_MEM_WINDIOWS_BLOCK-1))

CPcmciaMemWindows::CPcmciaMemWindows( CPCMSocket* pPcmSocket,
                                      DWORD dwWinIndex,
                                      const SS_WINDOW_STATE* pcWindowState,
                                      const SS_WINDOW_INFO* pcWindowInfo ) : CPcmciaMemWindowImpl<CPCMSocket>( pPcmSocket,
                                                                                                               dwWinIndex,
                                                                                                               pcWindowState,
                                                                                                               pcWindowInfo )
{
    m_dwBaseAddress = pcWindowInfo->uMemBase; // save the 1st memory window base address
    m_uEnableBit = ( 1 << dwWinIndex );
    m_wWinBaseOffset = ( WORD )
                       ( REG_MEM_MAP0_START_ADDR_LO +
                         ( dwWinIndex * REG_MEM_MAP_STRIDE ) );
    m_wWinPageOffset = ( WORD ) ( REG_MEM_MAP0_WINDOW_PAGE + dwWinIndex );
    Initialize();
}

CPcmciaMemWindows::~CPcmciaMemWindows()
{
    Deinitialize();
}

void CPcmciaMemWindows::FreeResources()
{
}

void CPcmciaMemWindows::DisableWindow()
{
    m_pPcmSocket->PCICRegisterWrite( REG_WINDOW_ENABLE,
                                     ( ( m_pPcmSocket->PCICRegisterRead( REG_WINDOW_ENABLE ) |
                                         WIN_MEMCS16_DECODE ) & ~m_uEnableBit ) );
}

void CPcmciaMemWindows::ProgramWindow()
{
    DEBUGCHK( m_WinStatus.uSize != NULL );
    m_pPcmSocket->PCICRegisterWrite( REG_WINDOW_ENABLE,
                                     ( ( m_pPcmSocket->PCICRegisterRead( REG_WINDOW_ENABLE ) |
                                         WIN_MEMCS16_DECODE ) & ~m_uEnableBit ) );        
    BYTE uData;
    // Base
    m_pPcmSocket->PCICRegisterWrite( m_wWinBaseOffset +
                                     REG_MEM_MAP_START_ADDR_LO,
                                     ( BYTE ) ( m_WinStatus.uBase >> 12 ) );
    uData = ( BYTE ) ( ( m_WinStatus.uBase >> ( 12 + 8 ) ) & 0xf ) ;
    uData |= ( ( m_WinStatus.fState & WIN_STATE_16BIT ) !=
               0 ?
               MSH_MEM_16BIT :
               0 ); // Set data width.
    m_pPcmSocket->PCICRegisterWrite( m_wWinBaseOffset +
                                     REG_MEM_MAP_START_ADDR_HI,
                                     uData );
    // End
    DWORD dwEndAddr = m_WinStatus.uBase + m_WinStatus.uSize - 1;
    m_pPcmSocket->PCICRegisterWrite( m_wWinBaseOffset +
                                     REG_MEM_MAP_END_ADDR_LO,
                                     ( BYTE ) ( dwEndAddr >> 12 ) );
    uData = ( BYTE ) ( ( dwEndAddr >> ( 12 + 8 ) ) & 0xf );
    uData |= MTH_WAIT_STATE_BIT0 | MTH_WAIT_STATE_BIT1; // This is hardcoded value.
    m_pPcmSocket->PCICRegisterWrite( m_wWinBaseOffset +
                                     REG_MEM_MAP_END_ADDR_HI,
                                     uData );
    // Offset.
    DWORD dwOffset = m_WinStatus.uOffset - m_WinStatus.uBase;
    m_pPcmSocket->PCICRegisterWrite( m_wWinBaseOffset +
                                     REG_MEM_MAP_ADDR_OFFSET_LO,
                                     ( BYTE ) ( dwOffset >> 12 ) );
    uData = ( BYTE ) ( ( dwOffset >> ( 12 + 8 ) ) & 0x3f );
    uData |= ( ( m_WinStatus.fState & WIN_STATE_ATTRIBUTE ) !=
               0 ?
               MOH_REG_ACTIVE :
               0 );
    uData |= ( ( m_WinStatus.fState & WIN_STATE_READONLY ) !=
               0 ?
               MOH_WRITE_PROTECT :
               0 );
    m_pPcmSocket->PCICRegisterWrite( m_wWinBaseOffset +
                                     REG_MEM_MAP_ADDR_OFFSET_HI,
                                     uData );
    // Enable it
    m_pPcmSocket->PCICRegisterWrite( REG_WINDOW_ENABLE,
                                     ( ( m_pPcmSocket->PCICRegisterRead( REG_WINDOW_ENABLE ) |
                                         WIN_MEMCS16_DECODE ) |
                                       m_uEnableBit ) );

#ifdef DEBUG
    m_pPcmSocket->GetPCCardBusBridge()->DumpAllRegisters();
#endif
}

DWORD CPcmciaMemWindows::GetNewOffset( PSS_WINDOW_STATE pWindowState )
{
    DWORD dwNewOffset = pWindowState->uOffset& PCMCIA_MEM_BLOCK_ADDR_MASK;
    return dwNewOffset;
}

DWORD CPcmciaMemWindows::GetNewLength( PSS_WINDOW_STATE pWindowState,
                                       DWORD dwNewOffset )
{
    DWORD dwNewLength = pWindowState->uOffset +
                        pWindowState->uSize -
                        dwNewOffset ;
    dwNewLength = ( dwNewLength + PCMCIA_MEM_WINDIOWS_BLOCK - 1 ) & PCMCIA_MEM_BLOCK_ADDR_MASK;
    return dwNewLength;
}

BOOL CPcmciaMemWindows::GetBaseAddress( PSS_WINDOW_STATE pWindowState,
                                        DWORD dwNewOffset,
                                        DWORD dwNewLength,
                                        DWORD& dwNewBaseAddress )
{
    dwNewBaseAddress = m_dwBaseAddress + m_dwWinIndex * m_WinInfo.uMemMaxSize;
    return TRUE;
}

CPcmciaIoWindows::CPcmciaIoWindows( CPCMSocket* pPcmSocket,
                                    DWORD dwWinIndex,
                                    const SS_WINDOW_STATE* pcWindowState,
                                    const SS_WINDOW_INFO* pcWindowInfo ) : CPcmciaIoWindowImpl<CPCMSocket>( pPcmSocket,
                                                                                                            dwWinIndex,
                                                                                                            pcWindowState,
                                                                                                            pcWindowInfo )
{
    m_dwOffset = 0;
    m_uEnableBit = ( 1 <<
                     ( dwWinIndex + 6 ) ); // for Window Enable Register.
    m_uIoCtrlBitOffset = ( BYTE ) ( dwWinIndex * 4 ); // for IO Window Confrol Register.
    m_wIoStatEndOffset = ( WORD )
                         ( REG_IO_MAP0_START_ADDR_LO + ( dwWinIndex * 4 ) );// offset for Start End IO window register 
    m_wIoOffsetOffset = ( WORD )
                        ( REG_IO_MAP0_OFFSET_ADDR_LO + ( dwWinIndex * 2 ) );// offset for offset register.
    //m_IoWinInfo.uIOMaxSize = pPcmSocket->GetPCCardBusBridge()->GetIOWindowMaximumSize();
    Initialize();
}

CPcmciaIoWindows::~CPcmciaIoWindows()
{
    Deinitialize();
}

void CPcmciaIoWindows::FreeResources()
{
    ResourceRelease( RESMGR_IOSPACE, m_dwOffset, m_WinStatus.uSize );
}

void CPcmciaIoWindows::DisableWindow()
{
    m_pPcmSocket->PCICRegisterWrite( REG_WINDOW_ENABLE,
                                     ( ( m_pPcmSocket->PCICRegisterRead( REG_WINDOW_ENABLE ) |
                                         WIN_MEMCS16_DECODE ) & ~m_uEnableBit ) );
}

void CPcmciaIoWindows::ProgramWindow()
{
    DEBUGCHK( m_WinStatus.uSize != NULL );
    m_pPcmSocket->PCICRegisterWrite( REG_WINDOW_ENABLE,
                                     ( ( m_pPcmSocket->PCICRegisterRead( REG_WINDOW_ENABLE ) |
                                         WIN_MEMCS16_DECODE ) & ~m_uEnableBit ) );        

    // Program the window.
    // Base
    DWORD dwStartAddr = m_WinStatus.uOffset;
    m_pPcmSocket->PCICRegisterWrite( m_wIoStatEndOffset + 0,
                                     ( BYTE ) ( dwStartAddr ) );
    m_pPcmSocket->PCICRegisterWrite( m_wIoStatEndOffset + 1,
                                     ( BYTE ) ( dwStartAddr >> 8 ) );
    // End
    DWORD dwEndAddr = m_WinStatus.uOffset + m_WinStatus.uSize - 1;
    m_WinStatus.uOffset = 0;
    m_pPcmSocket->PCICRegisterWrite( m_wIoStatEndOffset + 2,
                                     ( BYTE ) ( dwEndAddr ) );
    m_pPcmSocket->PCICRegisterWrite( m_wIoStatEndOffset + 3,
                                     ( BYTE ) ( dwEndAddr >> 8 ) );
    // Attribute.
    BYTE uData = m_pPcmSocket->PCICRegisterRead( REG_IO_WINDOW_CONTROL ) & ~( 0xf <<
                                                                              m_uIoCtrlBitOffset ); // Clean All related Bit.
    // For testing.
    uData |= ( ( m_WinStatus.fState & WIN_STATE_16BIT ) !=
               0 ?
               ( ( ICR_0_IOCS16 | ICR_0_IO_16BIT ) << m_uIoCtrlBitOffset ) :
               0 );
//    uData |= ( ( m_WinStatus.fSpeed & WIN_SPEED_USE_WAIT ) !=
//               0 ?
//               ( ICR_0_WAIT_STATE << m_uIoCtrlBitOffset ) :
//               0 );
    m_pPcmSocket->PCICRegisterWrite( REG_IO_WINDOW_CONTROL, uData );
    // Enable it
    m_pPcmSocket->PCICRegisterWrite( REG_WINDOW_ENABLE,
                                     ( ( m_pPcmSocket->PCICRegisterRead( REG_WINDOW_ENABLE ) |
                                         WIN_MEMCS16_DECODE ) |
                                       m_uEnableBit ) );

#ifdef DEBUG
    m_pPcmSocket->GetPCCardBusBridge()->DumpAllRegisters();
#endif
}

DWORD CPcmciaIoWindows::GetNewOffset( PSS_WINDOW_STATE pWindowState )
{
        m_dwOffset = pWindowState->uOffset;
        return pWindowState->uOffset;
}

DWORD CPcmciaIoWindows::GetNewLength( PSS_WINDOW_STATE pWindowState,
                                      DWORD dwNewOffset )
{
    return pWindowState->uSize;
}

BOOL CPcmciaIoWindows::GetBaseAddress( PSS_WINDOW_STATE pWindowState,
                                       DWORD dwNewOffset,
                                       DWORD dwNewLength,
                                       DWORD& dwNewBaseAddress )
{
    if( !ResourceRequest( RESMGR_IOSPACE,
                          m_dwOffset,
                          m_WinStatus.uSize ) ) // We failed to allocate physical address for this window.
    {
        return FALSE;
    }
    dwNewBaseAddress = 0x11000000;
    return TRUE;
}
