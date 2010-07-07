;
; Copyright (c) Microsoft Corporation.  All rights reserved.
;
;
; Use of this source code is subject to the terms of the Microsoft end-user
; license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
; If you did not accept the terms of the EULA, you are not authorized to use
; this source code. For a copy of the EULA, please see the LICENSE.RTF on your
; install media.
;
    INCLUDE kxarm.h
    INCLUDE s3c2410x.inc

;from fw.s, duplicated in startup.s
SleepState_Data_Start              EQU     (0)
SleepState_WakeAddr                EQU     (SleepState_Data_Start                  )
SleepState_MMUCTL                  EQU     (SleepState_WakeAddr        + WORD_SIZE )
SleepState_MMUTTB                  EQU     (SleepState_MMUCTL          + WORD_SIZE )
SleepState_MMUDOMAIN               EQU     (SleepState_MMUTTB          + WORD_SIZE )
SleepState_SVC_SP                  EQU     (SleepState_MMUDOMAIN       + WORD_SIZE )
SleepState_SVC_SPSR                EQU     (SleepState_SVC_SP          + WORD_SIZE )
SleepState_FIQ_SPSR                EQU     (SleepState_SVC_SPSR        + WORD_SIZE )
SleepState_FIQ_R8                  EQU     (SleepState_FIQ_SPSR        + WORD_SIZE )
SleepState_FIQ_R9                  EQU     (SleepState_FIQ_R8          + WORD_SIZE )
SleepState_FIQ_R10                 EQU     (SleepState_FIQ_R9          + WORD_SIZE )
SleepState_FIQ_R11                 EQU     (SleepState_FIQ_R10         + WORD_SIZE )
SleepState_FIQ_R12                 EQU     (SleepState_FIQ_R11         + WORD_SIZE )
SleepState_FIQ_SP                  EQU     (SleepState_FIQ_R12         + WORD_SIZE )
SleepState_FIQ_LR                  EQU     (SleepState_FIQ_SP          + WORD_SIZE )
SleepState_ABT_SPSR                EQU     (SleepState_FIQ_LR          + WORD_SIZE )
SleepState_ABT_SP                  EQU     (SleepState_ABT_SPSR        + WORD_SIZE )
SleepState_ABT_LR                  EQU     (SleepState_ABT_SP          + WORD_SIZE )
SleepState_IRQ_SPSR                EQU     (SleepState_ABT_LR          + WORD_SIZE )
SleepState_IRQ_SP                  EQU     (SleepState_IRQ_SPSR        + WORD_SIZE )
SleepState_IRQ_LR                  EQU     (SleepState_IRQ_SP          + WORD_SIZE )
SleepState_UND_SPSR                EQU     (SleepState_IRQ_LR          + WORD_SIZE )
SleepState_UND_SP                  EQU     (SleepState_UND_SPSR        + WORD_SIZE )
SleepState_UND_LR                  EQU     (SleepState_UND_SP          + WORD_SIZE )
SleepState_SYS_SP                  EQU     (SleepState_UND_LR          + WORD_SIZE )
SleepState_SYS_LR                  EQU     (SleepState_SYS_SP          + WORD_SIZE )
SleepState_Data_End                EQU     (SleepState_SYS_LR          + WORD_SIZE )
SLEEPDATA_SIZE                     EQU     (SleepState_Data_End - SleepState_Data_Start) / 4

;from fw.s
PLLVAL  EQU     (((0xa1 << 12) + (0x3 << 4) + 0x1))

SLEEPDATA_BASE_VIRTUAL          EQU     0xA0020800 ; keep in sync w/ config.bib
DCACHE_LINES_PER_SET_BITS       EQU     6
DCACHE_LINES_PER_SET            EQU     64
DCACHE_NUM_SETS                 EQU     8
DCACHE_SET_INDEX_BIT            EQU     (32 - DCACHE_LINES_PER_SET_BITS)
DCACHE_LINE_SIZE                EQU     32

;stolen from reg2410.a
;******************************************************************************
; INTERRUPT
;******************************************************************************
vINTBASE    EQU 0xb0a00000      ;Interrupt request status
oSRCPND     EQU 0x00            ;Interrupt request status
oINTMSK     EQU 0x08            ;Interrupt mask control
oINTPND     EQU 0x10            ;Interrupt request status
oINTSUBMSK  EQU 0x1c            ;Interrupt sub mask

