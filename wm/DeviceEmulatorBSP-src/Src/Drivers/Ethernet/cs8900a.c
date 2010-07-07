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
//  File:  cs8900a.c
//
#include <windows.h>
#include <ceddk.h>
#include <ethdbg.h>
#include <oal.h>
#include <nkintr.h>
#include <bsp.h>

//------------------------------------------------------------------------------

#define CS8900A_EISA_NUMBER         0x630e
#define RETRY_COUNT                 1000000

//------------------------------------------------------------------------------

//
// Make a stencil of the CS8900 device hardware. Use explicit types.
//
typedef struct {
    unsigned __int16 DATA0;
    unsigned __int16 DATA1;
    unsigned __int16 TXCMD;
    unsigned __int16 TXLENGTH;
    unsigned __int16 ISQ;
    unsigned __int16 PAGEIX;
    unsigned __int16 PAGE0;
    unsigned __int16 PAGE1;
} CS8900A_REGS;

static CS8900A_REGS *g_pCS8900;
static UINT16 g_hash[4];
static UINT32 g_filter;

//------------------------------------------------------------------------------

#define EISA_NUMBER                 0x0000
#define PRODUCT_ID_CODE             0x0002
#define IO_BASE_ADDRESS             0x0020
#define INTERRUPT_NUMBER            0x0022
#define DMA_CHANNEL_NUMBER          0x0024
#define DMA_START_OF_FRAME          0x0026
#define DMA_FRAME_COUNT             0x0028
#define RXDMA_BYTE_COUNT            0x002a
#define MEMORY_BASE_ADDR            0x002c
#define BOOT_PROM_BASE_ADDR         0x0030
#define BOOT_PROM_ADDR_MASK         0x0034
#define EEPROM_COMMAND              0x0040
#define EEPROM_DATA                 0x0042
#define RECEIVE_FRAME_BYTE_COUNT    0x0050

#define INT_SQ                      0x0120
#define RX_CFG                      0x0102
#define RX_EVENT                    0x0124
#define RX_CTL                      0x0104
#define TX_CFG                      0x0106
#define TX_EVENT                    0x0128
#define TX_CMD                      0x0108
#define BUF_CFG                     0x010A
#define BUF_EVENT                   0x012C
#define RX_MISS                     0x0130
#define TX_COL                      0x0132
#define LINE_CTL                    0x0112
#define LINE_ST                     0x0134
#define SELF_CTL                    0x0114
#define SELF_ST                     0x0136
#define BUS_CTL                     0x0116
#define BUS_ST                      0x0138
#define TEST_CTL                    0x0118
#define AUI_TIME_DOMAIN             0x013C
#define TX_CMD_REQUEST              0x0144
#define TX_CMD_LENGTH               0x0146

#define LOGICAL_ADDR_FILTER_BASE    0x0150
#define INDIVIDUAL_ADDRESS          0x0158

#define RX_STATUS                   0x0400
#define RX_LENGTH                   0x0402
#define RX_FRAME                    0x0404
#define TX_FRAME                    0x0a00

//------------------------------------------------------------------------------

#define ISQ_ID_MASK                 0x003F

#define SELF_CTL_RESET              (1 << 6)

#define SELF_ST_SIBUSY              (1 << 8)
#define SELF_ST_INITD               (1 << 7)

#define LINE_CTL_MOD_BACKOFF        (1 << 11)
#define LINE_CTL_AUI_ONLY           (1 << 8)
#define LINE_CTL_TX_ON              (1 << 7)
#define LINE_CTL_RX_ON              (1 << 6)

#define RX_CFG_RX_OK_IE             (1 << 8)
#define RX_CFG_SKIP_1               (1 << 6)

#define RX_CTL_BROADCAST            (1 << 11)
#define RX_CTL_INDIVIDUAL           (1 << 10)
#define RX_CTL_MULTICAST            (1 << 9)
#define RX_CTL_RX_OK                (1 << 8)
#define RX_CTL_PROMISCUOUS          (1 << 7)
#define RX_CTL_IAHASH               (1 << 6)

