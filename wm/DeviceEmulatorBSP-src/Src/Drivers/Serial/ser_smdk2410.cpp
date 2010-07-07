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

    Serial PDD for SamSang 2410 Development Board.

Notes: 
--*/
#include <windows.h>
#include <types.h>
#include <ceddk.h>

#include <ddkreg.h>
#include <serhw.h>
#include <Serdbg.h>
#include <pdds3c2410_ser.h>
#include <s3c2410x_base_regs.h>
#include <s3c2410x_ioport.h>

// CPdd2410Serial1 is only use for UART 0 which 
// RTS CTS is use GPH0 GPH1
// DTR & DSR is USE GPH6 GPH7 which is UART2's Rx & Tx. You can not enable UART2 when this object is running.
class CPdd2410Serial1 : public CPdd2410Uart {
public:
    CPdd2410Serial1(LPTSTR lpActivePath, PVOID pMdd, PHWOBJ pHwObj)
        : CPdd2410Uart(lpActivePath, pMdd, pHwObj)
        {
        m_pIOPregs = NULL;
        m_fIsDSRSet = FALSE;
    }
    ~CPdd2410Serial1() {
        if (m_pIOPregs!=NULL)
            MmUnmapIoSpace((PVOID)m_pIOPregs,0);
    }
    virtual BOOL Init() {
        PHYSICAL_ADDRESS    ioPhysicalBase = { S3C2410X_BASE_REG_PA_IOPORT, 0};
        ULONG               inIoSpace = 0;
        if (TranslateBusAddr(m_hParent,Internal,0, ioPhysicalBase,&inIoSpace,&ioPhysicalBase)) {
            // Map it if it is Memeory Mapped IO.
            m_pIOPregs = (S3C2410X_IOPORT_REG *)MmMapIoSpace(ioPhysicalBase, sizeof(S3C2410X_IOPORT_REG),FALSE);
        }
        if (m_pIOPregs) {
            DDKISRINFO ddi;
            if (GetIsrInfo(&ddi)== ERROR_SUCCESS && 
                    KernelIoControl(IOCTL_HAL_REQUEST_SYSINTR, &ddi.dwIrq, sizeof(UINT32), &ddi.dwSysintr, sizeof(UINT32), NULL))
            {   
                RegSetValueEx(DEVLOAD_SYSINTR_VALNAME,REG_DWORD,(PBYTE)&ddi.dwSysintr, sizeof(UINT32));
            }
            else
                return FALSE;
            m_pDTRPort = (volatile ULONG *)&(m_pIOPregs->GPHDAT);
            m_pDSRPort = (volatile ULONG *)&(m_pIOPregs->GPHDAT);
            m_dwDTRPortNum = 6;
            m_dwDSRPortNum = 7;
            return CPdd2410Uart::Init();
        }
        return FALSE;
    };
    virtual void    SetDefaultConfiguration() {
        m_pIOPregs->GPHCON &= ~(0x3<<0 | 0x3<<2 | 0x3<<4 | 0x3<<6 | 0x3<<12 | 0x3<<14); // clear uart 0 - rx, tx
        m_pIOPregs->GPHCON |= (0x2<<4 | 0x2<<6 | 0x1<<12 | 0x0<<14); 
        m_pIOPregs->GPHCON |= (0x2<<0 | 0x2<<2 );
        m_pIOPregs->GPHUP  |= 0xc3;
        CPdd2410Uart::SetDefaultConfiguration();
    }
    virtual BOOL    InitModem(BOOL bInit) {
        SetDTR(bInit);
        return CPdd2410Uart::InitModem(bInit);
    }
    virtual ULONG   GetModemStatus() {
        ULONG ulReturn = CPdd2410Uart::GetModemStatus();
        ULONG ulEvent = 0;
        m_HardwareLock.Lock();
        BOOL fIsDSRSet = (((*m_pDSRPort) & (1<<m_dwDSRPortNum))==0);
        if (fIsDSRSet != m_fIsDSRSet) {
            ulEvent |= EV_DSR | EV_RLSD;
        }
        ulReturn |= (fIsDSRSet?(MS_DSR_ON|MS_RLSD_ON):0);
        m_fIsDSRSet = fIsDSRSet;
        m_HardwareLock.Unlock();
        if (ulEvent!=0)
            EventCallback(ulEvent,ulReturn);
        return ulReturn;
    }
    virtual void    SetDTR(BOOL bSet) {
        if (bSet)
            *m_pDTRPort &= ~(1<<m_dwDTRPortNum);
        else
            *m_pDTRPort |= (1<<m_dwDTRPortNum);
    };
private:
    volatile S3C2410X_IOPORT_REG * m_pIOPregs; 
    volatile ULONG *    m_pDTRPort;
    DWORD               m_dwDTRPortNum;
    volatile ULONG *    m_pDSRPort;
    DWORD               m_dwDSRPortNum;
    BOOL                m_fIsDSRSet;
};


