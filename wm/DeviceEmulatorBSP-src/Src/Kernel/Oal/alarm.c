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
//------------------------------------------------------------------------------
//
//  Module: rtc.c
//
//  Real-time clock (RTC) routines for the Samsung S3C2410x processor
//
#include <windows.h>
#include <ceddk.h>
#include <nkintr.h>
#include <oal.h>
#include <s3c2410x.h>

//------------------------------------------------------------------------------
// Defines 
#define TO_BCD(n)       ((((n) / 10) << 4) | ((n) % 10))
#define RTC_YEAR_DATUM  2000

//------------------------------------------------------------------------------
//
//  Function:  OALIoCtlHalInitRTC
//
//  This function is called by WinCE OS to initialize the time after boot.
//  Input buffer contains SYSTEMTIME structure with default time value.
//  If hardware has persistent real time clock it will ignore this value
//  (or all call).
//
BOOL OALIoCtlHalInitRTC(
    UINT32 code, VOID *pInpBuffer, UINT32 inpSize, VOID *pOutBuffer,
    UINT32 outSize, UINT32 *pOutSize
) {
    BOOL rc = FALSE;
    SYSTEMTIME *pTime = (SYSTEMTIME*)pInpBuffer;

    OALMSG(OAL_IOCTL&&OAL_FUNC, (L"+OALIoCtlHalInitRTC(...)\r\n"));

    // Validate inputs
    if (pInpBuffer == NULL || inpSize < sizeof(SYSTEMTIME)) {
        NKSetLastError(ERROR_INVALID_PARAMETER);
        OALMSG(OAL_ERROR, (
            L"ERROR: OALIoCtlHalInitRTC: Invalid parameter\r\n"
        ));
        goto cleanUp;
    }

    // Add static mapping for RTC alarm
    OALIntrStaticTranslate(SYSINTR_RTC_ALARM, IRQ_RTC);

    // Set time
    rc = OEMSetRealTime(pTime);
    
cleanUp:
    OALMSG(OAL_IOCTL&&OAL_FUNC, (L"-OALIoCtlHalInitRTC(rc = %d)\r\n", rc));
    return rc;
}

//------------------------------------------------------------------------------
//
//  Function:  OEMSetAlarmTime
//
//  Set the RTC alarm time.
//
BOOL OEMSetAlarmTime(SYSTEMTIME *pTime)
{
    BOOL rc = FALSE;
    S3C2410X_RTC_REG *pRTCReg;
    UINT32 irq;

    OALMSG(OAL_RTC&&OAL_FUNC, (
        L"+OEMSetAlarmTime(%d/%d/%d %d:%d:%d.%03d)\r\n", 
        pTime->wMonth, pTime->wDay, pTime->wYear, pTime->wHour, pTime->wMinute,
        pTime->wSecond, pTime->wMilliseconds
    ));

    if (pTime == NULL) goto cleanUp;
    
    // Get uncached virtual address
    pRTCReg = OALPAtoVA(S3C2410X_BASE_REG_PA_RTC, FALSE);
    
    // Enable RTC control
    SETREG32(&pRTCReg->RTCCON, 1);

    OUTPORT32(&pRTCReg->ALMSEC,  TO_BCD(pTime->wSecond));
    OUTPORT32(&pRTCReg->ALMMIN,  TO_BCD(pTime->wMinute));
    OUTPORT32(&pRTCReg->ALMHOUR, TO_BCD(pTime->wHour));
    OUTPORT32(&pRTCReg->ALMDATE, TO_BCD(pTime->wDay));
    OUTPORT32(&pRTCReg->ALMMON,  TO_BCD(pTime->wMonth));
    OUTPORT32(&pRTCReg->ALMYEAR, TO_BCD(pTime->wYear - RTC_YEAR_DATUM));
   
    // Enable the RTC alarm interrupt
    OUTPORT32(&pRTCReg->RTCALM, 0x7F);
 
    // Disable RTC control.
    CLRREG32(&pRTCReg->RTCCON, 1);

    // Enable/clear RTC interrupt
    irq = IRQ_RTC;
    OALIntrDoneIrqs(1, &irq);

    // Done
    rc = TRUE;
    
cleanUp:
    OALMSG(OAL_RTC&&OAL_FUNC, (L"-OEMSetAlarmTime(rc = %d)\r\n", rc));
    return rc;
}

//------------------------------------------------------------------------------