#define RX_EVENT_RX_OK              (1 << 8)
#define RX_EVENT_ID                 0x0004

#define TX_CMD_PAD_DIS              (1 << 13)
#define TX_CMD_INHIBIT_CRC          (1 << 12)
#define TX_CMD_ONECOLL              (1 << 9)
#define TX_CMD_FORCE                (1 << 8)
#define TX_CMD_START_5              (0 << 6)
#define TX_CMD_START_381            (1 << 6)
#define TX_CMD_START_1021           (2 << 6)
#define TX_CMD_START_ALL            (3 << 6)

#define BUS_ST_TX_RDY               (1 << 8)

#define BUS_CTL_ENABLE_IRQ          (1 << 15)

//------------------------------------------------------------------------------

#define pBSPArgs                    ((BSP_ARGS *) IMAGE_SHARE_ARGS_UA_START)

//------------------------------------------------------------------------------

static UINT16 ReadPacketPage(UINT16 address);
static VOID WritePacketPage(UINT16 address, UINT16 data);
static UINT32 ComputeCRC(UINT8 *pBuffer, UINT32 length);
extern BOOL CS8900DBG_Init(BYTE *iobase, ULONG membase, USHORT MacAddr[3]);

//------------------------------------------------------------------------------
//
//  Function:  CS8900AInit
//
BOOL
CS8900AInit(UINT8 *pAddress, UINT32 offset, UINT16 mac[3])
{
    UINT32 NumRetries;
    PBYTE  pBaseIOAddress = NULL;
    UINT32 MemoryBase = 0;  
    BOOL   rc = FALSE;

    OALMSGS(OAL_ETHER && OAL_FUNC, (
        L"+CS8900AInit(0x%08x, 0x%08x, %02x:%02x:%02x:%02x:%02x:%02x)\r\n",
        pAddress, offset, mac[0]&0xFF, mac[0]>>8, mac[1]&0xFF, mac[1]>>8,
        mac[2]&0xFF, mac[2]>>8
    ));

    // Save address
    g_pCS8900 = (CS8900A_REGS*)pAddress;

    // First check if there is chip
    if (ReadPacketPage(EISA_NUMBER) != CS8900A_EISA_NUMBER)
    {
        OALMSGS(OAL_ERROR, (L"ERROR: CS8900AInit: Failed detect chip\r\n"));
        goto Exit;
    }

    OALMSGS(OAL_INFO, (L"INFO: CS8900AInit chip detected\r\n"));

    // Initiate a reset sequence in software.
    WritePacketPage(SELF_CTL, SELF_CTL_RESET);

    // Wait for status to indicate initialization is complete.
    NumRetries = 0;
    while ((ReadPacketPage(SELF_ST) & SELF_ST_INITD) == 0)
    {
        if (NumRetries++ >= RETRY_COUNT)
        {
            OALMSGS(OAL_ERROR, (L"ERROR: CS8900AInit: Timed out during initialization\r\n"));
            goto Exit;
        }
    }

    // Wait for status to indicate the EEPROM is done being accessed/programmed.
    NumRetries = 0;
    while ((ReadPacketPage(SELF_ST) & SELF_ST_SIBUSY) != 0)
    {
        if (NumRetries++ >= RETRY_COUNT)
        {
            OALMSGS(OAL_ERROR, (L"ERROR: CS8900AInit: Timed out (EEPROM stayed busy)\r\n"));
            goto Exit;
        }
    }

    pBaseIOAddress   = (PBYTE)OALPAtoVA(pBSPArgs->kitl.devLoc.LogicalLoc, FALSE);
    MemoryBase       = (UINT32)OALPAtoVA(BSP_BASE_REG_PA_CS8900A_MEMBASE, FALSE);
    
    // Initialize the Ethernet controller, set MAC address
    //
    if (!CS8900DBG_Init((PBYTE)pBaseIOAddress, MemoryBase, mac))
    {
        OALMSG(OAL_ERROR, (TEXT("ERROR: InitEthDevice: Failed to initialize Ethernet controller.\r\n")));
        goto Exit;
    }

    // Enable receive
    WritePacketPage(RX_CTL, RX_CTL_RX_OK | RX_CTL_INDIVIDUAL | RX_CTL_BROADCAST);

    // Enable interrupt on receive
    WritePacketPage(RX_CFG, RX_CFG_RX_OK_IE);

    // Configure the hardware to use INTRQ0
    WritePacketPage(INTERRUPT_NUMBER, 0);

    // Enable
    WritePacketPage(LINE_CTL, LINE_CTL_RX_ON | LINE_CTL_TX_ON);

    // Make sure MAC address has been programmed.
    //
    if (!pBSPArgs->kitl.mac[0] && !pBSPArgs->kitl.mac[1] && !pBSPArgs->kitl.mac[2])
    {
        OALMSG(OAL_ERROR, (TEXT("ERROR: InitEthDevice: Invalid MAC address.\r\n")));
        goto Exit;
    }

    // Done
    rc = TRUE;

Exit:
    OALMSGS(OAL_ETHER && OAL_FUNC, (L"-CS8900AInit(rc = %d)\r\n", rc));
    return rc;
}

