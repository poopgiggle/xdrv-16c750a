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

#include "x-16c750_ioctl.h"
#include "x-16c750_lld.h"
#include "port.h"

/*=========================================================  LOCAL MACRO's  ==*/

#define U16_LOW_BYTE(val)                                                       \
    ((val) & 0x00FFU)

#define U16_HIGH_BYTE(val)                                                      \
    ((val) >> 8)

/*======================================================  LOCAL DATA TYPES  ==*/

const struct xUartProto gDefProtocol = {
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
    volatile u8 *       ioRemap,
    enum hwReg          reg,
    u16                 bitmask,
    u16                 bits) {

    u16                 tmp;

    tmp = lldRegRd(ioRemap, reg);
    tmp &= ~bitmask;
    tmp |= (bits & bitmask);
    lldRegWr(ioRemap, reg, tmp);
}

u16 lldRegRdBits(
    volatile u8 *       ioRemap,
    enum hwReg          reg,
    u16                 bitmask) {

    u16                 tmp;

    tmp = lldRegRd(ioRemap, reg);
    tmp &= ~bitmask;

    return (tmp);
}

void lldCfgModeSet(
    volatile u8 *       ioRemap,
    enum lldCfgMode     cfgMode) {

    lldRegWr(
        ioRemap,
        LCR,
        cfgMode);
}

void lldModeSet(
    volatile u8 *       ioRemap,
    enum lldMode        mode) {

    lldRegWrBits(
        ioRemap,
        MDR1,
        MDR1_MODESELECT_Mask,
        mode);
}

u16 lldModeGet(
    volatile u8 *       ioRemap) {

    u16                 tmp;

    tmp = lldRegRdBits(
        ioRemap,
        MDR1,
        MDR1_MODESELECT_Mask);

    return (tmp);
}

void lldIntEnable(
    volatile u8 *       ioRemap,
    enum lldIntNum         intNum) {

    u16                 tmp;

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
        }
    }
    lldRegSetBits(
        ioRemap,
        IER,
        tmp);
}

void lldIntDisable(
    volatile u8 *       ioRemap,
    enum lldIntNum         intNum) {

    u16                 tmp;

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
        }
    }
    lldRegResetBits(
        ioRemap,
        IER,
        tmp);
}

u16 lldIntGet(
    volatile u8 *       ioRemap) {

    u16                 tmp;

    tmp = lldRegRd(
        ioRemap,
        IIR);
    tmp &= IIR_IT_TYPE_Mask;

    return (tmp);
}

u16 lldIsIntPending(
    volatile u8 *       ioRemap) {

    u16                 tmp;

    tmp = lldRegRd(
        ioRemap,
        IIR);

    return (tmp & IIR_IT_PENDING);
}

void lldEnhanced(
    volatile u8 *       ioRemap,
    enum lldState       state) {

    u16                 regLCR;
    u16                 tmp;

    regLCR = lldRegRd(ioRemap, LCR);
    lldRegWr(ioRemap, LCR, LLD_CFG_MODE_B);
    lldCfgModeSet(
        ioRemap,
        LLD_CFG_MODE_B);
    tmp = lldRegRd(
        ioRemap,
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
        ioRemap,
        wbEFR,
        tmp);
    lldRegWr(
        ioRemap,
        LCR,
        regLCR);
}

int lldSoftReset(
    volatile u8 *       ioRemap) {

    int                 retval;
    u32                 cnt;

    lldRegWr(
        ioRemap,
        SYSC,
        SYSC_SOFTRESET);
    cnt = 1000U;

    while ((0 == (SYSS_RESETDONE & lldRegRd(ioRemap, SYSS))) && (0U != cnt)) {
        --cnt;
    }

    if (0U != cnt) {
        retval = RETVAL_SUCCESS;
    } else {
        retval = RETVAL_FAILURE;
    }

    return (retval);
}

