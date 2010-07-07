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
                INCLUDE armmacros.s
                INCLUDE s3c2410x.inc

;-------------------------------------------------------------------------------

MemoryMap       EQU     0x2a4
BANK_SIZE       EQU     0x00100000      ; 1MB per bank in MemoryMap array
BANK_SHIFT      EQU     20


;   Define RAM space for the Page Tables:
;
PHYBASE         EQU     0x30000000      ; physical start
PTs             EQU     0x30010000      ; 1st level page table address (PHYBASE + 0x10000)
                                        ; save room for interrupt vectors.

;-------------------------------------------------------------------------------

        TEXTAREA

        IMPORT  main

; Set up the MMU and Dcache for bootloader.
;
; This routine will initialize the first-level page table based up the contents
; of the MemoryMap array and enable the MMU and caches.
;
; Copy the image to RAM if it's not already running there.
;
; Include Files 



; Defines 

;------------------------------------------------------------------------------
; BUGBUG - ?

PLLVAL      EQU     (((0xa1 << 12) + (0x3 << 4) + 0x1))


;------------------------------------------------------------------------------
; Cache Configuration

DCACHE_LINES_PER_SET_BITS   EQU     (6)
DCACHE_LINES_PER_SET        EQU     (64)
DCACHE_NUM_SETS             EQU     (8)
DCACHE_SET_INDEX_BIT        EQU     (32 - DCACHE_LINES_PER_SET_BITS)
DCACHE_LINE_SIZE            EQU     (32)

;------------------------------------------------------------------------------
; Sleep state constants 
; 
; Location of sleep data 

; BUGBUG - this needs to be declared as a local var.

SLEEPDATA_BASE_PHYSICAL         EQU     0x30058000

; Sleep State memory locations

SleepState_Data_Start           EQU     (0)
SleepState_WakeAddr             EQU     (SleepState_Data_Start  + 0)
SleepState_MMUCTL               EQU     (SleepState_WakeAddr    + WORD_SIZE)
SleepState_MMUTTB               EQU     (SleepState_MMUCTL      + WORD_SIZE)
SleepState_MMUDOMAIN            EQU     (SleepState_MMUTTB      + WORD_SIZE)
SleepState_SVC_SP               EQU     (SleepState_MMUDOMAIN   + WORD_SIZE)
SleepState_SVC_SPSR             EQU     (SleepState_SVC_SP      + WORD_SIZE)
SleepState_FIQ_SPSR             EQU     (SleepState_SVC_SPSR    + WORD_SIZE)
SleepState_FIQ_R8               EQU     (SleepState_FIQ_SPSR    + WORD_SIZE)
SleepState_FIQ_R9               EQU     (SleepState_FIQ_R8      + WORD_SIZE)
SleepState_FIQ_R10              EQU     (SleepState_FIQ_R9      + WORD_SIZE)
SleepState_FIQ_R11              EQU     (SleepState_FIQ_R10     + WORD_SIZE)
SleepState_FIQ_R12              EQU     (SleepState_FIQ_R11     + WORD_SIZE)
SleepState_FIQ_SP               EQU     (SleepState_FIQ_R12     + WORD_SIZE)
SleepState_FIQ_LR               EQU     (SleepState_FIQ_SP      + WORD_SIZE)
SleepState_ABT_SPSR             EQU     (SleepState_FIQ_LR      + WORD_SIZE)
SleepState_ABT_SP               EQU     (SleepState_ABT_SPSR    + WORD_SIZE)
SleepState_ABT_LR               EQU     (SleepState_ABT_SP      + WORD_SIZE)
SleepState_IRQ_SPSR             EQU     (SleepState_ABT_LR      + WORD_SIZE)
SleepState_IRQ_SP               EQU     (SleepState_IRQ_SPSR    + WORD_SIZE)
SleepState_IRQ_LR               EQU     (SleepState_IRQ_SP      + WORD_SIZE)
SleepState_UND_SPSR             EQU     (SleepState_IRQ_LR      + WORD_SIZE)
SleepState_UND_SP               EQU     (SleepState_UND_SPSR    + WORD_SIZE)
SleepState_UND_LR               EQU     (SleepState_UND_SP      + WORD_SIZE)
SleepState_SYS_SP               EQU     (SleepState_UND_LR      + WORD_SIZE)
SleepState_SYS_LR               EQU     (SleepState_SYS_SP      + WORD_SIZE)
SleepState_Data_End             EQU     (SleepState_SYS_LR      + WORD_SIZE)