//------------------------------------------------------------------------------
//
//  Function:  CS8900ASendFrame
//
UINT16
CS8900ASendFrame(UINT8 *pData, UINT32 length)
{
    UINT32 NumRetries;
    BOOL   rc = TRUE;	// Default to failure code

    OALMSGS(OAL_ETHER && OAL_VERBOSE, (
        L"+CS8900ASendFrame(0x%08x, %d)\r\n", pData, length
    ));

    // Send Command
    OUTPORT16(&g_pCS8900->TXCMD, TX_CMD_START_ALL);
    OUTPORT16(&g_pCS8900->TXLENGTH, length);

    NumRetries = 0;
    while ((ReadPacketPage(BUS_ST) & BUS_ST_TX_RDY) == 0)
    {
        if (NumRetries++ >= RETRY_COUNT)
        {
            OALMSGS(OAL_ERROR, (L"ERROR: CS8900AInit: Not ready for TX\r\n"));
            goto Exit;
        }
    }

    length = (length + 1) >> 1;
    while (length-- > 0)
    {
        OUTPORT16(&g_pCS8900->DATA0, *(UINT16*)pData);
        pData += sizeof *(UINT16*)pData;
    }

    rc = FALSE;		// Success

Exit:
    OALMSGS(OAL_ETHER && OAL_VERBOSE, (L"-CS8900ASendFrame(rc = %d)\r\n", rc));
    return (rc);
}