;******************************************************************************
; I/O PORT 
;******************************************************************************
vGPIOBASE   EQU 0xb1600000      ;Port A control
oGPFCON     EQU 0x50            ;Port F control
oGPGCON     EQU 0x60            ;Port G control
oGSTATUS3   EQU 0xb8            ;Saved data0(32-bit) before entering POWER_OFF mode 
vMISCCR     EQU 0xb1600080      ;Miscellaneous control

;******************************************************************************=========
; CLOCK & POWER MANAGEMENT
;******************************************************************************=========

vMPLLCON    EQU 0xb0c00004      ;MPLL Control

;******************************************************************************
; Memory control 
;******************************************************************************

vREFRESH    EQU 0xB0800024      ;DRAM/SDRAM refresh
vCLKCON     EQU 0xb0c0000c      ;Clock generator control

    IMPORT INTERRUPTS_ENABLE
    IMPORT OALFlushDCache
    IMPORT OALFlushICache
    IMPORT OALClearUTLB
    
    TEXTAREA

    ;**
    ; * CPUPowerReset - Software reset routine. Just jump to StartUp in this file.
    ; *
    ; *Entry none
    ; *Exit none
    ; *Uses r0-r3
    ; *

    LEAF_ENTRY CPUPowerReset
        mov     r0, #0                      ; disable interrupts - confusing things can happen if we
        bl      INTERRUPTS_ENABLE           ; get an interrupt while in the middle of disabling the MMU, etc.

        ldr     r3, =SLEEPDATA_BASE_VIRTUAL ; base of Sleep mode storage
        mov     r2, #0                      ; store Virtual return address
        str     r2, [r3], #4

        bl OALClearUTLB                     ;
        bl OALFlushICache                   ; nuke the cache
        bl OALFlushDCache                   ;

        ; Check to see if we're in NOR
        ldr     r2, =0xEA000000
        add     r2, r2, #0x3f0
        add     r2, r2, #0xe                        ; jump instruction: 0xEA0003FE.

        ldr     r3, =0x92000000                     ; base address 0x9200.0000.
        ldr     r1, [r3]
        cmp     r1, r2
        bne     %f10                                ; find jump instruction?  yes, must be NOR.
        ldr     r2, = PhysicalStartNOR              ; translate address for image in NOR
        ldr     r3, = 0x92000000
        sub     r2, r2, r3
        mov     r1, #0x0070                 ; Disable MMU
        mcr     p15, 0, r1, c1, c0, 0
        nop
        mov     pc, r2                      ; Jump to PStart
        nop

PhysicalStartNOR
        
        ldr     r2, =0x00000000             ; offset into the RAM 
        add     r2, r2, #0x1000             ; add physical base
        mov     pc, r2                      ; & jump to StartUp address

10

        ldr     r2, = PhysicalStart                 ; translate address for image in RAM
        ldr     r3, = (0x80000000 - 0x30000000)
        sub     r2, r2, r3
        
        mov     r1, #0x0070                 ; Disable MMU
        mcr     p15, 0, r1, c1, c0, 0
        nop
        mov     pc, r2                      ; Jump to PStart
        nop

        ; MMU & caches now disabled.

