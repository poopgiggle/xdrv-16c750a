/*
 * This file is part of x-16c750
 *
 * Copyright (C) 2011, 2012 - Nenad Radulovic
 *
 * x-16c750 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * x-16c750 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with x-16c750; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * web site:    http://blueskynet.dyndns-server.com
 * e-mail  :    blueskyniss@gmail.com
 *//***********************************************************************//**
 * @file
 * @author      Nenad Radulovic
 * @brief       Low Level Driver (LLD) for 16C750 UART hardware
 *********************************************************************//** @{ */

/*=========================================================  INCLUDE FILES  ==*/

#include "plat/dma.h"

#include "drv/x-16c750_ioctl.h"
#include "drv/x-16c750_lld.h"
#include "port/port.h"

/*=========================================================  LOCAL MACRO's  ==*/

#define U16_LOW_BYTE(val)                                                       \
    ((val) & 0x00FFU)

#define U16_HIGH_BYTE(val)                                                      \
    ((val) >> 8)

#if (CFG_FIFO_TRIG <= 8)
# define FIFO_RX_LVL                    TLR_RX_FIFO_TRIG_DMA_8
# define FIFO_TX_LVL                    TLR_TX_FIFO_TRIG_DMA_8
#elif (CFG_FIFO_TRIG <= 16)
# define FIFO_RX_LVL                    TLR_RX_FIFO_TRIG_DMA_16
# define FIFO_TX_LVL                    TLR_TX_FIFO_TRIG_DMA_16
#else
# define FIFO_RX_LVL                    TLR_RX_FIFO_TRIG_DMA_56
# define FIFO_TX_LVL                    TLR_TX_FIFO_TRIG_DMA_56
#endif

#define TX_FIFO_SIZE                    64U

/*======================================================  LOCAL DATA TYPES  ==*/

const struct xUartProto DefProtocol = {
    .baud               = CFG_DEFAULT_BAUD_RATE,
    .parity             = XUART_PARITY_NONE,
    .dataBits           = XUART_DATA_8,
    .stopBits           = XUART_STOP_1
};

/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/
/*=======================================================  LOCAL VARIABLES  ==*/
/*======================================================  GLOBAL VARIABLES  ==*/
/*============================================  LOCAL FUNCTION DEFINITIONS  ==*/
/*===================================  GLOBAL PRIVATE FUNCTION DEFINITIONS  ==*/
/*====================================  GLOBAL PUBLIC FUNCTION DEFINITIONS  ==*/

void lldRegWrBits(
    volatile uint8_t *  io,
    enum hwReg          reg,
    uint16_t            bitmask,
    uint16_t            bits) {

    uint16_t            tmp;

    tmp = lldRegRd(io, reg);
    tmp &= ~bitmask;
    tmp |= (bits & bitmask);
    lldRegWr(io, reg, tmp);
}

uint16_t lldRegRdBits(
    volatile uint8_t *  io,
    enum hwReg          reg,
    uint16_t            bitmask) {

    uint16_t            tmp;

    tmp = lldRegRd(io, reg);
    tmp &= ~bitmask;

    return (tmp);
}

void lldCfgModeSet(
    volatile uint8_t *  io,
    enum lldCfgMode     cfgMode) {

    lldRegWr(
        io,
        LCR,
        cfgMode);
}

void lldModeSet(
    volatile uint8_t *  io,
    enum lldMode        mode) {

    lldRegWrBits(
        io,
        MDR1,
        MDR1_MODESELECT_Mask,
        mode);
}

uint16_t lldModeGet(
    volatile uint8_t *  io) {

    uint16_t            tmp;

    tmp = lldRegRdBits(
        io,
        MDR1,
        MDR1_MODESELECT_Mask);

    return (tmp);
}

void lldIntEnable(
    volatile uint8_t *  io,
    enum lldIntNum      intNum) {

    uint16_t            tmp;

    switch (intNum) {
        case LLD_INT_TX : {
            tmp = IER_THRIT;
            break;
        }
        case LLD_INT_RX : {
            tmp = IER_RHRIT;
            break;
        }
        case LLD_INT_RX_TIMEOUT : {
            tmp = IER_RHRIT;
            break;
        }
        case LLD_INT_LINEST : {
            tmp = IER_LINESTSIT;
            break;
        }
        case LLD_INT_ALL : {
            tmp = 0x00FFU;
            break;
        }
        default : {
            tmp = 0U;
            break;
        }
    }
    lldRegSetBits(
        io,
        IER,
        tmp);
}