// CPdd2410Serial2 is only use for UART 2 which 
// RTS CTS is use GPH0 GPH1
// DTR & DSR is USE GPH6 GPH7 which is UART2's Rx & Tx. You can not enable UART2 when this object is running.
class CPdd2410Serial2 : public CPdd2410Uart {
public:
    CPdd2410Serial2(LPTSTR lpActivePath, PVOID pMdd, PHWOBJ pHwObj)
        : CPdd2410Uart(lpActivePath, pMdd, pHwObj)
        {
        m_pIOPregs = NULL;
    }
    ~CPdd2410Serial2() {
        if (m_pIOPregs!=NULL)
            MmUnmapIoSpace((PVOID)m_pIOPregs,0);
    }
    virtual BOOL Init() {
        PHYSICAL_ADDRESS    ioPhysicalBase = { S3C2410X_BASE_REG_PA_IOPORT, 0};
        ULONG               inIoSpace = 0;
        if (TranslateBusAddr(m_hParent,Internal,0, ioPhysicalBase,&inIoSpace,&ioPhysicalBase)) {
            // Map it if it is Memeory Mapped IO.
            m_pIOPregs =(S3C2410X_IOPORT_REG *) MmMapIoSpace(ioPhysicalBase, sizeof(S3C2410X_IOPORT_REG),FALSE);
        }
        if (m_pIOPregs) {
            DDKISRINFO ddi;
            if (GetIsrInfo(&ddi)== ERROR_SUCCESS && 
                    KernelIoControl(IOCTL_HAL_REQUEST_SYSINTR, &ddi.dwIrq, sizeof(UINT32), &ddi.dwSysintr, sizeof(UINT32), NULL))
            {   
                RegSetValueEx(DEVLOAD_SYSINTR_VALNAME,REG_DWORD,(PBYTE)&ddi.dwSysintr, sizeof(UINT32));
            }
            else
                return FALSE;
            return CPdd2410Uart::Init();
        }
        return FALSE;
    };
    virtual void    SetDefaultConfiguration() {
        m_pIOPregs->GPHCON &= ~(0x3<<12 | 0x3<<14); // clear uart 2 - rx, tx
        m_pIOPregs->GPHCON |= (0x2<<12 | 0x2<<14); 
        m_pIOPregs->GPHUP |= 0xc0;
        CPdd2410Uart::SetDefaultConfiguration();
    }
    virtual ULONG   GetModemStatus() {
        return (CPdd2410Uart::GetModemStatus() | MS_CTS_ON);
    }
    volatile S3C2410X_IOPORT_REG * m_pIOPregs; 
};
CSerialPDD * CreateSerialObject(LPTSTR lpActivePath, PVOID pMdd,PHWOBJ pHwObj, DWORD DeviceArrayIndex)
{
    CSerialPDD * pSerialPDD = NULL;
    switch (DeviceArrayIndex) {
      case 0:
        pSerialPDD = new CPdd2410Serial1(lpActivePath,pMdd, pHwObj);
        break;
      case 1:
        pSerialPDD = new CPdd2410Serial2(lpActivePath,pMdd, pHwObj);
        break;
    }
    if (pSerialPDD && !pSerialPDD->Init()) {
        delete pSerialPDD;
        pSerialPDD = NULL;
    }    
    return pSerialPDD;
}
void DeleteSerialObject(CSerialPDD * pSerialPDD)
{
    if (pSerialPDD)
        delete pSerialPDD;
}

