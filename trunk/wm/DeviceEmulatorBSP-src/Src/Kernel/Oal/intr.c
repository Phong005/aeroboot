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
//  File:  intr.h
//
//  This file contains SMDK2410X board specific interrupt code. The board uses
//  GPG1 pin as interrupt for CS8900A ethernet chip.
//
#include <bsp.h>

//------------------------------------------------------------------------------
//
//  Function:  BSPIntrInit
//
BOOL BSPIntrInit()
{
    S3C2410X_IOPORT_REG *pOalPortRegs;
    ULONG value;

    OALMSG(OAL_INTR&&OAL_FUNC, (L"+BSPIntrInit\r\n"));
    
    // Then get virtual address for IO port
    pOalPortRegs = OALPAtoVA(S3C2410X_BASE_REG_PA_IOPORT, FALSE);

    // Set GPG1 as EINT9 for CS8900A
    value = INREG32(&pOalPortRegs->GPGCON);
    OUTREG32(&pOalPortRegs->GPGCON, (value & ~(3 << 2))|(2 << 2));
    // Disable pullup
    value = INREG32(&pOalPortRegs->GPGUP);
    OUTREG32(&pOalPortRegs->GPGUP, value | (1 << 1));
    // High level interrupt
    value = INREG32(&pOalPortRegs->EXTINT1);
    OUTREG32(&pOalPortRegs->EXTINT1, (value & ~(0xf << 4))|(0x1 << 4));

    // Set GPG2 as EINT10 for DMA
    value = INREG32(&pOalPortRegs->GPGCON);
    OUTREG32(&pOalPortRegs->GPGCON, (value & ~(3 << 4))|(2 << 4));
    // Disable pullup
    value = INREG32(&pOalPortRegs->GPGUP);
    OUTREG32(&pOalPortRegs->GPGUP, value | (1 << 2));
    // Low level interrupt
    value = INREG32(&pOalPortRegs->EXTINT1);
    OUTREG32(&pOalPortRegs->EXTINT1, (value & ~(0xf << 8))|(0x1 << 8));


    // Add static mapping for Built-In OHCI 
    OALIntrStaticTranslate(SYSINTR_OHCI, IRQ_USBH );

    OALMSG(OAL_INTR&&OAL_FUNC, (L"-BSPIntrInit(rc = 1)\r\n"));
    return TRUE;
}

//------------------------------------------------------------------------------

BOOL BSPIntrRequestIrqs(DEVICE_LOCATION *pDevLoc, UINT32 *pCount, UINT32 *pIrqs)
{
    BOOL rc = FALSE;

    OALMSG(OAL_INTR&&OAL_FUNC, (
        L"+BSPIntrRequestIrq(0x%08x->%d/%d/0x%08x/%d, 0x%08x, 0x%08x)\r\n",
        pDevLoc, pDevLoc->IfcType, pDevLoc->BusNumber, pDevLoc->LogicalLoc,
        pDevLoc->Pin, pCount, pIrqs
    ));

    if (pIrqs == NULL || pCount == NULL || *pCount < 1) goto cleanUp;

    switch (pDevLoc->IfcType) {
    case Internal:
        switch ((ULONG)pDevLoc->LogicalLoc) {
        case BSP_BASE_REG_PA_CS8900A_IOBASE:
            pIrqs[0] = IRQ_EINT9;
            *pCount = 1;
            rc = TRUE;
            break;
        }
        break;
    }

cleanUp:
    OALMSG(OAL_INTR&&OAL_FUNC, (L"-BSPIntrRequestIrq(rc = %d)\r\n", rc));
    return rc;
}

//------------------------------------------------------------------------------
//
//  Function:  BSPIntrEnableIrq
//
//  This function is called from OALIntrEnableIrq to enable interrupt on
//  secondary interrupt controller.
//
UINT32 BSPIntrEnableIrq(UINT32 irq)
{
    OALMSG(OAL_INTR&&OAL_VERBOSE, (L"+BSPIntrEnableIrq(%d)\r\n", irq));
    OALMSG(OAL_INTR&&OAL_VERBOSE, (L"-BSPIntrEnableIrq(irq = %d)\r\n", irq));
    return irq;
}

//------------------------------------------------------------------------------
//
//  Function:  BSPIntrDisableIrq
//
//  This function is called from OALIntrDisableIrq to disable interrupt on
//  secondary interrupt controller.
//
UINT32 BSPIntrDisableIrq(UINT32 irq)
{
    OALMSG(OAL_INTR&&OAL_VERBOSE, (L"+BSPIntrDisableIrq(%d)\r\n", irq));
    OALMSG(OAL_INTR&&OAL_VERBOSE, (L"-BSPIntrDisableIrq(irq = %d\r\n", irq));
    return irq;
}


//------------------------------------------------------------------------------
//
//  Function:  BSPIntrDoneIrq
//
//  This function is called from OALIntrDoneIrq to finish interrupt on
//  secondary interrupt controller.
//
UINT32 BSPIntrDoneIrq(UINT32 irq)
{
    OALMSG(OAL_INTR&&OAL_VERBOSE, (L"+BSPIntrDoneIrq(%d)\r\n", irq));
    OALMSG(OAL_INTR&&OAL_VERBOSE, (L"-BSPIntrDoneIrq(irq = %d)\r\n", irq));
    return irq;
}


//------------------------------------------------------------------------------
//
//  Function:  BSPIntrActiveIrq
//
//  This function is called from interrupt handler to give BSP chance to 
//  translate IRQ in case of secondary interrupt controller.
//
UINT32 BSPIntrActiveIrq(UINT32 irq)
{
    OALMSG(OAL_INTR&&OAL_VERBOSE, (L"+BSPIntrActiveIrq(%d)\r\n", irq));
    OALMSG(OAL_INTR&&OAL_VERBOSE, (L"-BSPIntrActiveIrq(%d)\r\n", irq));
    return irq;
}

//------------------------------------------------------------------------------