void lldIntDisable(
    volatile uint8_t *  io,
    enum lldIntNum      intNum) {

    uint16_t            tmp;

    switch (intNum) {
        case LLD_INT_TX : {
            tmp = IER_THRIT;
            break;
        }
        case LLD_INT_RX : {
            tmp = IER_RHRIT;
            break;
        }
        case LLD_INT_RX_TIMEOUT : {
            tmp = IER_RHRIT;
            break;
        }
        case LLD_INT_LINEST : {
            tmp = IER_LINESTSIT;
            break;
        }
        case LLD_INT_ALL : {
            tmp = 0x00FFU;
            break;
        }
        default : {
            tmp = 0U;
            break;
        }
    }
    lldRegResetBits(
        io,
        IER,
        tmp);
}

void lldEnhanced(
    volatile uint8_t *  io,
    enum lldState       state) {

    uint16_t            regLCR;
    uint16_t            tmp;

    regLCR = lldRegRd(io, LCR);
    lldRegWr(io, LCR, LLD_CFG_MODE_B);
    lldCfgModeSet(
        io,
        LLD_CFG_MODE_B);
    tmp = lldRegRd(
        io,
        bEFR);

    switch (state) {
        case LLD_ENABLE : {
            tmp |= EFR_ENHANCEDEN;
            break;
        }
        case LLD_DISABLE : {
            tmp &= ~EFR_ENHANCEDEN;
            break;
        }
        default : {
            break;
        }
    }
    lldRegWr(
        io,
        wbEFR,
        tmp);
    lldRegWr(
        io,
        LCR,
        regLCR);
}

int32_t lldSoftReset(
    volatile uint8_t *  io) {

    int32_t             retval;
    uint32_t            cnt;

    lldRegWr(
        io,
        SYSC,
        SYSC_SOFTRESET);
    cnt = 1000U;

    while ((0 == (SYSS_RESETDONE & lldRegRd(io, SYSS))) && (0U != cnt)) {
        --cnt;
    }

    if (0U != cnt) {
        retval = 0;
    } else {
        retval = -EAGAIN;
    }

    return (retval);
}

int32_t lldInit(
    volatile uint8_t *  io) {

    int                 retval;

    retval = lldSoftReset(
            io);

    if (0 != retval) {

        return (retval);
    }
    lldFIFODMAInit(
        io);
    lldEnhanced(
        io,
        LLD_ENABLE);
    lldFIFORxFlush(
        io);
    lldFIFOTxFlush(
        io);

    return (0);
}

int32_t lldTerm(
    volatile uint8_t *  io) {

    return (0);
}

void lldFIFORxGranularitySet(
    volatile uint8_t *  io,
    size_t              bytes) {

    /*
     * TODO: napisati ovo
     */
}

void lldFIFORxGranularityState(
    volatile uint8_t *  io,
    enum lldState       state) {

    /*
     * TODO: napisati ovo
     */
}

size_t lldFIFORxOccupied(
    volatile uint8_t *  io) {

    return (lldRegRd(io, RXFIFO_LVL));
}

size_t lldFIFOTxFree(
    volatile uint8_t *  io) {

    return (TX_FIFO_SIZE - lldRegRd(io, TXFIFO_LVL));
}

void lldFIFORxFlush(
    volatile uint8_t *  io) {

    enum lldIntNum      intNum;

    lldRegSetBits(
        io,
        wFCR,
        FCR_RX_FIFO_CLEAR);
    intNum = lldIntGet(
        io);

    while ((LLD_INT_RX == intNum) || (LLD_INT_RX_TIMEOUT == intNum)) {          /* Loop until there are interrupts to process               */
        lldRegRd(
            io,
            RHR);
        intNum = lldIntGet(
            io);
    }
}

void lldFIFOTxFlush(
    volatile uint8_t *  io) {

    lldRegSetBits(
        io,
        wFCR,
        FCR_TX_FIFO_CLEAR);
}

