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

#include "x-16c750_lld.h"
#include "port.h"

/*=========================================================  LOCAL MACRO's  ==*/
/*======================================================  LOCAL DATA TYPES  ==*/
/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/
/*=======================================================  LOCAL VARIABLES  ==*/
/*======================================================  GLOBAL VARIABLES  ==*/
/*============================================  LOCAL FUNCTION DEFINITIONS  ==*/
/*===================================  GLOBAL PRIVATE FUNCTION DEFINITIONS  ==*/
/*====================================  GLOBAL PUBLIC FUNCTION DEFINITIONS  ==*/

int lldUartSoftReset(
    u8 *                ioRemap) {

    lldRegWr(
        ioRemap,
        SYSC,
        SYSC_SOFTRESET);

    while (0 == (SYSS_RESETDONE & lldRegRd(ioRemap, SYSS)));

    return (RETVAL_SUCCESS);
}

int lldUartDMAFIFOSetup(
    struct uartCtx *    uartCtx) {

    int                 retval;
    u16                 tmp;
    u16                 regLCR;
    u16                 regEFR;
    u16                 regMCR;

    regLCR = lldRegRd(                                                          /* Switch to register configuration mode B to access the    */
        uartCtx->ioRemap,                                                       /* EFR register                                             */
        LCR);
    UART_CFG_MODE_SET(uartCtx->ioRemap, UART_CFG_MODE_B);
    regEFR = lldRegSetBits(                                                     /* Enable register submode TCR_TLR to access the TLR        */
        uartCtx->ioRemap,                                                       /* register (1/2)                                           */
        bEFR,
        EFR_ENHANCEDEN);
    UART_CFG_MODE_SET(uartCtx->ioRemap, UART_CFG_MODE_A);                       /* Switch to register configuration mode A to access the MCR*/
                                                                                /* register                                                 */
    regMCR = lldRegSetBits(                                                     /* Enable register submode TCR_TLR to access the TLR        */
        uartCtx->ioRemap,                                                       /* register (2/2)                                           */
        aMCR,
        MCR_TCRTLR);
    lldRegWr(                                                                   /* Load the new FIFO triggers (1/3) and the new DMA mode    */
        uartCtx->ioRemap,                                                       /* (1/2)                                                    */
        waFCR,
        FCR_RX_FIFO_TRIG_56 | FCR_TX_FIFO_TRIG_56 | FCR_DMA_MODE |
            FCR_TX_FIFO_CLEAR | FCR_RX_FIFO_CLEAR | FCR_FIFO_EN);
    UART_CFG_MODE_SET(uartCtx->ioRemap, UART_CFG_MODE_B);                       /* Switch to register configuration mode B to access the EFR*/
                                                                                /* register   */
    lldRegWr(                                                                   /* Load the new FIFO triggers (2/3)                         */
        uartCtx->ioRemap,
        wbTLR,
        TLR_RX_FIFO_TRIG_DMA_0 | TLR_TX_FIFO_TRIG_DMA_0);
    (void)lldRegResetBits(                                                      /* Load the new FIFO triggers (3/3) and the new DMA mode    */
        uartCtx->ioRemap,                                                       /* (2/2)                                                    */
        SCR,
        SCR_RXTRIGGRANU1 | SCR_TXTRIGGRANU1 | SCR_DMAMODE2_Mask |
            SCR_DMAMODECTL);
    tmp = regEFR & EFR_ENHANCEDEN;                                              /* Restore EFR<4> ENHANCED_EN bit                           */
    tmp |= lldRegRd(uartCtx->ioRemap, bEFR) & ~EFR_ENHANCEDEN;
    lldRegWr(
        uartCtx->ioRemap,
        bEFR,
        tmp);
    UART_CFG_MODE_SET(uartCtx->ioRemap, UART_CFG_MODE_A);                       /* Switch to register configuration mode A to access the MCR*/
                                                                                /* register                                                 */
    tmp = regMCR & MCR_TCRTLR;                                                  /* Restore MCR<6> TCRTLR bit                                */
    tmp |= lldRegRd(uartCtx->ioRemap, aMCR) & ~MCR_TCRTLR;
    lldRegWr(
        uartCtx->ioRemap,
        aMCR,
        tmp);
    lldRegWr(                                                                   /* Restore LCR                                              */
        uartCtx->ioRemap,
        LCR,
        regLCR);

    retval = portDMAInit(
        uartCtx);

    return (retval);
}

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of x-16c750_regs.c
 ******************************************************************************/