PhysicalStart

        ldr     r2, =0x21000                ; offset into the RAM 
        add     r2, r2, #0x30000000         ; add physical base
        mov     pc, r2                      ; & jump to StartUp address

    LEAF_ENTRY CPUPowerOff

        ; 1. Save register state and return address on the stack.
        ;
        stmdb   sp!, {r4-r12}                   
        stmdb   sp!, {lr}

        ; 2. Save MMU & CPU Registers to RAM.
        ;
        ldr     r3, =SLEEPDATA_BASE_VIRTUAL         ; base of Sleep mode storage

        ldr     r2, =Awake_address
        str     r2, [r3], #4                        ; save resume function address (virtual).
    
        mrc     p15, 0, r2, c1, c0, 0
        ldr     r0, =MMU_CTL_MASK
        bic     r2, r2, r0
        str     r2, [r3], #4                        ; save MMU control data.

        mrc     p15, 0, r2, c2, c0, 0
        ldr     r0, =MMU_TTB_MASK
        bic     r2, r2, r0
        str     r2, [r3], #4                        ; save TTB address.

        mrc     p15, 0, r2, c3, c0, 0
        str     r2, [r3], #4                        ; save domain access control.

        str     sp, [r3], #4                        ; save SVC mode stack pointer.

        mrs     r2, spsr
        str     r2, [r3], #4                        ; save SVC status register.

        mov     r1, #Mode_FIQ:OR:I_Bit:OR:F_Bit     ; enter FIQ mode, no interrupts.
        msr     cpsr, r1
        mrs     r2, spsr
        stmia   r3!, {r2, r8-r12, sp, lr}           ; save the FIQ mode registers.

        mov     r1, #Mode_ABT:OR:I_Bit:OR:F_Bit     ; enter ABT mode, no interrupts.
        msr     cpsr, r1
        mrs     r0, spsr
        stmia   r3!, {r0, sp, lr}                   ; save the ABT mode Registers.

        mov     r1, #Mode_IRQ:OR:I_Bit:OR:F_Bit     ; enter IRQ mode, no interrupts.
        msr     cpsr, r1
        mrs     r0, spsr
        stmia   r3!, {r0, sp, lr}                   ; save the IRQ Mode Registers.

        mov     r1, #Mode_UND:OR:I_Bit:OR:F_Bit     ; enter UND mode, no interrupts.
        msr     cpsr, r1
        mrs     r0, spsr
        stmia   r3!, {r0, sp, lr}                   ; save the UND mode Registers.

        mov     r1, #Mode_SYS:OR:I_Bit:OR:F_Bit     ; enter SYS mode, no interrupts.
        msr     cpsr, r1
        stmia   r3!, {sp, lr}                       ; save the SYS mode Registers.

        mov     r1, #Mode_SVC:OR:I_Bit:OR:F_Bit     ; back to SVC mode, no interrupts.
        msr     cpsr, r1

        ; 3. Compute the checksum on SleepData (verify integrity of data after resume).
        ;
        ldr     r3, =SLEEPDATA_BASE_VIRTUAL         ; get pointer to SLEEPDATA.
        mov     r2, #0
        ldr     r0, =SLEEPDATA_SIZE                 ; get size of data structure (in words).