void lldInit(
    volatile u8 *       io) {

    struct {
        u16             EFR;
        u16             MCR;
    }                   reg;
    int                 retval;

    /* Reset UART */
    retval = lldSoftReset(io);
    LOG_WARN_IF(RETVAL_FAILURE == retval, "failed to reset UART");

    /* Switch off DLL and DLH to access UART registers */
    lldCfgModeSet(io, LLD_CFG_MODE_A);
    lldRegWr(io, waDLL, 0U);
    lldRegWr(io, waDLH, 0U);
    lldCfgModeSet(io, LLD_CFG_MODE_NORM);
    lldCfgModeSet(io,LLD_CFG_MODE_B);
    reg.EFR = lldRegRd(io, bEFR);
    lldRegWr(io, wbEFR, reg.EFR | EFR_ENHANCEDEN);
    lldCfgModeSet(io, LLD_CFG_MODE_A);
    reg.MCR = lldRegRd(io, aMCR);
    lldRegWr(io, waMCR, reg.MCR | MCR_TCRTLR);

    /* FIFO reset and enable */
    lldRegWr(io, waFCR, FCR_RX_FIFO_CLEAR | FCR_TX_FIFO_CLEAR);
    lldRegWr(io, waFCR, FCR_RX_FIFO_TRIG_8 | FCR_TX_FIFO_TRIG_8 | FCR_FIFO_EN);
    lldCfgModeSet(io, LLD_CFG_MODE_B);
    lldRegWr(io, wbSCR, 0U);
    lldRegWr(io, wbEFR, reg.EFR);
    lldCfgModeSet(io, LLD_CFG_MODE_A);
    lldRegWr(io, waMCR, reg.MCR);

    /* Protocol */

}

void lldFIFOInit(
    volatile u8 *       ioRemap) {

    u16                 tmp;
    u16                 regLCR;
    u16                 regEFR;
    u16                 regMCR;
    u16                 regDLL;
    u16                 regDLH;

    regLCR = lldRegRd(                                                          /* Switch to register configuration mode B to access the    */
        ioRemap,                                                                /* EFR register                                             */
        LCR);
    lldCfgModeSet(
        ioRemap,
        LLD_CFG_MODE_B);
    regEFR = lldRegSetBits(                                                     /* Enable register submode TCR_TLR to access the TLR        */
        ioRemap,                                                                /* register (1/2)                                           */
        bEFR,
        EFR_ENHANCEDEN);
    lldCfgModeSet(                                                              /* Switch to register configuration mode A to access the MCR*/
        ioRemap,                                                                /* register                                                 */
        LLD_CFG_MODE_A);
    regDLL = lldRegRd(ioRemap, aDLL);
    lldRegWr(ioRemap, waDLL, 0);
    regDLH = lldRegRd(ioRemap, aDLH);
    lldRegWr(ioRemap, waDLH, 0);
    regMCR = lldRegSetBits(                                                     /* Enable register submode TCR_TLR to access the TLR        */
        ioRemap,                                                                /* register (2/2)                                           */
        aMCR,
        MCR_TCRTLR);
    (void)lldRegResetBits(                                                      /* Load the new FIFO triggers (3/3) and the new DMA mode    */
        ioRemap,                                                                /* (2/2)                                                    */
        SCR,
        SCR_RXTRIGGRANU1 | SCR_TXTRIGGRANU1 | SCR_DMAMODE2_Mask |
            SCR_DMAMODECTL);
    lldRegWr(                                                                   /* Load the new FIFO triggers (1/3) and the new DMA mode    */
        ioRemap,                                                                /* (1/2)                                                    */
        waFCR,
        FCR_RX_FIFO_TRIG_56 | FCR_TX_FIFO_TRIG_56 |
            FCR_TX_FIFO_CLEAR | FCR_RX_FIFO_CLEAR | FCR_FIFO_EN);
    lldCfgModeSet(                                                              /* Switch to register configuration mode B to access the EFR*/
        ioRemap,                                                                /* register                                                 */
        LLD_CFG_MODE_B);
    lldRegWr(                                                                   /* Load the new FIFO triggers (2/3)                         */
        ioRemap,
        wbTLR,
        TLR_RX_FIFO_TRIG_DMA_0 | TLR_TX_FIFO_TRIG_DMA_0);
    tmp = regEFR & EFR_ENHANCEDEN;                                              /* Restore EFR<4> ENHANCED_EN bit                           */
    tmp |= lldRegRd(ioRemap, bEFR) & ~EFR_ENHANCEDEN;
    lldRegWr(
        ioRemap,
        bEFR,
        tmp);
    lldCfgModeSet(                                                              /* Switch to register configuration mode A to access the MCR*/
        ioRemap,                                                                /* register                                                 */
        LLD_CFG_MODE_A);
    tmp = regMCR & MCR_TCRTLR;                                                  /* Restore MCR<6> TCRTLR bit                                */
    tmp |= lldRegRd(ioRemap, aMCR) & ~MCR_TCRTLR;
    lldRegWr(
        ioRemap,
        aMCR,
        tmp);
    lldRegWr(                                                                   /* Restore LCR                                              */
        ioRemap,
        LCR,
        regLCR);
}