SLEEPDATA_SIZE                  EQU     (SleepState_Data_End - SleepState_Data_Start) / 4



; External Variables 

; External Functions 

; Global Variables 
 
; Local Variables 
 
; Local Functions 

;-------------------------------------------------------------------------------
;   Function: Startup
;
;   Main entry point for CPU initialization.
;

        STARTUPTEXT
        LEAF_ENTRY      StartUp
    
        ; Jump over power-off code. 
        b       ResetHandler

        ; Power-off the CPU. 
        str     r1, [r0]                        ; enable SDRAM self-refresh.
        str     r3, [r2]                        ; MISCCR setting.
        str     r5, [r4]                        ; POWER OFF!!!!!
        b       .
    
ResetHandler

        ; Make sure that TLB & cache are consistent
        mov     r0, #0
        mcr     p15, 0, r0, c8, c7, 0           ; flush both TLB
        mcr     p15, 0, r0, c7, c5, 0           ; invalidate instruction cache
        mcr     p15, 0, r0, c7, c6, 0           ; invalidate data cache
        
        ldr     r0, = GPFCON
        ldr     r1, = 0x55aa      
        str     r1, [r0]

        ldr     r0, = WTCON                     ; disable watch dog
        ldr     r1, = 0x0         
        str     r1, [r0]

        ldr     r0, = INTMSK
        ldr     r1, = 0xffffffff                ; disable all interrupts
        str     r1, [r0]

        ldr     r0, = INTSUBMSK
        ldr     r1, = 0x7ff                     ; disable all sub interrupt
        str     r1, [r0]

        ldr     r0, = INTMOD
        mov     r1, #0x0                        ; set all interrupt as IRQ
        str     r1, [r0]

        ldr     r0, = CLKDIVN
        ldr     r1, = 0x3                       ; 0x0 = 1:1:1,  0x1 = 1:1:2
                                                ; 0x2 = 1:2:2,  0x3 = 1:2:4,
                                                ; 0x8 = 1:4:4
        str     r1, [r0]

        ands    r1, r1, #0x2                    ; set AsyncBusMode
        beq     %F10

        mrc     p15, 0, r0, c1, c0, 0
        orr     r0, r0, #R1_nF:OR:R1_iA
        mcr     p15, 0, r0, c1, c0, 0
10
        ldr     r0, = LOCKTIME                  ; To reduce PLL lock time, adjust the LOCKTIME register. 
        ldr     r1, = 0xffffff
        str     r1, [r0]
    
        ldr     r0, = MPLLCON                   ; Configure MPLL
                                                ; Fin=12MHz, Fout=50MHz
        ldr     r1, = PLLVAL
        str     r1, [r0]

        ldr     r0, = UPLLCON                   ; Fin=12MHz, Fout=48MHz
        ldr     r1, = ((0x48 << 12) + (0x3 << 4) + 0x2)  
        str     r1, [r0]

        mov     r0, #0x2000
20   
        subs    r0, r0, #1
        bne     %B20

;------------------------------------------------------------------------------
;   Add for Power Management 

        ldr     r1, =GSTATUS2                   ; Determine Booting Mode
        ldr     r10, [r1]
        tst     r10, #0x2
        beq     %F30                            ; if not wakeup from PowerOffmode
                                                ;    skip MISCCR setting

        ldr     r1, =MISCCR                     ; MISCCR's Bit 17, 18, 19 -> 0
        ldr     r0, [r1]                        ; I don't know why, Just fallow Sample Code.
        bic     r0, r0, #(7 << 17)              ; SCLK0:0->SCLK, SCLK1:0->SCLK, SCKE:L->H
        str     r0, [r1]
