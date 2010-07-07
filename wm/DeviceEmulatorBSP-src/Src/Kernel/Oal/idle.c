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
//  File: idle.c
//
//  This file contains OEMIdle function stub.
//
#include <windows.h>
#include <nkintr.h>
#include <oal.h>

//------------------------------------------------------------------------------
//
//  Function:     OEMIdle
//
//  This function is called by the kernel when there are no threads ready to 
//  run. The CPU should be put into a reduced power mode if possible and halted. 
//  It is important to be able to resume execution quickly upon receiving an 
//  interrupt.
//
//  It is assumed that interrupts are off when OEMIdle is called. Interrupts
//  are also turned off when OEMIdle returns.
//

// enable optimization for both debug and retail builds so that the Emulator
// will be able to recognize the OEMIdle opcode sequence
#pragma optimize("gsy", on)

VOID OEMIdle(DWORD idleParam)
{
    extern volatile BOOL fInterruptFlag;

    // Entered with interrupts disabled. Safely clear the flag.
    fInterruptFlag = 0;

    // Re-enable interrupts, and do nothing until one occurs.
    INTERRUPTS_ON();
    do {} while (!fInterruptFlag);

    // Turn interrupts back off, just like when entered.
    INTERRUPTS_OFF();
}

//------------------------------------------------------------------------------