int lldDMAFIFOSetup(
    volatile u8 *       ioRemap) {

    u16                 tmp;
    u16                 regLCR;
    u16                 regEFR;
    u16                 regMCR;

    regLCR = lldRegRd(                                                          /* Switch to register configuration mode B to access the    */
        ioRemap,                                                                /* EFR register                                             */
        LCR);
    lldCfgModeSet(
        ioRemap,
        LLD_CFG_MODE_B);
    regEFR = lldRegSetBits(                                                     /* Enable register submode TCR_TLR to access the TLR        */
        ioRemap,                                                                /* register (1/2)                                           */
        bEFR,
        EFR_ENHANCEDEN);
    lldCfgModeSet(                                                              /* Switch to register configuration mode A to access the MCR*/
        ioRemap,                                                                /* register                                                 */
        LLD_CFG_MODE_A);
    regMCR = lldRegSetBits(                                                     /* Enable register submode TCR_TLR to access the TLR        */
        ioRemap,                                                                /* register (2/2)                                           */
        aMCR,
        MCR_TCRTLR);
    lldRegWr(                                                                   /* Load the new FIFO triggers (1/3) and the new DMA mode    */
        ioRemap,                                                                /* (1/2)                                                    */
        waFCR,
        FCR_RX_FIFO_TRIG_56 | FCR_TX_FIFO_TRIG_56 | FCR_DMA_MODE |
            FCR_TX_FIFO_CLEAR | FCR_RX_FIFO_CLEAR | FCR_FIFO_EN);
    lldCfgModeSet(                                                              /* Switch to register configuration mode B to access the EFR*/
        ioRemap,                                                                /* register                                                 */
        LLD_CFG_MODE_B);
    lldRegWr(                                                                   /* Load the new FIFO triggers (2/3)                         */
        ioRemap,
        wbTLR,
        TLR_RX_FIFO_TRIG_DMA_0 | TLR_TX_FIFO_TRIG_DMA_0);
    (void)lldRegResetBits(                                                      /* Load the new FIFO triggers (3/3) and the new DMA mode    */
        ioRemap,                                                                /* (2/2)                                                    */
        SCR,
        SCR_RXTRIGGRANU1 | SCR_TXTRIGGRANU1 | SCR_DMAMODE2_Mask |
            SCR_DMAMODECTL);
    tmp = regEFR & EFR_ENHANCEDEN;                                              /* Restore EFR<4> ENHANCED_EN bit                           */
    tmp |= lldRegRd(ioRemap, bEFR) & ~EFR_ENHANCEDEN;
    lldRegWr(
        ioRemap,
        bEFR,
        tmp);
    lldCfgModeSet(                                                              /* Switch to register configuration mode A to access the MCR*/
        ioRemap,                                                                /* register                                                 */
        LLD_CFG_MODE_A);
    tmp = regMCR & MCR_TCRTLR;                                                  /* Restore MCR<6> TCRTLR bit                                */
    tmp |= lldRegRd(ioRemap, aMCR) & ~MCR_TCRTLR;
    lldRegWr(
        ioRemap,
        aMCR,
        tmp);
    lldRegWr(                                                                   /* Restore LCR                                              */
        ioRemap,
        LCR,
        regLCR);

    return (RETVAL_SUCCESS);
}