30

;------------------------------------------------------------------------------
;   Initialize memory controller


        add     r0, pc, #MEMCTRLTAB - (. + 8)
        ldr     r1, = BWSCON                    ; BWSCON Address
        add     r2, r0, #52                     ; End address of MEMCTRLTAB
40      ldr     r3, [r0], #4    
        str     r3, [r1], #4    
        cmp     r2, r0      
        bne     %B40

;------------------------------------------------------------------------------
;   Add for Power Management 

        tst     r10, #0x2
        beq     BringUpWinCE                    ; Normal Mode Booting

;------------------------------------------------------------------------------
;   Recover Process : Starting Point
;
;   1. Checksum Calculation saved Data

        ldr     r5, =SLEEPDATA_BASE_PHYSICAL    ; pointer to physical address of reserved Sleep mode info data structure 
        mov     r3, r5                          ; pointer for checksum calculation
        mov     r2, #0
        ldr     r0, =SLEEPDATA_SIZE             ; get size of data structure to do checksum on
50      ldr     r1, [r3], #4                    ; pointer to SLEEPDATA
        and     r1, r1, #0x1
        mov     r1, r1, LSL #31
        orr     r1, r1, r1, LSR #1
        add     r2, r2, r1
        subs    r0, r0, #1                      ; dec the count
        bne     %b50                            ; loop till done    

        ldr     r0,=GSTATUS3
        ldr     r3, [r0]                        ; get the Sleep data checksum from the Power Manager Scratch pad register
        teq     r2, r3                          ; compare to what we saved before going to sleep
        bne     BringUpWinCE                    ; bad news - do a cold boot