//------------------------------------------------------------------------------
//
//  Function:  CS8900AGetFrame
//
UINT16
CS8900AGetFrame(UINT8 *pData, UINT16 *pLength)
{
    UINT16 isq, length, status, count, data;
    UINT16 DeviceInterrupt;
    BOOL   GlobalInterrupt;

    OALMSGS(OAL_ETHER && OAL_VERBOSE, (
        L"+CS8900AGetFrame(0x%08x, %d)\r\n", pData, *pLength
    ));

    //
    // This routine could be interrupt driven, or polled.
    // When polled, it keeps the emulator so busy that it
    // starves other threads. The emulator already has the
    // ability to recognize the polling in the bootloader
    // by noting whether the interrupt for this device is
    // disabled. In order to make the emulator recognize it
    // here as well, we need to detect the polling by noting
    // that *global* interrupts are disabled, then disable at
    // the device level as well if so.
    //
    GlobalInterrupt = INTERRUPTS_ENABLE(FALSE);	// Disable to obtain state
    INTERRUPTS_ENABLE(GlobalInterrupt);          // Immediately restore it.

    // Mimic the global interrupt state at the device level too.
    if (!GlobalInterrupt)	// If polled
    {
        // Disable at the device level too so the emulator knows we're polling.
        // Now it can yeild to other threads on the host side.
        DeviceInterrupt = ReadPacketPage(BUS_CTL);
        WritePacketPage(BUS_CTL, DeviceInterrupt & ~BUS_CTL_ENABLE_IRQ);
        DeviceInterrupt &= BUS_CTL_ENABLE_IRQ;
    }
        
    length = 0;
    isq = INPORT16(&g_pCS8900->ISQ);

    if ((isq & ISQ_ID_MASK) == RX_EVENT_ID
    &&  (isq & RX_EVENT_RX_OK) != 0)
    {
        // Get RxStatus and length
        status = INPORT16(&g_pCS8900->DATA0);
        length = INPORT16(&g_pCS8900->DATA0);

        if (length > *pLength)
        {
            // If packet doesn't fit in buffer, skip it
            data = ReadPacketPage(RX_CFG);
            WritePacketPage(RX_CFG, data | RX_CFG_SKIP_1);
            length = 0;
        } else {
            // Read packet data
            count = length;
            while (count > 1)
            {
                data = INPORT16(&g_pCS8900->DATA0);
                *(UINT16*)pData = data;
                pData += sizeof *(UINT16*)pData;
                count -= sizeof *(UINT16*)pData;
            }

            // Read last one byte
            if (count > 0)
            {
                data = INPORT16(&g_pCS8900->DATA0);
                *pData = (UINT8)data;
            }
        }
    }

    if (!GlobalInterrupt)	// If polled
    {
        if (DeviceInterrupt)    // If it was enabled at the device level,
        {
            // Re-enable at the device level.
            CS8900AEnableInts();
        }
    }

    // Return packet size
    *pLength = length;

    OALMSGS(OAL_ETHER && OAL_VERBOSE, (
        L"-CS8900AGetFrame(length = %d)\r\n", length
    ));

    return (length);
}

//------------------------------------------------------------------------------
//
//  Function:  CS8900AEnableInts
//
VOID
CS8900AEnableInts(VOID)
{
    UINT16 data;

    OALMSGS(OAL_ETHER && OAL_FUNC, (L"+CS8900AEnableInts\r\n"));

    data = ReadPacketPage(BUS_CTL);
    WritePacketPage(BUS_CTL, data | BUS_CTL_ENABLE_IRQ);

    OALMSGS(OAL_ETHER && OAL_FUNC, (L"-CS8900AEnableInts\r\n"));
}

//------------------------------------------------------------------------------
//
//  Function:  CS8900ADisableInts
//
VOID
CS8900ADisableInts(VOID)
{
    UINT16 data;

    OALMSGS(OAL_ETHER && OAL_FUNC, (L"+CS8900ADisableInts\r\n"));

    data = ReadPacketPage(BUS_CTL);
    WritePacketPage(BUS_CTL, data & ~BUS_CTL_ENABLE_IRQ);

    OALMSGS(OAL_ETHER && OAL_FUNC, (L"-CS8900ADisableInts\r\n"));
}

