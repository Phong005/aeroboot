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
#include "socksv2.h"
#include <ceddk.h>
#include <devload.h>
#include <pcibus.h>
#include "Resource.h"
#include "PDSocket.h"

#ifdef DEBUG
DBGPARAM dpCurSettings =
{
    TEXT( "SMDK2410" ),
    {
    TEXT( "Errors" ),
    TEXT( "Warnings" ),
    TEXT( "Functions" ),
    TEXT( "Callbacks" ),
    TEXT( "STSCHG Int" ),
    TEXT( "IREQ Int" ),
    TEXT( "MDD Init" ),
    TEXT( "PCI Enum" ),
    TEXT( "PDD" ),
    TEXT( "PDD Init" ),
    TEXT( "CIS Tuples" ),
    TEXT( "MDD" ),
    TEXT( "Undefined" ),
    TEXT( "Undefined" ),
    TEXT( "Undefined" ),
    TEXT( "Undefined" ) },
    0xff
};

#endif  // DEBUG

class CSocketContainer : public CStaticContainer <CPcmciaCardSocket, NUM_SLOTS>
{
public:
    CPcmciaCardSocket* ObjectBySocket( HANDLE hSocket )
    {
        CPcmciaCardSocket*  pObject = NULL;    
        Lock();
        for( DWORD dwIndex = 0; dwIndex < m_dwArraySize; dwIndex++ )
        {
            if( ( pObject = m_rgObjectArray[dwIndex] ) != NULL )
            {
                if( pObject->GetSocketHandle() == hSocket )
                {
                    pObject->AddRef();
                    break;
                }
            }
            pObject = NULL;
        }
        Unlock();
        return pObject;
    }
};
class CSocketContainer* pSocketContainer = NULL;

extern "C" BOOL WINAPI DllEntry( HANDLE  hInstDll,
                                 DWORD   dwReason,
                                 LPVOID  lpvReserved )
{
    switch( dwReason )
    {
      case DLL_PROCESS_ATTACH:
        DEBUGREGISTER( ( HINSTANCE ) hInstDll );
        DEBUGMSG( ZONE_INIT, ( TEXT( "SMDK2410 : DLL_PROCESS_ATTACH\r\n" ) ) );
        DisableThreadLibraryCalls( ( HMODULE ) hInstDll );
        pSocketContainer = new CSocketContainer();
        if( !pSocketContainer )
            return FALSE;
        break;

      case DLL_PROCESS_DETACH:
        DEBUGMSG( ZONE_INIT, ( TEXT( "SMDK2410 : DLL_PROCESS_DETACH\r\n" ) ) );
        if( pSocketContainer )
        {
            delete pSocketContainer;
            pSocketContainer = NULL;
        };
        break;
    }

    return ( TRUE );
}

extern "C" DWORD Init( DWORD dwInfo )
{
    CPcmciaBusBridge* pSocket = new  CPcmciaBusBridge( ( LPCTSTR ) dwInfo );
    if( pSocket )
    {
        if( pSocket->Init() )
        {
            return ( DWORD ) pSocket;
        }
        else
        {
            delete pSocket;
        }
    }
    return 0;
}

extern "C" void Deinit( DWORD dwSocketNum )  // This argument is return value from PDCardInitServices.
{
    CPcmciaBusBridge* pSocket = ( CPcmciaBusBridge* ) dwSocketNum;
    if( pSocket )
    {
        delete pSocket;
    }
}

CPcmciaCardSocket* GetSocket( HANDLE hSocket )
{
    if( pSocketContainer != 0 )
        return pSocketContainer->ObjectBySocket( hSocket );
    else
        return NULL;
}