;   2. MMU Enable

        ldr     r10, [r5, #SleepState_MMUDOMAIN] ; load the MMU domain access info
        ldr     r9,  [r5, #SleepState_MMUTTB]    ; load the MMU TTB info 
        ldr     r8,  [r5, #SleepState_MMUCTL]    ; load the MMU control info 
        ldr     r7,  [r5, #SleepState_WakeAddr ] ; load the LR address
        nop         
        nop
        nop
        nop
        nop

; if software reset

        mov     r1, #0
        teq     r1, r7
        bne     %f60
        bl      BringUpWinCE

; wakeup routine
60      mcr     p15, 0, r10, c3, c0, 0          ; setup access to domain 0
        mcr     p15, 0, r9,  c2, c0, 0          ; PT address
        mcr     p15, 0, r0,  c8, c7, 0          ; flush I+D TLBs
        mcr     p15, 0, r8,  c1, c0, 0          ; restore MMU control

;   3. Jump to Kernel Image's fw.s (Awake_address)

        mov     pc, r7                          ;  jump to new VA (back up Power management stack)
        nop

;------------------------------------------------------------------------------
;   Add for Power Management ?

BringUpWinCE
    
        ldr     r0, = GPFDAT
        mov     r1, #0x60
        str     r1, [r0]

;------------------------------------------------------------------------------
;   Copy boot loader to memory

        ands    r9, pc, #0xFF000000     ; see if we are in flash or in ram
        bne     %f20                    ; go ahead if we are already in ram

        ; This is the loop that perform copying.
        ldr     r0, = 0x38000           ; offset into the RAM 
        add     r0, r0, #PHYBASE        ; add physical base
        mov     r1, r0                  ; (r1) copy destination
        ldr     r2, =0x0                ; (r2) flash started at physical address 0
        ldr     r3, =0x10000            ; counter (0x40000/4)
10      ldr     r4, [r2], #4
        str     r4, [r1], #4
        subs    r3, r3, #1
        bne     %b10

        ; Restart from the RAM position after copying.
        mov pc, r0
        nop
        nop
        nop

        ; Shouldn't get here.
        b       .

        INCLUDE oemaddrtab_cfg.inc
 

        ; Compute physical address of the OEMAddressTable.
20      add     r11, pc, #g_oalAddressTable - (. + 8)
        ldr     r10, =PTs                ; (r10) = 1st level page table


        ; Setup 1st level page table (using section descriptor)     
        ; Fill in first level page table entries to create "un-mapped" regions
        ; from the contents of the MemoryMap array.
        ;
        ;   (r10) = 1st level page table
        ;   (r11) = ptr to MemoryMap array

        add     r10, r10, #0x2000       ; (r10) = ptr to 1st PTE for "unmapped space"
        mov     r0, #0x0E               ; (r0) = PTE for 0: 1MB cachable bufferable
        orr     r0, r0, #0x400          ; set kernel r/w permission
25      mov     r1, r11                 ; (r1) = ptr to MemoryMap array

        
30      ldr     r2, [r1], #4            ; (r2) = virtual address to map Bank at
        ldr     r3, [r1], #4            ; (r3) = physical address to map from
        ldr     r4, [r1], #4            ; (r4) = num MB to map

        cmp     r4, #0                  ; End of table?
        beq     %f40

        ldr     r5, =0x1FF00000
        and     r2, r2, r5              ; VA needs 512MB, 1MB aligned.                

        ldr     r5, =0xFFF00000
        and     r3, r3, r5              ; PA needs 4GB, 1MB aligned.

        add     r2, r10, r2, LSR #18
        add     r0, r0, r3              ; (r0) = PTE for next physical page

35      str     r0, [r2], #4
        add     r0, r0, #0x00100000     ; (r0) = PTE for next physical page
        sub     r4, r4, #1              ; Decrement number of MB left 
        cmp     r4, #0
        bne     %b35                    ; Map next MB

        bic     r0, r0, #0xF0000000     ; Clear Section Base Address Field
        bic     r0, r0, #0x0FF00000     ; Clear Section Base Address Field
        b       %b30                    ; Get next element
        
40      tst     r0, #8
        bic     r0, r0, #0x0C           ; clear cachable & bufferable bits in PTE
        add     r10, r10, #0x0800       ; (r10) = ptr to 1st PTE for "unmapped uncached space"
        bne     %b25                    ; go setup PTEs for uncached space
        sub     r10, r10, #0x3000       ; (r10) = restore address of 1st level page table

        ; Setup mmu to map (VA == 0) to (PA == 0x30000000).
        ldr     r0, =PTs                ; PTE entry for VA = 0
        ldr     r1, =0x3000040E         ; uncache/unbuffer/rw, PA base == 0x30000000
        str     r1, [r0]

        ; uncached area.
        add     r0, r0, #0x0800         ; PTE entry for VA = 0x0200.0000 , uncached     
        ldr     r1, =0x30000402         ; uncache/unbuffer/rw, base == 0x30000000
        str     r1, [r0]
        
        ; Comment:
        ; The following loop is to direct map RAM VA == PA. i.e. 
        ;   VA == 0x30XXXXXX => PA == 0x30XXXXXX for S3C2400
        ; Fill in 8 entries to have a direct mapping for DRAM
        ;
        ldr     r10, =PTs               ; restore address of 1st level page table
        ldr     r0,  =PHYBASE

        add     r10, r10, #(0x3000 / 4) ; (r10) = ptr to 1st PTE for 0x30000000

        add     r0, r0, #0x1E           ; 1MB cachable bufferable
        orr     r0, r0, #0x400          ; set kernel r/w permission
        mov     r1, #0 
        mov     r3, #64
45      mov     r2, r1                  ; (r2) = virtual address to map Bank at
        cmp     r2, #0x20000000:SHR:BANK_SHIFT
        add     r2, r10, r2, LSL #BANK_SHIFT-18
        strlo   r0, [r2]
        add     r0, r0, #0x00100000     ; (r0) = PTE for next physical page
        subs    r3, r3, #1
        add     r1, r1, #1
        bgt     %b45

        ldr     r10, =PTs               ; (r10) = restore address of 1st level page table

        ; The page tables and exception vectors are setup.
        ; Initialize the MMU and turn it on.
        mov     r1, #1
        mcr     p15, 0, r1, c3, c0, 0   ; setup access to domain 0
        mcr     p15, 0, r10, c2, c0, 0

        mcr     p15, 0, r0, c8, c7, 0   ; flush I+D TLBs
        mov     r1, #0x0071             ; Enable: MMU
        orr     r1, r1, #0x0004         ; Enable the cache

        ldr     r0, =VirtualStart

        cmp     r0, #0                  ; make sure no stall on "mov pc,r0" below
        mcr     p15, 0, r1, c1, c0, 0
        mov     pc, r0                  ;  & jump to new virtual address
        nop

        ; MMU & caches now enabled.
        ;   (r10) = physcial address of 1st level page table
        ;

VirtualStart

        mov     sp, #0x8C000000
        add     sp, sp, #0x30000        ; arbitrary initial super-page stack pointer
        b       main

        ENTRY_END
 
        LTORG

;------------------------------------------------------------------------------
; Memory Controller Configuration 
;
;   The below defines are used in the MEMCTRLTAB table
;   defined below to iniatialize the memory controller's
;   register bank.
;
; SDRAM refresh control register configuration

REFEN       EQU     (0x1)               ; Refresh enable
TREFMD      EQU     (0x0)               ; CBR(CAS before RAS)/Auto refresh
Trp         EQU     (0x0)               ; 2clk
Trc         EQU     (0x3)               ; 7clk
Tchr        EQU     (0x2)               ; 3clk
REFCNT      EQU     (1113)              ; period=15.6us, HCLK=60Mhz, (2048+1-15.6*60)

; Bank Control 
;
; Bus width and wait status control 

B1_BWSCON   EQU     (DW32)
B2_BWSCON   EQU     (DW16)
B3_BWSCON   EQU     (DW16 + WAIT + UBLB)
B4_BWSCON   EQU     (DW16)
B5_BWSCON   EQU     (DW16)
B6_BWSCON   EQU     (DW32)
B7_BWSCON   EQU     (DW32)

; Bank 0

B0_Tacs     EQU     (0x0)    ; 0clk
B0_Tcos     EQU     (0x0)    ; 0clk
B0_Tacc     EQU     (0x7)    ; 14clk
B0_Tcoh     EQU     (0x0)    ; 0clk
B0_Tah      EQU     (0x0)    ; 0clk
B0_Tacp     EQU     (0x0)    
B0_PMC      EQU     (0x0)    ; normal

; Bank 1

B1_Tacs     EQU     (0x0)    ; 0clk
B1_Tcos     EQU     (0x0)    ; 0clk
B1_Tacc     EQU     (0x7)    ; 14clk
B1_Tcoh     EQU     (0x0)    ; 0clk
B1_Tah      EQU     (0x0)    ; 0clk
B1_Tacp     EQU     (0x0)    
B1_PMC      EQU     (0x0)    ; normal

; Bank 2

B2_Tacs     EQU     (0x0)    ; 0clk
B2_Tcos     EQU     (0x0)    ; 0clk
B2_Tacc     EQU     (0x7)    ; 14clk
B2_Tcoh     EQU     (0x0)    ; 0clk
B2_Tah      EQU     (0x0)    ; 0clk
B2_Tacp     EQU     (0x0)     
B2_PMC      EQU     (0x0)    ; normal

; Bank 3

B3_Tacs     EQU     (0x0)    ; 0clk
B3_Tcos     EQU     (0x0)    ; 0clk
B3_Tacc     EQU     (0x7)    ; 14clk
B3_Tcoh     EQU     (0x0)    ; 0clk
B3_Tah      EQU     (0x0)    ; 0clk
B3_Tacp     EQU     (0x0)    
B3_PMC      EQU     (0x0)    ; normal

; Bank 4

B4_Tacs     EQU     (0x0)    ; 0clk
B4_Tcos     EQU     (0x0)    ; 0clk
B4_Tacc     EQU     (0x7)    ; 14clk
B4_Tcoh     EQU     (0x0)    ; 0clk
B4_Tah      EQU     (0x0)    ; 0clk
B4_Tacp     EQU     (0x0)    
B4_PMC      EQU     (0x0)    ; normal

; Bank 5

B5_Tacs     EQU     (0x0)    ; 0clk
B5_Tcos     EQU     (0x0)    ; 0clk
B5_Tacc     EQU     (0x7)    ; 14clk
B5_Tcoh     EQU     (0x0)    ; 0clk
B5_Tah      EQU     (0x0)    ; 0clk
B5_Tacp     EQU     (0x0)    
B5_PMC      EQU     (0x0)    ; normal

; Bank 6

B6_MT       EQU     (0x3)    ; SDRAM
B6_Trcd     EQU     (0x1)    ; 3clk
B6_SCAN     EQU     (0x1)    ; 9bit

; Bank 7
;
; Note - there is no memory connected to Bank 7

B7_MT       EQU     (0x3)    ; SDRAM
B7_Trcd     EQU     (0x1)    ; 3clk
B7_SCAN     EQU     (0x1)    ; 9bit


;------------------------------------------------------------------------------
;   Memory Controller Configuration Data Table
;
;   This data block is loaded into the memory controller's 
;   registers to configure the platform memory.
;

MEMCTRLTAB DATA
        DCD (0+(B1_BWSCON<<4)+(B2_BWSCON<<8)+(B3_BWSCON<<12)+(B4_BWSCON<<16)+(B5_BWSCON<<20)+(B6_BWSCON<<24)+(B7_BWSCON<<28))
        DCD ((B0_Tacs<<13)+(B0_Tcos<<11)+(B0_Tacc<<8)+(B0_Tcoh<<6)+(B0_Tah<<4)+(B0_Tacp<<2)+(B0_PMC))   ; BANKCON0
        DCD ((B1_Tacs<<13)+(B1_Tcos<<11)+(B1_Tacc<<8)+(B1_Tcoh<<6)+(B1_Tah<<4)+(B1_Tacp<<2)+(B1_PMC))   ; BANKCON1 
        DCD ((B2_Tacs<<13)+(B2_Tcos<<11)+(B2_Tacc<<8)+(B2_Tcoh<<6)+(B2_Tah<<4)+(B2_Tacp<<2)+(B2_PMC))   ; BANKCON2
        DCD ((B3_Tacs<<13)+(B3_Tcos<<11)+(B3_Tacc<<8)+(B3_Tcoh<<6)+(B3_Tah<<4)+(B3_Tacp<<2)+(B3_PMC))   ; BANKCON3
        DCD ((B4_Tacs<<13)+(B4_Tcos<<11)+(B4_Tacc<<8)+(B4_Tcoh<<6)+(B4_Tah<<4)+(B4_Tacp<<2)+(B4_PMC))   ; BANKCON4
        DCD ((B5_Tacs<<13)+(B5_Tcos<<11)+(B5_Tacc<<8)+(B5_Tcoh<<6)+(B5_Tah<<4)+(B5_Tacp<<2)+(B5_PMC))   ; BANKCON5
        DCD ((B6_MT<<15)+(B6_Trcd<<2)+(B6_SCAN))                                                        ; BANKCON6
        DCD ((B7_MT<<15)+(B7_Trcd<<2)+(B7_SCAN))                                                        ; BANKCON7
        DCD ((REFEN<<23)+(TREFMD<<22)+(Trp<<20)+(Trc<<18)+(Tchr<<16)+REFCNT)                            ; REFRESH
        DCD 0xB2                                                                                        ; BANKSIZE
        DCD 0x30                                                                                        ; MRSRB6
        DCD 0x30                                                                                        ; MRSRB7

        END

;-------------------------------------------------------------------------------        