void lldFIFODMAInit(
    volatile uint8_t *  io) {

    uint16_t            regLCR;
    uint16_t            regEFR;
    uint16_t            regMCR;

    regLCR = lldRegRd(
        io,
        LCR);
    lldCfgModeSet(                                                              /* Switch to register configuration mode B to access the    */
        io,                                                                     /* EFR register                                             */
        LLD_CFG_MODE_B);
    regEFR = lldRegRd(
        io,
        bEFR);
    lldRegWr(                                                                   /* Enable register submode TCR_TLR to access the TLR        */
        io,                                                                     /* register (1/2)                                           */
        wbEFR,
        regEFR | EFR_ENHANCEDEN);
    lldCfgModeSet(                                                              /* Switch to register configuration mode A to access the DLL*/
        io,                                                                     /* DLH and MCR registers                                    */
        LLD_CFG_MODE_A);
    lldRegWr(
        io,
        waDLL,
        0);                                                                     /* Stop divisor to access FCR higher bits                   */
    lldRegWr(
        io,
        waDLH,
        0);                                                                     /* Stop divisor to access FCR higher bits                   */
    regMCR = lldRegRd(
        io,
        aMCR);
    lldRegWr(                                                                   /* Enable register submode TCR_TLR to access the TLR        */
        io,                                                                     /* register (2/2)                                           */
        waMCR,
        regMCR | MCR_TCRTLR);
    lldCfgModeSet(                                                              /* Switch to register configuration mode B to access the EFR*/
        io,                                                                     /* register                                                 */
        LLD_CFG_MODE_B);
    lldRegWr(                                                                   /* Load the new FIFO triggers (2/3)                         */
        io,
        wbTLR,
        FIFO_RX_LVL | FIFO_TX_LVL);
    lldCfgModeSet(                                                              /* Switch to register configuration mode A to access the MCR*/
        io,                                                                     /* register                                                 */
        LLD_CFG_MODE_A);
    lldRegWr(                                                                   /* Load the new FIFO triggers (3/3) and the new DMA mode    */
        io,                                                                     /* (2/2)                                                    */
        wSCR,
        0);
    lldRegWr(                                                                   /* Load the new FIFO triggers (1/3) and the new DMA mode    */
        io,                                                                     /* (1/2)                                                    */
        waFCR,
        FCR_RX_FIFO_CLEAR | FCR_TX_FIFO_CLEAR | FCR_FIFO_EN);                   /* BUG NOTE: HW does not listen these FIFO granularity      */
#if (2u == CFG_DMA_MODE)
    lldUARTDMAStateSet(
        io,
        LLD_DMA_MODE_TX_AND_RX);
#endif
    lldCfgModeSet(                                                              /* Switch to register configuration mode B to access the EFR*/
        io,                                                                     /* register                                                 */
        LLD_CFG_MODE_B);
    lldRegWr(                                                                   /* Restore EFR register                                     */
        io,
        wbEFR,
        regEFR);
    lldCfgModeSet(                                                              /* Switch to register configuration mode A to access the MCR*/
        io,                                                                     /* register                                                 */
        LLD_CFG_MODE_A);
    lldRegWr(                                                                   /* Restore MCR register                                     */
        io,
        waMCR,
        regMCR);
    lldRegWr(                                                                   /* Restore LCR register                                     */
        io,
        LCR,
        regLCR);
}

void lldUARTDMAStateSet(
    volatile uint8_t *  io,
    enum lldDMAMode     mode) {

    uint16_t            reg;

    reg = lldRegRd(
        io,
        SCR);
    reg |= SCR_DMAMODECTL;
    lldRegWr(
        io,
        wSCR,
        reg);
    reg &= ~SCR_DMAMODE2_Mask;
    reg |= mode & SCR_DMAMODE2_Mask;
    lldRegWr(
        io,
        wSCR,
        reg);
}

void lldUARTTxDMAThresholdCtrl(
    volatile uint8_t *  io,
    enum lldDMATxThreshold threshold) {

    uint16_t            val;

    val = lldRegRd(
        io,
        MDR3);
    val &= ~MDR3_SET_DMA_TX_THRESHOLD;
    val |= threshold & MDR3_SET_DMA_TX_THRESHOLD;
    lldRegWr(
        io,
        wMDR3,
        val);
}

void lldUARTDMATxThresholdVal(
    volatile uint8_t *  io,
    uint32_t            val) {

    uint16_t            regval;

    regval = lldRegRd(
        io,
        TXDMA);
    regval &= ~TXDMA_TX_DMA_THRESHOLD_Mask;
    regval |= val & TXDMA_TX_DMA_THRESHOLD_Mask;
    lldRegWr(
        io,
        wTXDMA,
        regval);
}

int lldDMAFIFOInit(
    volatile uint8_t *  io) {

    /*
     * TODO: napisati ovo
     */

    return (0);
}

void lldProtocolPrint(
    const struct xUartProto * proto) {

    char *              parity;
    char *              stopBits;
    char *              dataBits;

    switch (proto->parity) {
        case XUART_PARITY_NONE : {
            parity = "none";
            break;
        }
        case XUART_PARITY_EVEN : {
            parity = "even";
            break;
        }
        case XUART_PARITY_ODD : {
            parity = "odd";
            break;
        }
        default : {
            parity = "unknown";
            break;
        }
    }

    switch (proto->stopBits) {
        case XUART_STOP_1 : {
            stopBits = "1";
            break;
        }
        case XUART_STOP_1n5 : {
            stopBits = "1n5";
            break;
        }
        case XUART_STOP_2 : {
            stopBits = "2";
            break;
        }
        default : {
            stopBits = "unknown";
            break;
        }
    }

    switch (proto->dataBits) {
        case XUART_DATA_8 : {
            dataBits = "8";
            break;
        }
        case XUART_DATA_5 : {
            dataBits = "5";
            break;
        }
        default : {
            dataBits = "unknown";
            break;
        }
    }
    LOG_INFO("protocol: %d, %s %s %s",
        proto->baud,
        dataBits,
        parity,
        stopBits);
}

