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
#ifndef __NAND_H__
#define __NAND_H__

#define NAND_BLOCK_CNT          (1024 * 4)      /* Each Plane has 1024 Blocks   */
#define NAND_PAGE_CNT           (32)            /* Each Block has 32 Pages      */
#define NAND_PAGE_SIZE          (512)           /* Each Page has 512 Bytes      */
#define NAND_BLOCK_SIZE         (NAND_PAGE_CNT * NAND_PAGE_SIZE)

#define CMD_READID              0x90        //  ReadID
#define CMD_READ                0x00        //  Read
#define CMD_READ2               0x50        //  Read2
#define CMD_RESET               0xff        //  Reset
#define CMD_ERASE               0x60        //  Erase phase 1
#define CMD_ERASE2              0xd0        //  Erase phase 2
#define CMD_WRITE               0x80        //  Write phase 1
#define CMD_WRITE2              0x10        //  Write phase 2

/* !!! Maximum Delay Setting, Please Adjust these value to Optimize */
#define TACLS                   7      
#define TWRPH0                  7 
#define TWRPH1                  7 

#define NF_CMD(cmd)	            {s2410NAND->NFCMD   =  (cmd);}
#define NF_ADDR(addr)	        {s2410NAND->NFADDR  =  (unsigned char)(addr);}	
#define NF_nFCE_L()	            {s2410NAND->NFCONF &= ~(1 << 11);}
#define NF_nFCE_H()	            {s2410NAND->NFCONF |=  (1 << 11);}
#define NF_RSTECC()	            {s2410NAND->NFCONF |=  (1 << 12);}
#define NF_RDDATA() 	        (s2410NAND->NFDATA)
#define NF_WRDATA(data)         {s2410NAND->NFDATA  =  (data);}
#define NF_WAITRB()             {while (!(s2410NAND->NFSTAT & (1 << 0)));} 

#endif    // __NAND_H_.

