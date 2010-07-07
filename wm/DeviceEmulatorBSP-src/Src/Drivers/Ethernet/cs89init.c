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
/******************************************************************************
 *
 * System On Chip(SOC)
 *
 * Copyright (c) 2002 Software Center, Samsung Electronics, Inc.
 * All rights reserved.
 *
 * This software is the confidential and proprietary information of Samsung 
 * Electronics, Inc("Confidential Information"). You Shall not disclose such 
 * Confidential Information and shall use it only in accordance with the terms 
 * of the license agreement you entered into Samsung.
 *      
 ******************************************************************************
*/
    
#include <windows.h>
#include <halether.h>
#include "cs8900dbg.h"

// Hash creation constants.
//
#define CRC_PRIME               0xFFFFFFFF;
#define CRC_POLYNOMIAL          0x04C11DB6;

#define IOREAD(o)					((USHORT)*((volatile USHORT *)(dwEthernetIOBase + (o))))
#define IOWRITE(o, d)				*((volatile USHORT *)(dwEthernetIOBase + (o))) = (USHORT)(d)

#define MEMREAD(o)					((USHORT)*((volatile USHORT *)(dwEthernetMemBase + (o))))
#define MEMWRITE(o, d)				*((volatile USHORT *)(dwEthernetMemBase + (o))) = (USHORT)(d)

#define MAX_COUNT					0x100000

#define CS8900DBG_PROBE				(1 << 0)

static DWORD dwEthernetIOBase;
static DWORD dwEthernetMemBase;

#define CS8900_MEM_MODE

#ifdef	CS8900_MEM_MODE

#define READ_REG1					ReadReg
#define READ_REG2					MEMREAD

#define WRITE_REG1					WriteReg
#define WRITE_REG2					MEMWRITE

#else

#define READ_REG1					ReadReg
#define READ_REG2					ReadReg

#define WRITE_REG1					WriteReg
#define WRITE_REG2					WriteReg

#endif

static	BOOL bIsPacket;


static USHORT 
ReadReg(USHORT offset)
{
	IOWRITE(IO_PACKET_PAGE_POINTER, offset);
	return IOREAD(IO_PACKET_PAGE_DATA_0);
}

static void 
WriteReg(USHORT offset, USHORT data)
{
	IOWRITE(IO_PACKET_PAGE_POINTER, offset);
	IOWRITE(IO_PACKET_PAGE_DATA_0 , data);
}


static BOOL Probe(void)
{
	BOOL r = FALSE;

	do 
	{
		/* Check the EISA registration number.	*/
		if (READ_REG1(PKTPG_EISA_NUMBER) != CS8900_EISA_NUMBER)
		{
			RETAILMSG(1, (TEXT("ERROR: Probe: EISA Number Error.\r\n")));
			break;
		}
		/* Check the Product ID.				*/
		if ((READ_REG1(PKTPG_PRDCT_ID_CODE) & CS8900_PRDCT_ID_MASK)
			!= CS8900_PRDCT_ID)
		{
			RETAILMSG(1, (TEXT("ERROR: Probe: Product ID Error.\r\n")));
			break;
		}
	   
		RETAILMSG(1, (TEXT("INFO: Probe: CS8900 is detected.\r\n")));
		r = TRUE;
	} while (0);

	return r;
}

static BOOL 
Reset(void)
{
	BOOL r = FALSE;
	USHORT dummy;
	int i;
											/* Set RESET bit of SelfCTL register.	*/
	do 
	{
		//WRITE_REG1(PKTPG_SELF_CTL, SELF_CTL_RESET | SELF_CTL_LOW_BITS);
		WRITE_REG1(PKTPG_SELF_CTL, SELF_CTL_RESET);

								/* Wait until INITD bit of SelfST register is set.	*/
		for (i = 0; i < MAX_COUNT; i++)
		{
			dummy = READ_REG1(PKTPG_SELF_ST);
			if (dummy & SELF_ST_INITD) break;
		}

		if (i >= MAX_COUNT)
		{
			RETAILMSG(1, (TEXT("ERROR: Reset: Reset failed (SelfST).\r\n")));
			break;
		}

						/* Wait until SIBUSY bit of SelfST register is cleared.		*/
		for (i = 0; i < MAX_COUNT; i++)
		{
			dummy = READ_REG1(PKTPG_SELF_ST);
			if ((dummy & SELF_ST_SIBUSY) == 0) break;
		}

		if (i >= MAX_COUNT)
		{
			RETAILMSG(1, (TEXT("ERROR: Reset: Reset failed (SIBUSY).\r\n")));
			break;
		}
		r = TRUE;

	} while (0);

	return r;
}


static BOOL 
Init(USHORT *mac)
{
	USHORT temp;

#ifdef CS8900_MEM_MODE

	WRITE_REG1(PKTPG_MEMORY_BASE_ADDR     , (USHORT)(dwEthernetMemBase & 0xffff));
	WRITE_REG1(PKTPG_MEMORY_BASE_ADDR + 2 , (USHORT)(dwEthernetMemBase >> 16   ));
	WRITE_REG1(PKTPG_BUS_CTL              ,  BUS_CTL_MEMORY_E | BUS_CTL_LOW_BITS);

#endif

	temp = READ_REG2(PKTPG_LINE_CTL) | LINE_CTL_10_BASE_T | LINE_CTL_MOD_BACKOFF;
	WRITE_REG2(PKTPG_LINE_CTL, temp);
						
	WRITE_REG2(PKTPG_RX_CFG, RX_CFG_RX_OK_I_E | RX_CFG_LOW_BITS);

	WRITE_REG2(PKTPG_INDIVISUAL_ADDR + 0, *mac++);
	WRITE_REG2(PKTPG_INDIVISUAL_ADDR + 2, *mac++);
	WRITE_REG2(PKTPG_INDIVISUAL_ADDR + 4, *mac  );

	WRITE_REG2(PKTPG_RX_CTL, (RX_CTL_RX_OK_A | RX_CTL_IND_ADDR_A | RX_CTL_BROADCAST_A | RX_CTL_LOW_BITS));

	WRITE_REG2(PKTPG_TX_CFG, TX_CFG_LOW_BITS);

	temp = READ_REG2(PKTPG_LINE_CTL) | LINE_CTL_RX_ON | LINE_CTL_TX_ON;
	WRITE_REG2(PKTPG_LINE_CTL,temp);

	RETAILMSG(1, (TEXT("INFO: Init: CS8900_Init OK.\r\n")));
	return TRUE;
}

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

BOOL
CS8900DBG_Init(BYTE *iobase, ULONG membase, USHORT MacAddr[3])
{
	BOOL r = FALSE;

	bIsPacket         = FALSE;
	dwEthernetIOBase  = (DWORD)iobase;
	dwEthernetMemBase = membase;
    
	RETAILMSG(1, (TEXT("CS8900: MAC Address: %x:%x:%x:%x:%x:%x\r\n"),
				MacAddr[0] & 0x00FF, MacAddr[0] >> 8,
				MacAddr[1] & 0x00FF, MacAddr[1] >> 8,
				MacAddr[2] & 0x00FF, MacAddr[2] >> 8));
    do
	{
		if (!Reset())			break;
		if (!Probe())			break;
		if (!Init(MacAddr))		break;

		r = TRUE;
	} while (0);
	return r;
}