/*
 * TODO: Break this function into a set of smaller functions
 */
int32_t lldProtocolSet(
    volatile uint8_t *  io,
    const struct xUartProto * protocol) {

    uint16_t            tmp;
    uint16_t            arg;
    uint16_t            retval;
    uint16_t            regEFR;

    retval = 0;
    lldModeSet(                                                                 /* Disable UART to access DLL and DLH registers             */
        io,
        LLD_MODE_DISABLE);
    lldCfgModeSet(                                                              /* Switch to register config mode B to access EFR register  */
        io,
        LLD_CFG_MODE_B);
    regEFR = lldRegSetBits(                                                     /* Enable access to IER[7:4] bit field                      */
        io,
        bEFR,
        EFR_ENHANCEDEN);
    lldRegWr(                                                                   /* Switch to normal config mode                             */
        io,
        LCR,
        LLD_CFG_MODE_NORM);
    lldRegWr(
        io,
        IER,
        0x0000U);
    tmp = portDIVdataGet(                                                       /* Get the new divisor value                                */
        protocol->baud);

    if (0 > tmp) {
        LOG_INFO("protocol: invalid baud rate");
        retval = -EINVAL;
    } else {
        lldCfgModeSet(                                                          /* Switch to config mode B to access DLH and DLL registers  */
            io,
            LLD_CFG_MODE_B);
        lldRegWr(
            io,
            wbDLL,
            U16_LOW_BYTE(tmp));
        lldRegWr(
            io,
            wbDLH,
            U16_HIGH_BYTE(tmp));
        lldCfgModeSet(
            io,
            LLD_CFG_MODE_NORM);
    }
    lldIntEnable(
        io,
        LLD_INT_RX);
    lldCfgModeSet(
        io,
        LLD_CFG_MODE_B);
    lldRegWr(
        io,
        wbEFR,
        regEFR);
    lldRegResetBits(
        io,
        LCR,
        LCR_BREAK_EN | LCR_DIV_EN);

    switch (protocol->parity) {
        case XUART_PARITY_NONE : {
            arg = 0U;
            break;
        }
        case XUART_PARITY_EVEN : {
            arg = LCR_PARITY_EN | LCR_PARITY_TYPE1;
            break;
        }
        case XUART_PARITY_ODD : {
            arg = LCR_PARITY_EN;
            break;
        }
        default : {                                                             /* Use default value and report warning                     */
            LOG_INFO("protocol: invalid parity");
            arg = 0;
            retval = -EINVAL;
            break;
        }
    }
    lldRegWrBits(
        io,
        LCR,
        LCR_PARITY_EN | LCR_PARITY_TYPE1 | LCR_PARITY_TYPE2,
        arg);

    switch (protocol->dataBits) {
        case XUART_DATA_8 : {
            arg = LCR_CHAR_LENGTH_8;
            break;
        }
        default : {                                                             /* Use default value and report warning                     */
            LOG_INFO("protocol: invalid data bits");
            arg = LCR_CHAR_LENGTH_8;
            retval = -EINVAL;
            break;
        }
    }
    lldRegWrBits(
        io,
        LCR,
        LCR_CHAR_LENGTH_Mask,
        arg);

    switch (protocol->stopBits) {
        case XUART_STOP_1 : {
            arg = 0U;
            break;
        }
        case XUART_STOP_1n5 : {

            if (XUART_DATA_5 == protocol->dataBits) {
                arg = LCR_NB_STOP;
            } else {
                arg = 0U;
            }
            break;
        }
        case XUART_STOP_2 : {
            arg = LCR_NB_STOP;
            break;
        }
        default : {                                                             /* Use default value and report warning                     */
            LOG_INFO("protocol: invalid stop bits");
            arg = 0U;
            retval = -EINVAL;
            break;
        }
    }
    lldRegWrBits(
        io,
        LCR,
        LCR_NB_STOP,
        arg);
    tmp = portModeGet(
        protocol->baud);

    if (0 > tmp) {
        LOG_INFO("protocol: failed to find mode, err: %d", tmp);

        return (tmp);
    }
    lldModeSet(
        io,
        tmp);

    return (0);
}

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of x-16c750_regs.c
 ******************************************************************************/