30
        ldr     r1, [r3], #4                        ; compute the checksum.
        and     r1, r1, #0x1
        mov     r1, r1, LSL #31
        orr     r1, r1, r1, LSR #1
        add     r2, r2, r1
        subs    r0, r0, #1
        bne     %b30

        ldr     r0, =vGPIOBASE
        str     r2, [r0, #oGSTATUS3]                ; save the checksum in the Power Manager Scratch pad register.

        ; 4. Mask and clear all interrupts.
        ;
        ldr     r0, =vINTBASE
        mvn     r2, #0
        str     r2, [r0, #oINTMSK]
        str     r2, [r0, #oSRCPND]
        str     r2, [r0, #oINTPND]

        ; 5. Flush caches and TLBs.
        ;
        bl OALClearUTLB                     ;
        bl OALFlushICache                   ; nuke the cache
        bl OALFlushDCache                   ;

        ; 6. Set external wake-up interrupts (EINT0-2: power-button and keyboard).
        ;
        ldr     r0, =vGPIOBASE
        ldr     r1, =0x550a
        str     r1, [r0, #oGPFCON]

        ldr     r1, =0x55550100
        str     r1, [r0, #oGPGCON]

        ; 7. Switch to power-off mode.
        ;
        ldr     r0, =vMPLLCON
        ldr     r1, =PLLVAL
        str     r1, [r0]

        ; **These registers are used later during power-off.
        ;
        ldr     r0, =vREFRESH
        ldr     r1, [r0]                            ; r1 = rREFRESH.
        orr     r1, r1, #(1 << 22)

        ; **These registers are used later during power-off.
        ;
        ldr     r2, =vMISCCR
        ldr     r3, [r2]
        orr     r3, r3, #(7 << 17)                  ; make sure that SCLK0:SCLK->0, SCLK1:SCLK->0, SCKE=L during boot-up.

        ; **These registers are used later during power-off.
        ;
        ldr     r4, =vCLKCON
        ldr     r5, =0x7fff8                        ; power-off mode.

        ; Return to the bootloader code in flash.  This allows us to put the SDRAM in self-refresh.
        ; To determine whether we should jump to NOR flash or the SmartMedia, we look in both places
        ; for a jump instruction.
        ;    
        ; For NOR flash, the target address is 0x92001004.
        ; For NAND flash, the target address is 0x92000004.
        ;
        ldr     r8, =0xEA000000
        add     r8, r8, #0x3f0
        add     r8, r8, #0xe                        ; jump instruction: 0xEA0003FE.

        ldr     r6, =0x92000000                     ; base address 0x9200.0000.
        ldr     r7, [r6]
        cmp     r7, r8
        bne     %f50                                ; find jump instruction?  yes, must be NOR.
        add     r6, r6, #0x1000                     ; first page of loader code should be skipped.
50
        add     r6, r6, #0x4                        ; offset past initial branch instruction.

        mov     pc, r6                              ; jump to power-off code.
        ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

        b       SelfRefreshAndPowerOff
        
        ALIGN   32                                  ; for I-Cache Line(32Byte, 8 Word)

SelfRefreshAndPowerOff                              ; run with Instruction Cache's code
        str     r1, [r0]                            ; Enable SDRAM self-refresh
        str     r3, [r2]                            ; MISCCR Setting
        str     r5, [r4]                            ; Power Off !!
        b       .

        ; This point is called from EBOOT's startup code(MMU is enabled)
        ;       in this routine, left information(REGs, INTMSK, INTSUBMSK ...)

Awake_address

        ; 1. Recover CPU Registers

        ldr     r3, =SLEEPDATA_BASE_VIRTUAL         ; Sleep mode information data structure
        add     r2, r3, #SleepState_FIQ_SPSR
        mov     r1, #Mode_FIQ:OR:I_Bit:OR:F_Bit     ; Enter FIQ mode, no interrupts
        msr     cpsr, r1
        ldr     r0,  [r2], #4
        msr     spsr, r0
        ldr     r8,  [r2], #4
        ldr     r9,  [r2], #4
        ldr     r10, [r2], #4
        ldr     r11, [r2], #4
        ldr     r12, [r2], #4
        ldr     sp,  [r2], #4
        ldr     lr,  [r2], #4

        mov     r1, #Mode_ABT:OR:I_Bit:OR:F_Bit ; Enter ABT mode, no interrupts
        msr     cpsr, r1
        ldr     r0, [r2], #4
        msr     spsr, r0
        ldr     sp, [r2], #4
        ldr     lr, [r2], #4

        mov     r1, #Mode_IRQ:OR:I_Bit:OR:F_Bit ; Enter IRQ mode, no interrupts
        msr     cpsr, r1
        ldr     r0, [r2], #4
        msr     spsr, r0
        ldr     sp, [r2], #4
        ldr     lr, [r2], #4

        mov     r1, #Mode_UND:OR:I_Bit:OR:F_Bit ; Enter UND mode, no interrupts
        msr     cpsr, r1
        ldr     r0, [r2], #4
        msr     spsr, r0
        ldr     sp, [r2], #4
        ldr     lr, [r2], #4

        mov     r1, #Mode_SYS:OR:I_Bit:OR:F_Bit ; Enter SYS mode, no interrupts
        msr     cpsr, r1
        ldr     sp, [r2], #4
        ldr     lr, [r2]

        mov     r1, #Mode_SVC:OR:I_Bit:OR:F_Bit ; Enter SVC mode, no interrupts
        msr     cpsr, r1
        ldr     r0, [r3, #SleepState_SVC_SPSR]
        msr     spsr, r0

        ; 2. Recover Last mode's REG's, & go back to caller of CPUPowerOff()

        ldr     sp, [r3, #SleepState_SVC_SP]
        ldr     lr, [sp], #4

        ldmia   sp!, {r4-r12}
        mov     pc, lr                          ; and now back to our sponsors


    END

    ENTRY_END

END

