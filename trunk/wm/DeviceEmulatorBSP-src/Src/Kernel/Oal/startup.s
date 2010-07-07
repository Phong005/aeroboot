;
;  Copyright (c) Microsoft Corporation.  All rights reserved.
;
;
;  Use of this source code is subject to the terms of the Microsoft end-user
;  license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
;  If you did not accept the terms of the EULA, you are not authorized to use
;  this source code. For a copy of the EULA, please see the LICENSE.RTF on your
;  install media.
;
;------------------------------------------------------------------------------
;
;   File:  startup.s
;
;   Kernel startup routine for Samsung SMDK2410X board. Hardware is
;   initialized in boot loader - so there isn't much code at all.
;
;------------------------------------------------------------------------------

        INCLUDE kxarm.h

        IMPORT  KernelStart

        TEXTAREA
        
        ; Include memory configuration file with g_oalAddressTable

        INCLUDE oemaddrtab_cfg.inc
 
        LEAF_ENTRY StartUp

        ; Compute the OEMAddressTable's physical address and 
        ; load it into r0. KernelStart expects r0 to contain
        ; the physical address of this table. The MMU isn't 
        ; turned on until well into KernelStart.  

        add     r0, pc, #g_oalAddressTable - (. + 8)
        bl      KernelStart

        ENTRY_END 

        END

;------------------------------------------------------------------------------