void CPcmciaBusBridge::InsertPcmciaCardSocket( CPcmciaCardSocket* pSocket )
{
    if( pSocket )
    {
        Lock();
        if( m_pCardSocket != pSocket )
            RemovePcmciaCardSocket();
        if( m_pCardSocket == NULL &&
            pSocketContainer != NULL )
        {
            CPcmciaCardSocket* pReturn = pSocketContainer->InsertObjectAtEmpty( NULL,
                                                                                pSocket );
            if( pReturn )
            {
                SS_SOCKET_STATE sSockState;
                m_pCardSocket = pSocket;
                m_pCardSocket->CardGetSocket( &sSockState );
                sSockState.dwEventChanged |= SOCK_EVENT_CD;
                sSockState.dwEventStatus |= SOCK_EVENT_CD;
                CallBackToCardService( m_pCardSocket->GetSocketHandle(), &sSockState );
            }
            else
            {
                // This should never happens.But we do our best to recover.
                DEBUGCHK( FALSE );
                delete pSocket;
            }
        }
        else
            DEBUGCHK( FALSE );
        Unlock();
    }
    else
    {
        DEBUGCHK( FALSE );
    }
}
void CPcmciaBusBridge::RemovePcmciaCardSocket()
{
    Lock();
    if( m_pCardSocket )
    {
        SS_SOCKET_STATE sSockState;
        m_pCardSocket->CardGetSocket( &sSockState );
        sSockState.dwEventChanged |= SOCK_EVENT_CD;
        sSockState.dwEventStatus &= ~SOCK_EVENT_CD;
        CallBackToCardService( m_pCardSocket->GetSocketHandle(), &sSockState );
        if( pSocketContainer )
        {
            CPcmciaCardSocket* pSocket = pSocketContainer->RemoveObjectBy( m_pCardSocket );
            DEBUGCHK( pSocket != NULL );
        }
        else
        {
            // This should never happen, but we do our best to recover.
            DEBUGCHK( FALSE );
            delete m_pCardSocket;
        }
        m_pCardSocket = NULL;
    }
    Unlock();
}
extern "C" 
BOOL PowerUp( DWORD dwData )
{
    CPcmciaBusBridge* pSocket = ( CPcmciaBusBridge* ) dwData;
    if( pSocket )
    {
        pSocket->PowerMgr( FALSE );
    }
    return TRUE;
};
extern "C"
BOOL PowerDown( DWORD dwData )
{
    CPcmciaBusBridge* pSocket = ( CPcmciaBusBridge* ) dwData;
    if( pSocket )
    {
        pSocket->PowerMgr( TRUE );
    }
    return TRUE;
}