void lldProtocolPrint(
    const struct xUartProto * protocol) {

    char *              parity;
    char *              stopBits;
    char *              dataBits;

    switch (protocol->parity) {
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

    switch (protocol->stopBits) {
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

    switch (protocol->dataBits) {
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
        protocol->baud,
        dataBits,
        parity,
        stopBits);
}

/*
 * TODO: Break this function into a set of smaller functions
 */
int lldProtocolSet(
    volatile u8 *       ioRemap,
    const struct xUartProto * protocol) {

    u16                 tmp;
    u16                 arg;
    u16                 retval;
    u16                 regEFR;

    retval = RETVAL_SUCCESS;
    lldModeSet(                                                                 /* Disable UART to access DLL and DLH registers             */
        ioRemap,
        LLD_MODE_DISABLE);
    lldCfgModeSet(                                                              /* Switch to register config mode B to access EFR register  */
        ioRemap,
        LLD_CFG_MODE_B);
    regEFR = lldRegSetBits(                                                     /* Enable access to IER[7:4] bit field                      */
        ioRemap,
        bEFR,
        EFR_ENHANCEDEN);
    lldRegWr(                                                                   /* Switch to normal config mode                             */
        ioRemap,
        LCR,
        LLD_CFG_MODE_NORM);
    lldRegWr(
        ioRemap,
        IER,
        0x0000U);
    tmp = portDIVdataGet(                                                       /* Get the new divisor value                                */
        protocol->baud);

    if (RETVAL_FAILURE == tmp) {
        LOG_DBG("protocol: invalid baud rate");
        retval = -EINVAL;
    } else {
        lldCfgModeSet(                                                              /* Switch to config mode B to access DLH and DLL registers  */
            ioRemap,
            LLD_CFG_MODE_B);
        lldRegWr(
            ioRemap,
            wbDLL,
            U16_LOW_BYTE(tmp));
        lldRegWr(
            ioRemap,
            wbDLH,
            U16_HIGH_BYTE(tmp));
        lldCfgModeSet(
            ioRemap,
            LLD_CFG_MODE_NORM);
    }

    lldIntEnable(
        ioRemap,
        LLD_INT_RX);
    lldCfgModeSet(
        ioRemap,
        LLD_CFG_MODE_B);
    tmp = (lldRegRd(ioRemap, bEFR) & ~EFR_ENHANCEDEN);
    tmp |= regEFR & EFR_ENHANCEDEN;
    lldRegWr(
        ioRemap,
        wbEFR,
        tmp);
    lldRegResetBits(
        ioRemap,
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
            arg = 0;
            LOG_DBG("protocol: invalid parity");
            retval = -EINVAL;
            break;
        }
    }
    lldRegWrBits(
        ioRemap,
        LCR,
        LCR_PARITY_EN | LCR_PARITY_TYPE1 | LCR_PARITY_TYPE2,
        arg);

    switch (protocol->dataBits) {
        case XUART_DATA_8 : {
            arg = LCR_CHAR_LENGTH_8;
            break;
        }

        default : {                                                             /* Use default value and report warning                     */
            arg = LCR_CHAR_LENGTH_8;
            LOG_DBG("protocol: invalid data bits");
            retval   = -EINVAL;
            break;
        }
    }
    lldRegWrBits(
        ioRemap,
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
            arg = 0U;
            LOG_DBG("protocol: invalid stop bits");
            retval   = -EINVAL;
            break;
        }
    }
    lldRegWrBits(
        ioRemap,
        LCR,
        LCR_NB_STOP,
        arg);

    tmp = portModeGet(
        protocol->baud);

    if (RETVAL_FAILURE != tmp) {
        lldModeSet(
            ioRemap,
            tmp);
    }
    lldProtocolPrint(
        protocol);

    return (retval);
}

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of x-16c750_regs.c
 ******************************************************************************/