//------------------------------------------------------------------------------
//
//  Function:  CS8900ACurrentPacketFilter
//
VOID
CS8900ACurrentPacketFilter(UINT32 filter)
{
    UINT16 rxCtl;

    OALMSGS(OAL_ETHER && OAL_FUNC, (
        L"+CS8900ACurrentPacketFilter(0x%08x)\r\n", filter
    ));

    // Read current filter
    rxCtl = ReadPacketPage(RX_CTL);

    if ((filter & PACKET_TYPE_ALL_MULTICAST) != 0) {
        WritePacketPage(LOGICAL_ADDR_FILTER_BASE + 0, 0xFFFF);
        WritePacketPage(LOGICAL_ADDR_FILTER_BASE + 2, 0xFFFF);
        WritePacketPage(LOGICAL_ADDR_FILTER_BASE + 4, 0xFFFF);
        WritePacketPage(LOGICAL_ADDR_FILTER_BASE + 6, 0xFFFF);
    } else {
        WritePacketPage(LOGICAL_ADDR_FILTER_BASE + 0, g_hash[0]);
        WritePacketPage(LOGICAL_ADDR_FILTER_BASE + 2, g_hash[1]);
        WritePacketPage(LOGICAL_ADDR_FILTER_BASE + 4, g_hash[2]);
        WritePacketPage(LOGICAL_ADDR_FILTER_BASE + 6, g_hash[3]);
    }

    if ((filter & PACKET_TYPE_MULTICAST)
    ||  (filter & PACKET_TYPE_ALL_MULTICAST))
    {
        rxCtl |= RX_CTL_MULTICAST;
    } else {
        rxCtl &= ~RX_CTL_MULTICAST;
    }

    if (filter & PACKET_TYPE_PROMISCUOUS)
    {
        rxCtl |= RX_CTL_PROMISCUOUS;
    } else {
        rxCtl &= ~RX_CTL_PROMISCUOUS;
    }

    WritePacketPage(RX_CTL, rxCtl);

    // Save actual filter
    g_filter = filter;

    OALMSGS(OAL_ETHER && OAL_FUNC, (L"-CS8900ACurrentPacketFilter\r\n"));
}

//------------------------------------------------------------------------------
//
//  Function:  CS8900AMulticastList
//
BOOL
CS8900AMulticastList(UINT8 *pAddresses, UINT32 count)
{
    UINT32 i, j, crc, data, bit;

    OALMSGS(OAL_ETHER && OAL_FUNC, (
        L"+RTL8139MulticastList(0x%08x, %d)\r\n", pAddresses, count
    ));

    g_hash[0] = g_hash[1] = g_hash[2] = g_hash[3] = 0;
    for (i = 0; i < count; i++) {
        data = crc = ComputeCRC(pAddresses, 6);
        for (j = 0, bit = 0; j < 6; j++) {
            bit <<= 1;
            bit |= (data & 1);
            data >>= 1;
        }
        g_hash[bit >> 4] |= 1 << (bit & 0x0f);
        pAddresses += 6;
    }

    // But update only if all multicast mode isn't active
    if ((g_filter & PACKET_TYPE_ALL_MULTICAST) == 0) {        
        WritePacketPage(LOGICAL_ADDR_FILTER_BASE + 0, g_hash[0]);
        WritePacketPage(LOGICAL_ADDR_FILTER_BASE + 2, g_hash[1]);
        WritePacketPage(LOGICAL_ADDR_FILTER_BASE + 4, g_hash[2]);
        WritePacketPage(LOGICAL_ADDR_FILTER_BASE + 6, g_hash[3]);
    }

    OALMSGS(OAL_ETHER && OAL_FUNC, (L"-CS8900AMulticastList(rc = 1)\r\n"));

    return (TRUE);
}

//------------------------------------------------------------------------------

static UINT16
ReadPacketPage(UINT16 address)
{
    OUTREG16(&g_pCS8900->PAGEIX, address);
    return (INREG16(&g_pCS8900->PAGE0));
}

//------------------------------------------------------------------------------

static VOID
WritePacketPage(UINT16 address, UINT16 data)
{
    OUTREG16(&g_pCS8900->PAGEIX, address);
    OUTREG16(&g_pCS8900->PAGE0, data);
}

//------------------------------------------------------------------------------

static UINT32
ComputeCRC(UINT8 *pBuffer, UINT32 length)
{
    UINT32 crc, carry, i, j;
    UINT8 byte;

    crc = 0xffffffff;
    for (i = 0; i < length; i++) {
        byte = pBuffer[i];
        for (j = 0; j < 8; j++) {
            carry = ((crc & 0x80000000) ? 1 : 0) ^ (byte & 0x01);
            crc <<= 1;
            byte >>= 1;
            if (carry) crc = (crc ^ 0x04c11db6) | carry;
        }
    }

    return (crc);
}