STATUS PDCardGetSocket( HANDLE hSocket, // @parm Socket number (first socket is 0)
                        PSS_SOCKET_STATE pState // @parm Pointer to PDCARD_SOCKET_STATE structure
)
{
    CPcmciaCardSocket* pSocket = GetSocket( hSocket );
    if( pSocket != NULL )
    {
        STATUS rStatus = pSocket->CardGetSocket( pState );
        pSocket->DeRef();
        return rStatus;
    }
    else
    {
        return CERR_BAD_SOCKET;
    }
}
STATUS PDCardSetSocket( HANDLE hSocket, // @parm Socket number (first socket is 0)
                        PSS_SOCKET_STATE pState // @parm Pointer to PDCARD_SOCKET_STATE structure
)
{
    CPcmciaCardSocket* pSocket = GetSocket( hSocket );
    if( pSocket != NULL )
    {
        STATUS rStatus = pSocket->CardSetSocket( pState );
        pSocket->DeRef();
        return rStatus;
    }
    else
    {
        return CERR_BAD_SOCKET;
    }
}
STATUS PDCardResetSocket( HANDLE hSocket  // @parm Socket number (first socket is 0)
)
{
    CPcmciaCardSocket* pSocket = GetSocket( hSocket );
    if( pSocket != NULL )
    {
        STATUS rStatus = pSocket->CardResetSocket();
        pSocket->DeRef();
        return rStatus;
    }
    else
    {
        return CERR_BAD_SOCKET;
    }
}
//
STATUS PDCardGetWindow( UINT32 uWindow, // @parm Window number (the first window is 0)
                        PSS_WINDOW_STATE pWindowState   // @parm Pointer to a PDCARD_WINDOW_STATE structure.
)
{
    if( pWindowState == NULL )
    {
        return CERR_BAD_WINDOW;
    }

    HANDLE hSocket = pWindowState->hSocket;
    CPcmciaCardSocket* pSocket = GetSocket( hSocket );
    if( pSocket != NULL )
    {
        STATUS rStatus = pSocket->CardGetWindow( uWindow, pWindowState );
        pSocket->DeRef();
        return rStatus;
    }
    else
    {
        return CERR_BAD_WINDOW;
    }
}
STATUS PDCardSetWindow( UINT32 uWindow, // @parm Window number (the first window is 0)
                        PSS_WINDOW_STATE pWindowState   // @parm Pointer to a PDCARD_WINDOW_STATE structure.
)
{
    if( pWindowState == NULL )
    {
        return CERR_BAD_WINDOW;
    }

    HANDLE hSocket = pWindowState->hSocket;
    CPcmciaCardSocket* pSocket = GetSocket( hSocket );
    if( pSocket != NULL )
    {
        STATUS rStatus = pSocket->CardSetWindow( uWindow, pWindowState );
        pSocket->DeRef();
        return rStatus;
    }
    else
    {
        return CERR_BAD_WINDOW;
    }
}
STATUS PDCardInquireWindow( UINT32 uWindow, // @parm Window number (the first window is 0)
                            PSS_WINDOW_INFO pWinInfo        // @parm Pointer to a PDCARD_WINDOW_INFO structure.
)
{
    if( pWinInfo == NULL )
    {
        return CERR_BAD_WINDOW;
    }
    HANDLE hSocket = pWinInfo->hSocket;
    CPcmciaCardSocket* pSocket = GetSocket( hSocket );
    if( pSocket != NULL )
    {
        STATUS rStatus = pSocket->CardInquireWindow( uWindow, pWinInfo );
        pSocket->DeRef();
        return rStatus;
    }
    else
    {
        return CERR_BAD_WINDOW;
    }
};
STATUS PDCardInquireSocket( HANDLE hSocket, PSS_SOCKET_INFO pSocketInfo )
{
    CPcmciaCardSocket* pSocket = GetSocket( hSocket );
    if( pSocket != NULL )
    {
        STATUS rStatus = pSocket->CardInquireSocket( pSocketInfo );
        pSocket->DeRef();
        return rStatus;
    }
    else
    {
        return CERR_BAD_SOCKET;
    }
}


STATUS PDCardAccessMemory( HANDLE hSocket, PSS_MEMORY_ACCESS pMemoryAccess )
{
    CPcmciaCardSocket* pSocket = GetSocket( hSocket );
    if( pSocket != NULL )
    {
        STATUS rStatus = pSocket->CardAccessMemory( pMemoryAccess );
        pSocket->DeRef();
        return rStatus;
    }
    else
    {
        return CERR_BAD_SOCKET;
    }
}
STATUS PDGetPowerEntry( HANDLE hSocket,
                        PDWORD pdwNumOfEnery,
                        PSS_POWER_ENTRY pPowerEntry )
{
    CPcmciaCardSocket* pSocket = GetSocket( hSocket );
    if( pSocket != NULL )
    {
        STATUS rStatus = pSocket->GetPowerEntry( pdwNumOfEnery, pPowerEntry );
        pSocket->DeRef();
        return rStatus;
    }
    else
    {
        return CERR_BAD_SOCKET;
    }
}
const SS_SOCKET_SERVICE CPcmciaBusBridge::SMDK2410SocketServiceStatic =
{
    0,// 
    PDCardInquireSocket,
    PDCardGetSocket,
    PDCardSetSocket,
    PDCardResetSocket,
    PDCardInquireWindow,
    PDCardGetWindow,
    PDCardSetWindow,
    PDCardAccessMemory,
    PDGetPowerEntry,
    NULL, // No need to support Translate Bus address.
    NULL  // No need to support Translate System Address.

};

