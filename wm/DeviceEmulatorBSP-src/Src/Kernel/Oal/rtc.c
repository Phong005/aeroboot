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

#define FROM_BCD(n)     ((((n) >> 4) * 10) + ((n) & 0xf))
#define TO_BCD(n)       ((((n) / 10) << 4) | ((n) % 10))
#define RTC_YEAR_DATUM  2000

//------------------------------------------------------------------------------
//
//  Function:  OEMGetRealTime
//
//  Reads the current RTC value and returns a system time.
//
BOOL OEMGetRealTime(SYSTEMTIME *pTime)
{
    BOOL rc = FALSE;
    S3C2410X_RTC_REG *pRTCReg;
    UINT32 data;
    UINT16 seconds;


    OALMSG(OAL_RTC&&OAL_FUNC, (L"+OEMGetRealTime(pTime = 0x%x)\r\n", pTime));

    if (pTime == NULL) goto cleanUp;
    
    // Get uncached virtual address
    pRTCReg = OALPAtoVA(S3C2410X_BASE_REG_PA_RTC, FALSE);
    do {
        data = INPORT32(&pRTCReg->BCDSEC) & 0x7f;
        seconds = FROM_BCD(data);
        data = INPORT32(&pRTCReg->BCDYEAR);
        pTime->wYear = FROM_BCD(data) + RTC_YEAR_DATUM;
        data = INPORT32(&pRTCReg->BCDMON) & 0x1f;
        pTime->wMonth = FROM_BCD(data);
        data = INPORT32(&pRTCReg->BCDDAY) & 0x3f;
        pTime->wDay = FROM_BCD(data);
        pTime->wDayOfWeek = (WORD)INPORT32(&pRTCReg->BCDDATE) - 1;
        data = INPORT32(&pRTCReg->BCDHOUR) & 0x3f;
        pTime->wHour = FROM_BCD(data);
        data = INPORT32(&pRTCReg->BCDMIN) & 0x7f;
        pTime->wMinute = FROM_BCD(data);
        data = INPORT32(&pRTCReg->BCDSEC) & 0x7f;
        pTime->wSecond = FROM_BCD(data);
        pTime->wMilliseconds = 0;
    } while (pTime->wSecond != seconds);

    // Done
    rc = TRUE;

cleanUp:
    OALMSG(OAL_FUNC, (L"-OEMGetRealTime(rc = %d)\r\n", rc));
    return rc;
}

//------------------------------------------------------------------------------
//
//  Function:  OEMSetRealTime
//
//  Updates the RTC with the specified system time.
//
BOOL OEMSetRealTime(LPSYSTEMTIME pTime) 
{
    BOOL rc = FALSE;
    S3C2410X_RTC_REG *pRTCReg;

    OALMSG(OAL_RTC&&OAL_FUNC, (
        L"+OEMSetRealTime(%d/%d/%d %d:%d:%d.%03d)\r\n", 
        pTime->wYear, pTime->wMonth, pTime->wDay, pTime->wHour, pTime->wMinute,
        pTime->wSecond, pTime->wMilliseconds
    ));

    if (pTime == NULL) goto cleanUp;
    
    // The RTC will only support a BCD year value of 0 - 99.  The year datum is
    // 1980, so any dates greater than 2079 will fail unless the datum is
    // adjusted.
    if (pTime->wYear < RTC_YEAR_DATUM || (pTime->wYear - RTC_YEAR_DATUM) > 99) {
        OALMSG(OAL_ERROR, (L"ERROR: OEMSetRealTime: "
            L"RTC cannot support a year greater than %d or less than %d "
            L"(value %d)\r\n", (RTC_YEAR_DATUM + 99), RTC_YEAR_DATUM, 
            pTime->wYear
        ));
        goto cleanUp;
    }

    // Get uncached virtual address
    pRTCReg = OALPAtoVA(S3C2410X_BASE_REG_PA_RTC, FALSE);

    // Enable RTC control.
    SETREG32(&pRTCReg->RTCCON, 1);

    OUTPORT32(&pRTCReg->BCDSEC,  TO_BCD(pTime->wSecond));
    OUTPORT32(&pRTCReg->BCDMIN,  TO_BCD(pTime->wMinute));
    OUTPORT32(&pRTCReg->BCDHOUR, TO_BCD(pTime->wHour));
    OUTPORT32(&pRTCReg->BCDDATE, pTime->wDayOfWeek + 1);
    OUTPORT32(&pRTCReg->BCDDAY,  TO_BCD(pTime->wDay));
    OUTPORT32(&pRTCReg->BCDMON,  TO_BCD(pTime->wMonth));
    OUTPORT32(&pRTCReg->BCDYEAR, TO_BCD(pTime->wYear - RTC_YEAR_DATUM));

    // Disable RTC control.
    CLRREG32(&pRTCReg->RTCCON, 1);

    // Done
    rc = TRUE;

cleanUp:
    OALMSG(OAL_RTC&&OAL_FUNC, (L"-OEMSetRealTime(rc = %d)\r\n", rc));
    return rc;
}

//------------------------------------------------------------------------------
