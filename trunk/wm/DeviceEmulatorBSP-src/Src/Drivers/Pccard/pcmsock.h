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

Abstract:

    Platform dependent PCMCIA definitions for Samsung SMDK2410 compatible socket
    controller.

Notes: 
--*/
#ifndef __PCMSOCK_H_
#define __PCMSOCK_H_
#include <pdsocket.h>

//
// Structure to track a physical socket
//
//

class CPCMSocket;

class CPcmciaMemWindows : public CPcmciaMemWindowImpl<CPCMSocket>
{
public:
                    CPcmciaMemWindows( CPCMSocket* pPcmSocket,
                                       DWORD dwWinIndex,
                                       const SS_WINDOW_STATE* pcWindowState,
                                       const SS_WINDOW_INFO* pcWindowInfo );
                    ~CPcmciaMemWindows();

protected:
    virtual void    FreeResources();
    virtual void    DisableWindow();
    virtual void    ProgramWindow();
    virtual DWORD   GetNewOffset( PSS_WINDOW_STATE pWindowState );
    virtual DWORD   GetNewLength( PSS_WINDOW_STATE pWindowState,
                                  DWORD dwNewOffset );
    virtual BOOL    GetBaseAddress( PSS_WINDOW_STATE pWindowState,
                                    DWORD dwNewOffset,
                                    DWORD dwNewLength,
                                    DWORD& dwNewBaseAddress );

    BYTE            m_uEnableBit;
    WORD            m_wWinBaseOffset;
    WORD            m_wWinPageOffset;
    DWORD           m_dwBaseAddress;
};


////////////////////////////////////////////////////////////////////////////////

class CPcmciaIoWindows : public CPcmciaIoWindowImpl<CPCMSocket>
{
public:
                    CPcmciaIoWindows( CPCMSocket* pPcmSocket,
                                      DWORD dwWinIndex,
                                      const SS_WINDOW_STATE* pcWindowState,
                                      const SS_WINDOW_INFO* pcWindowInfo );
                    ~CPcmciaIoWindows();

protected:
    virtual void    FreeResources();
    virtual void    DisableWindow();
    virtual void    ProgramWindow();
    virtual DWORD   GetNewOffset( PSS_WINDOW_STATE pWindowState );
    virtual DWORD   GetNewLength( PSS_WINDOW_STATE pWindowState,
                                  DWORD dwNewOffset );
    virtual BOOL    GetBaseAddress( PSS_WINDOW_STATE pWindowState,
                                    DWORD dwNewOffset,
                                    DWORD dwNewLength,
                                    DWORD& dwNewBaseAddress );

    BYTE            m_uEnableBit;
    BYTE            m_uIoCtrlBitOffset;
    WORD            m_wIoStatEndOffset;
    WORD            m_wIoOffsetOffset;
    DWORD           m_dwOffset;
};

class CPCMSocket : public CPCMCIASocketBase<CPcmciaMemWindows, CPcmciaIoWindows, CPcmciaCardSocket, CPcmciaBusBridge>
{
public:
                    CPCMSocket( CPcmciaBusBridge* pBriedge );
                    ~CPCMSocket();
    virtual void    SocketEventHandle( DWORD dwEvent, DWORD dwPresentStateReg ); // Event Handle from Interrupt.

    virtual STATUS  CardGetSocket( PSS_SOCKET_STATE pState );
    virtual STATUS  CardSetSocket( PSS_SOCKET_STATE pState );
    virtual STATUS  CardResetSocket();
    virtual STATUS CardInquireSocket( PSS_SOCKET_INFO pSocketInfo )
    {
        if( pSocketInfo )
        {
            *pSocketInfo = m_SocketInfo;
            return CERR_SUCCESS;
        }
        else
        {
            return CERR_BAD_ARGS;
        }
    }

    virtual void    PowerMgr( BOOL bPowerDown );
    virtual BOOL    Resuming();

    //virtual STATUS CardAccessMemory(PSS_MEMORY_ACCESS pMemoryAccess);
    UINT8 PCICRegisterRead( WORD register_num )
    {
        return m_pBridge->ReadPCICRegister( register_num );
    }
    void PCICRegisterWrite( WORD register_num, UINT8 value )
    {
        m_pBridge->WritePCICRegister( register_num, value );
    }
    CPcmciaBusBridge* GetPCCardBusBridge()
    {
        return m_pBridge;
    };

private:
    // For Power Manager.
#ifdef DEBUG
    VOID                            DumpAllRegisters();
#endif
    BYTE                            m_bBackupPCICPwrCtrlReg;
    BOOL                            m_bResuming;
    void                            PowerMgrCallback( BOOL bPowerOff );
    void                            PowerOnProcedure( UINT8 fVcc, UINT8 fVpp );
    void                            PowerCycleEvent();
    void                            CardInjectEvent();

    SS_SOCKET_STATE                 m_SocketState;
    SS_SOCKET_INFO                  m_SocketInfo;
    static const SS_SOCKET_STATE    ms_SocketInitState;
    static const SS_SOCKET_INFO     ms_SocketInitInfo;
};

CPcmciaCardSocket* CreatePCMCIASocket( CPcmciaBusBridge* pBridge,
                                       UINT8 nSocketNumber );

#endif
