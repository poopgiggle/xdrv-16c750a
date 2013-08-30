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

#define U16_LOW_BYTE(val)                                                       \
    ((val) & 0x00FFU)

#define U16_HIGH_BYTE(val)                                                      \
    ((val) >> 8)

/*======================================================  LOCAL DATA TYPES  ==*/
/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/
/*=======================================================  LOCAL VARIABLES  ==*/

const struct rtser_config gDefProtocol = {
    .config_mask        = RTSER_SET_BAUD | RTSER_SET_PARITY | RTSER_SET_DATA_BITS |
                          RTSER_SET_STOP_BITS,
    .baud_rate          = 115200,
    .parity             = RTSER_NO_PARITY,
    .data_bits          = RTSER_8_BITS,
    .stop_bits          = RTSER_1_STOPB,
};

/*======================================================  GLOBAL VARIABLES  ==*/
/*============================================  LOCAL FUNCTION DEFINITIONS  ==*/
/*===================================  GLOBAL PRIVATE FUNCTION DEFINITIONS  ==*/
/*====================================  GLOBAL PUBLIC FUNCTION DEFINITIONS  ==*/

void lldRegWrBits(
    u8 *                ioRemap,
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
    u8 *                ioRemap,
    enum hwReg          reg,
    u16                 bitmask) {

    u16                 tmp;

    tmp = lldRegRd(ioRemap, reg);
    tmp &= ~bitmask;

    return (tmp);
}

void lldCfgModeSet(
    u8 *                ioRemap,
    enum lldCfgMode     cfgMode) {

    lldRegWr(
        ioRemap,
        LCR,
        cfgMode);
}

void lldModeSet(
    u8 *                ioRemap,
    enum lldMode        mode) {

    lldRegWrBits(
        ioRemap,
        MDR1,
        MDR1_MODESELECT_Mask,
        mode);
}

u16 lldModeGet(
    u8 *                ioRemap) {

    u16                 tmp;

    tmp = lldRegRdBits(
        ioRemap,
        MDR1,
        MDR1_MODESELECT_Mask);

    return (tmp);
}

void lldIntEnable(
    u8 *                ioRemap,
    enum lldINT         intNum) {

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
    u8 *                ioRemap,
    enum lldINT         intNum) {

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

        default : {
            tmp = 0U;
        }
    }
    lldRegResetBits(
        ioRemap,
        IER,
        tmp);
}

enum lldINT lldIntGet(
    u8 *                ioRemap) {

    u16                 tmp;

    tmp = lldRegRd(
        ioRemap,
        IIR);

    switch (tmp) {
        case 0xc0: {

            return (LLD_INT_RX_TIMEOUT);
        }

        case 0x04: {

            return (LLD_INT_RX);
        }

        case 0x02: {

            return (LLD_INT_TX);
        }

        default: {

            return (LLD_INT_NONE);
        }
    }
}

int lldSoftReset(
    u8 *                ioRemap) {

    lldRegWr(
        ioRemap,
        SYSC,
        SYSC_SOFTRESET);

    while (0 == (SYSS_RESETDONE & lldRegRd(ioRemap, SYSS)));

    return (RETVAL_SUCCESS);
}

int lldFIFOSetup(
    u8 *                ioRemap) {

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
        FCR_RX_FIFO_TRIG_56 | FCR_TX_FIFO_TRIG_56 |
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

int lldDMAFIFOSetup(
    u8 *                ioRemap) {

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
    const struct rtser_config * config) {

#define DEF_STRING_SIZE                 10U

    char *              parity;
    char *              stopBits;
    char *              dataBits;

    switch (config->parity) {
        case RTSER_NO_PARITY : {
            parity = "none";
            break;
        }

        case RTSER_ODD_PARITY : {
            parity = "odd";
            break;
        }

        case RTSER_EVEN_PARITY : {
            parity = "even";
            break;
        }

        default : {
            parity = "unknown";
            break;
        }
    }

    switch (config->stop_bits) {
        case RTSER_1_STOPB : {
            stopBits = "1";
            break;
        }

        case RTSER_2_STOPB : {
            stopBits = "2";
            break;
        }

        default : {
            stopBits = "unknown";
            break;
        }
    }

    switch (config->data_bits) {
        case RTSER_8_BITS : {
            dataBits = "8";
            break;
        }

        default : {
            dataBits = "unknown";
            break;
        }
    }
    LOG_INFO("configuration: %d, %s %s %s",
        config->baud_rate,
        dataBits,
        parity,
        stopBits);
}

/*
 * TODO: Break this functin into a set of smaller functions
 */
int lldProtocolSet(
    u8 *                ioRemap,
    const struct rtser_config * config) {

    u16                 tmp;
    u16                 arg;
    u16                 retval;
    u16                 regEFR;
    u16                 oldMode;

    retval = RETVAL_SUCCESS;
    oldMode = lldModeGet(
        ioRemap);
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

    if (0 != (RTSER_SET_BAUD & config->config_mask)) {
        tmp = portDIVdataGet(                                                       /* Get the new divisor value                                */
            config->baud_rate);

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
    }

    /*
     * TODO: set interrupts
     */
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

    if (0 != (RTSER_SET_PARITY & config->config_mask)) {

        switch (config->parity) {
            case RTSER_EVEN_PARITY : {
                arg = LCR_PARITY_EN | LCR_PARITY_TYPE1;
                break;
            }

            case RTSER_ODD_PARITY : {
                arg = LCR_PARITY_EN;
                break;
            }

            case RTSER_NO_PARITY : {
                arg = 0U;
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
    }

    if (0 != (RTSER_SET_DATA_BITS & config->config_mask)) {

        switch (config->data_bits) {
            case RTSER_8_BITS : {
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
    }

    if (0 != (RTSER_SET_STOP_BITS & config->config_mask)) {
        switch (config->stop_bits) {
            case RTSER_2_STOPB : {
                arg = LCR_NB_STOP;
                break;
            }

            case RTSER_1_STOPB : {
                arg = 0U;
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
    }

    if (0 != (RTSER_SET_BAUD & config->config_mask)) {
        tmp = portModeGet(                                                          /* NOTE: return should not be checked here because DIV func */
            config->baud_rate);                                                     /* already did it before                                    */
        lldModeSet(
            ioRemap,
            tmp);
    } else {
        lldModeSet(
            ioRemap,
            oldMode);
    }
    lldProtocolPrint(
        config);

    return (retval);
}

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of x-16c750_regs.c
 ******************************************************************************/
