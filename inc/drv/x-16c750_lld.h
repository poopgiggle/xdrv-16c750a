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
 * @author  	Nenad Radulovic
 * @brief       Low Level Driver (LLD) for 16C750 UART hardware
 *********************************************************************//** @{ */

#if !defined(X_16C750_LLD_H_)
#define X_16C750_LLD_H_

/*=========================================================  INCLUDE FILES  ==*/

#include "x-16c750.h"
#include "x-16c750_cfg.h"
#include "log.h"

/*===============================================================  MACRO's  ==*/

/*------------------------------------------------------------------------*//**
 * @name        16C750 Register definitions
 * @{ *//*--------------------------------------------------------------------*/

/**@brief       16C750 Register address offsets
 * @details     From datasheet: SPRUH73H - October 2011 - Rev April 2013
 */
/*
 *        | LCR[7] = 0    | LCR != 0xbf   | LCR == 0xbf   | address offset
 *          RD      WR      RD      WR      RD      WR      ADDR
 */
#define REGS_TABLE(entry)                                                       \
    entry(  RHR,    THR,    DLL,    DLL,    DLL,    DLL,    0x00)               \
    entry(  IER,    IER,    DLH,    DLH,    DLH,    DLH,    0x04)               \
    entry(  IIR,    FCR,    IIR,    FCR,    EFR,    EFR,    0x08)               \
    entry(  LCR,    LCR,    LCR,    LCR,    LCR,    LCR,    0x0c)               \
    entry(  MCR,    MCR,    MCR,    MCR,    XON1,   XON1,   0x10)               \
    entry(  LSR,    na1,    LSR,    na1,    XON2,   XON2,   0x14)               \
    entry(  MSR,    TCR,    MSR,    TCR,    XOFF1,  XOFF1,  0x18)               \
    entry(  SPR,    SPR,    SPR,    SPR,    XOFF2,  XOFF2,  0x1c)               \
    entry(  MDR1,   MDR1,   MDR1,   MDR1,   MDR1,   MDR1,   0x20)               \
    entry(  MDR2,   MDR2,   MDR2,   MDR2,   MDR2,   MDR2,   0x24)               \
    entry(  SFLSR,  TXFLL,  SFLSR,  TXFLL,  SFLSR,  TXFLL,  0x28)               \
    entry(  RESUME, TXFLH,  RESUME, TXFLH,  RESUME, TXFLH,  0x2c)               \
    entry(  SFREGL, RXFLL,  SFREGL, RXFLL,  SFREGL, RXFLL,  0x30)               \
    entry(  SFREGH, RXFLH,  SFREGH, RXFLH,  SFREGH, RXFLH,  0x34)               \
    entry(  BLR,    BLR,    UASR,   na2,    UASR,   na2,    0x38)               \
    entry(  ACREG,  ACREG,  na3,    na3,    na3,    na3,    0x3c)               \
    entry(  SCR,    SCR,    SCR,    SCR,    SCR,    SCR,    0x40)               \
    entry(  SSR,    SSR,    SSR,    SSR,    SSR,    SSR,    0x44)               \
    entry(  EBLR,   EBLR,   na4,    na4,    na4,    na4,    0x48)               \
    entry(  MVR,    na5,    MVR,    na5,    MVR,    na5,    0x50)               \
    entry(  SYSC,   SYSC,   SYSC,   SYSC,   SYSC,   SYSC,   0x54)               \
    entry(  SYSS,   na6,    SYSS,   na6,    SYSS,   na6,    0x58)               \
    entry(  WER,    WER,    WER,    WER,    WER,    WER,    0x5c)               \
    entry(  CFPS,   CFPS,   CFPS,   CFPS,   CFPS,   CFPS,   0x60)               \
    entry(  RXFIFO_LVL, RXFIFO_LVL, RXFIFO_LVL, RXFIFO_LVL, RXFIFO_LVL, RXFIFO_LVL, 0x64)   \
    entry(  TXFIFO_LVL, TXFIFO_LVL, TXFIFO_LVL, TXFIFO_LVL, TXFIFO_LVL, TXFIFO_LVL, 0x68)   \
    entry(  IER2,   IER2,   IER2,   IER2,   IER2,   IER2,   0x6c)               \
    entry(  ISR2,   ISR2,   ISR2,   ISR2,   ISR2,   ISR2,   0x70)               \
    entry(  FREQ_SEL, FREQ_SEL, FREQ_SEL, FREQ_SEL, FREQ_SEL, FREQ_SEL, 0x74)   \
    entry(  MDR3,   MDR3,   MDR3,   MDR3,   MDR3,   MDR3,   0x80)               \
    entry(  TXDMA,  TXDMA,  TXDMA,  TXDMA,  TXDMA,  TXDMA,  0x84)

/**@brief       Create registers from table
 */
#define REGS_EXPAND_AS_ENUM(rd0_reg, wr0_reg, rdN_reg, wrN_reg, rd_reg, wr_reg, addr)       \
    rd0_reg = addr,                                                             \
    w##wr0_reg = addr,                                                          \
    a##rdN_reg = addr,                                                          \
    wa##wrN_reg = addr,                                                         \
    b##rd_reg = addr,                                                           \
    wb##wr_reg = addr,

/**@brief       Hardware registers
 */
enum hwReg {
    REGS_TABLE(REGS_EXPAND_AS_ENUM)
    LAST_REG_ENTRY
};

/** @} *//*---------------------------------------------------------------*//**
 * @name        Register aliases
 * @{ *//*--------------------------------------------------------------------*/

#define TLR                             SPR
#define wTLR                            wSPR
#define raTLR                           raSPR
#define waTLR                           waSPR
#define rbTLR                           rbXOFF2
#define wbTLR                           wbXOFF2

/** @} *//*---------------------------------------------------------------*//**
 * @name        Register bit definitions
 * @{ *//*--------------------------------------------------------------------*/

/* SYSC register bits                                                         */
#define SYSC_AUTOIDLE                   (1U << 0)
#define SYSC_SOFTRESET                  (1U << 1)
#define SYSC_ENAWAKEUP                  (1U << 2)

/* SYSS register bits                                                         */
#define SYSS_RESETDONE                  (1U << 0)

/* EFR register bits                                                          */
#define EFR_ENHANCEDEN                  (1U << 4)

/* MCR register bits                                                          */
#define MCR_TCRTLR                      (1U << 6)

/* FCR register bits                                                          */
#define FCR_RX_FIFO_TRIG_Mask           (0x3u << 6)
#define FCR_RX_FIFO_TRIG_8              (0x0u << 6)
#define FCR_RX_FIFO_TRIG_16             (0x1u << 6)
#define FCR_RX_FIFO_TRIG_56             (0x2u << 6)
#define FCR_RX_FIFO_TRIG_60             (0x3u << 6)
#define FCR_TX_FIFO_TRIG_Mask           (0x3u << 4)
#define FCR_TX_FIFO_TRIG_8              (0x0u << 4)
#define FCR_TX_FIFO_TRIG_16             (0x1u << 4)
#define FCR_TX_FIFO_TRIG_32             (0x2u << 4)
#define FCR_TX_FIFO_TRIG_56             (0x3u << 4)
#define FCR_DMA_MODE                    (0x1u << 3)
#define FCR_TX_FIFO_CLEAR               (0x1u << 2)
#define FCR_RX_FIFO_CLEAR               (0x1u << 1)
#define FCR_FIFO_EN                     (0x1u << 0)

/* Trigger Level Register (TLR) : register bits                               */
#define TLR_RX_FIFO_TRIG_DMA_Mask       (0x0fU << 4)
#define TLR_RX_FIFO_TRIG_DMA_TO_FCR     (0x00U << 4)
#define TLR_RX_FIFO_TRIG_DMA_4          (0x01U << 4)
#define TLR_RX_FIFO_TRIG_DMA_8          (0x02U << 4)
#define TLR_RX_FIFO_TRIG_DMA_12         (0x03U << 4)
#define TLR_RX_FIFO_TRIG_DMA_16         (0x04U << 4)
#define TLR_RX_FIFO_TRIG_DMA_20         (0x05U << 4)
#define TLR_RX_FIFO_TRIG_DMA_22         (0x06U << 4)
#define TLR_RX_FIFO_TRIG_DMA_28         (0x07U << 4)
#define TLR_RX_FIFO_TRIG_DMA_32         (0x08U << 4)
#define TLR_RX_FIFO_TRIG_DMA_36         (0x09U << 4)
#define TLR_RX_FIFO_TRIG_DMA_40         (0x0aU << 4)
#define TLR_RX_FIFO_TRIG_DMA_44         (0x0bU << 4)
#define TLR_RX_FIFO_TRIG_DMA_48         (0x0cU << 4)
#define TLR_RX_FIFO_TRIG_DMA_52         (0x0dU << 4)
#define TLR_RX_FIFO_TRIG_DMA_56         (0x0eU << 4)
#define TLR_RX_FIFO_TRIG_DMA_60         (0x0fU << 4)
#define TLR_TX_FIFO_TRIG_DMA_Mask       (0x0fU << 0)
#define TLR_TX_FIFO_TRIG_DMA_TO_FCR     (0x00U << 0)
#define TLR_TX_FIFO_TRIG_DMA_4          (0x01U << 0)
#define TLR_TX_FIFO_TRIG_DMA_8          (0x02U << 0)
#define TLR_TX_FIFO_TRIG_DMA_12         (0x03U << 0)
#define TLR_TX_FIFO_TRIG_DMA_16         (0x04U << 0)
#define TLR_TX_FIFO_TRIG_DMA_20         (0x05U << 0)
#define TLR_TX_FIFO_TRIG_DMA_22         (0x06U << 0)
#define TLR_TX_FIFO_TRIG_DMA_28         (0x07U << 0)
#define TLR_TX_FIFO_TRIG_DMA_32         (0x08U << 0)
#define TLR_TX_FIFO_TRIG_DMA_36         (0x09U << 0)
#define TLR_TX_FIFO_TRIG_DMA_40         (0x0aU << 0)
#define TLR_TX_FIFO_TRIG_DMA_44         (0x0bU << 0)
#define TLR_TX_FIFO_TRIG_DMA_48         (0x0cU << 0)
#define TLR_TX_FIFO_TRIG_DMA_52         (0x0dU << 0)
#define TLR_TX_FIFO_TRIG_DMA_56         (0x0eU << 0)
#define TLR_TX_FIFO_TRIG_DMA_60         (0x0fU << 0)

/* Supplementary Control Register (SCR) : register bits                       */
#define SCR_RXTRIGGRANU1                (0x1u << 7)
#define SCR_TXTRIGGRANU1                (0x1u << 6)
#define SCR_DMAMODE2_Mask               (0x3u << 1)
#define SCR_DMAMODE2_NO_DMA             (0x0u << 1)
#define SCR_DMAMODE2_TX_AND_RX          (0x1u << 1)
#define SCR_DMAMODE2_RX                 (0x2u << 1)
#define SCR_DMAMODE2_TX                 (0x3u << 1)
#define SCR_DMAMODECTL                  (0x1u << 0)

/* Mode Definition Register 1 (MDR1) : register bits                          */
#define MDR1_MODESELECT_Mask            (0x7u << 0)
#define MDR1_MODESELECT_UART16          (0x0u << 0)
#define MDR1_MODESELECT_SIR             (0x1u << 0)
#define MDR1_MODESELECT_UART16AUTO      (0x2u << 0)
#define MDR1_MODESELECT_UART13          (0x3u << 0)
#define MDR1_MODESELECT_MIR             (0x4u << 0)
#define MDR1_MODESELECT_FIR             (0x5u << 0)
#define MDR1_MODESELECT_CIR             (0x6u << 0)
#define MDR1_MODESELECT_DISABLE         (0x7u << 0)

/* Mode Definition Register 3 (MDR3) : register bits                          */
#define MDR3_SET_DMA_TX_THRESHOLD       (0x01u << 2)

/* Interrupt Enable Register (IER) : register bits                            */
#define IER_CTSIT                       (0x1u << 7)
#define IER_RTSIT                       (0x1u << 6)
#define IER_XOFFIT                      (0x1u << 5)
#define IER_SLEEPMODE                   (0x1u << 4)
#define IER_MODEMSTSIT                  (0x1u << 3)
#define IER_LINESTSIT                   (0x1u << 2)
#define IER_THRIT                       (0x1u << 1)
#define IER_RHRIT                       (0x1u << 0)

/* Interrupt Identification Register (IIR) : register bits                    */
#define IIR_IT_TYPE_Mask                (0x3FU << 0)
#define IIR_IT_TYPE_NONE                (0x01U << 0)
#define IIR_IT_TYPE_THR                 (0x01U << 1)
#define IIR_IT_TYPE_RHR                 (0x02U << 1)
#define IIR_IT_TYPE_LINEST              (0x03U << 1)
#define IIR_IT_TYPE_RX_TIMEOUT          (0x06U << 1)
#define IIR_IT_PENDING                  (0x01U << 0)

/* Line Control Register (LCR) : register bits                                */
#define LCR_DIV_EN                      (0x1u << 7)
#define LCR_BREAK_EN                    (0x1u << 6)
#define LCR_PARITY_TYPE2                (0x1u << 5)
#define LCR_PARITY_TYPE1                (0x1u << 4)
#define LCR_PARITY_EN                   (0x1u << 3)
#define LCR_NB_STOP                     (0x1u << 2)
#define LCR_CHAR_LENGTH_Mask            (0x3u << 0)
#define LCR_CHAR_LENGTH_8               (0x3u << 0)
#define LCR_CHAR_LENGTH_7               (0x2u << 0)
#define LCR_CHAR_LENGTH_6               (0x1u << 0)
#define LCR_CHAR_LENGTH_5               (0x0u << 0)

/* Supplementary Status Register (SSR) : register bits                        */
#define SSR_TXFIFOFULL                  (0x01U << 0)

/* Line Status Register (LSR) : register bits                                 */
#define LSR_RXFIFOE                     (0x01U << 0)

/* Tx DMA Threshold Register (TXDMA) : register bits                          */
#define TXDMA_TX_DMA_THRESHOLD_Mask     (0x3fu << 0)

/** @} *//*-------------------------------------------------------------------*/
/*============================================================  DATA TYPES  ==*/

enum lldCfgMode {
    LLD_CFG_MODE_NORM   = 0x0000U,
    LLD_CFG_MODE_A      = 0x0080U,
    LLD_CFG_MODE_B      = 0x00bfU
};

enum lldMode {
    LLD_MODE_DISABLE    = MDR1_MODESELECT_DISABLE,
    LLD_MODE_UART16     = MDR1_MODESELECT_UART16,
    LLD_MODE_UART16AUTO = MDR1_MODESELECT_UART16AUTO,
    LLD_MODE_UART13     = MDR1_MODESELECT_UART13,
    LLD_MODE_MIR        = MDR1_MODESELECT_MIR,
    LLD_MODE_FIR        = MDR1_MODESELECT_FIR,
    LLD_MODE_CIR        = MDR1_MODESELECT_CIR,
    LLD_MODE_SIR        = MDR1_MODESELECT_SIR
};

enum lldIntNum {
    LLD_INT_ALL         = 0xFFU,
    LLD_INT_NONE        = IIR_IT_TYPE_NONE,
    LLD_INT_RX          = IIR_IT_TYPE_RHR,
    LLD_INT_RX_TIMEOUT  = IIR_IT_TYPE_RX_TIMEOUT,
    LLD_INT_TX          = IIR_IT_TYPE_THR,
    LLD_INT_LINEST      = IIR_IT_TYPE_LINEST
};

enum lldState {
    LLD_ENABLE,
    LLD_DISABLE
};

enum lldDMAMode {
    LLD_DMA_MODE_DISABLED   = SCR_DMAMODE2_NO_DMA,
    LLD_DMA_MODE_TX_AND_RX  = SCR_DMAMODE2_TX_AND_RX,
    LLD_DMA_MODE_RX         = SCR_DMAMODE2_RX,
    LLD_DMA_MODE_TX         = SCR_DMAMODE2_TX
};

enum lldDMATxThreshold {
    LLD_DMA_TX_THRESHOLD_64 = MDR3_SET_DMA_TX_THRESHOLD,
    LLD_DMA_TX_THRESHOLD_REG = 0x0u
};

/*======================================================  GLOBAL VARIABLES  ==*/

extern const struct xUartProto DefProtocol;

/*===================================================  FUNCTION PROTOTYPES  ==*/

/*------------------------------------------------------------------------*//**
 * @name        Register access functions
 * @{ *//*--------------------------------------------------------------------*/

/**@brief       Write a value into register
 * @param       ioRemap
 *              Pointer to IO remaped memory
 * @param       reg
 *              Register from enum hwReg.
 * @param       val
 *              Value to write into @c reg.
 */
static inline void lldRegWr(
    volatile uint8_t *  io,
    enum hwReg          reg,
    uint16_t            val) {

    iowrite16(val, io + (uint32_t)reg);
}

/**@brief       Read a value from register
 */
static inline uint16_t lldRegRd(
    volatile uint8_t *  io,
    enum hwReg          reg) {

    uint16_t            retval;

    retval = ioread16(io + (uint32_t)reg);

    return (retval);
}

/**@brief       Set register bits according to @c bits bit mask
 * @return      Original value of register
 */
static inline uint16_t lldRegSetBits(
    volatile uint8_t *  io,
    enum hwReg          reg,
    uint16_t            bits) {

    uint16_t            tmp;

    tmp = lldRegRd(
        io,
        reg);
    lldRegWr(
        io,
        reg,
        tmp | bits);

    return (tmp);
}

/**@brief       Reset register bits according to @c bits bit mask
 * @return      Original value of register
 */
static inline uint16_t lldRegResetBits(
    volatile uint8_t *  io,
    enum hwReg          reg,
    uint16_t            bits) {

    uint16_t            tmp;

    tmp = lldRegRd(
        io,
        reg);
    lldRegWr(
        io,
        reg,
        tmp & ~bits);

    return (tmp);
}

/**@brief       Write register bits which are bitmasked with @c bitmask
 */
void lldRegWrBits(
    volatile uint8_t *  io,
    enum hwReg          reg,
    uint16_t            bitmask,
    uint16_t            bits);

/**@brief       Read register bits which are bitmasked with @c bitmask
 */
uint16_t lldRegRdBits(
    volatile uint8_t *  io,
    enum hwReg          reg,
    uint16_t            bitmask);

/**@} *//*----------------------------------------------------------------*//**
 * @name        Generic functions
 * @{ *//*--------------------------------------------------------------------*/

int32_t lldInit(
    volatile uint8_t *  io);

int32_t lldTerm(
    volatile uint8_t *  io);

/**@} *//*----------------------------------------------------------------*//**
 * @name        Configuration mode
 * @{ *//*--------------------------------------------------------------------*/

/**@brief       Set configuration mode
 * @param       ioRemap
 *              Pointer to IO mapped memory
 * @param       cfgMode
 *              Configuration mod, @see lldCfgMode
 *  @arg        LLD_CFG_MODE_A
 *  @arg        LLD_CFG_MODE_B
 *  @arg        LLD_CFG_MODE_NORM
 */
void lldCfgModeSet(
    volatile uint8_t *  io,
    enum lldCfgMode     cfgMode);

/**@} *//*----------------------------------------------------------------*//**
 * @name        Operational mode
 * @{ *//*--------------------------------------------------------------------*/

/**@brief       Set peripheral operation mode
 * @param       ioRemap
 *              Pointer to IO mapped memory
 * @param       mode
 *              Peripheral operation mode, @see lldMode
 */
void lldModeSet(
    volatile uint8_t *  io,
    enum lldMode        mode);

uint16_t lldModeGet(
    volatile uint8_t *  io);

int32_t lldSoftReset(
    volatile uint8_t *  io);

void lldEnhanced(
    volatile uint8_t *  io,
    enum lldState       state);

/**@} *//*----------------------------------------------------------------*//**
 * @name        Interrupt actions
 * @{ *//*--------------------------------------------------------------------*/

void lldIntEnable(
    volatile uint8_t *  io,
    enum lldIntNum      intNum);

void lldIntDisable(
    volatile uint8_t *  io,
    enum lldIntNum      intNum);

static inline uint16_t lldIntGet(
    volatile uint8_t *  io) {

    uint16_t            tmp;

    tmp = lldRegRd(
        io,
        IIR);
    tmp &= IIR_IT_TYPE_Mask;

    return (tmp);
}

/**@} *//*----------------------------------------------------------------*//**
 * @name        FIFO related actions
 * @{ *//*--------------------------------------------------------------------*/

void lldFIFODMAInit(
    volatile uint8_t *  io);

/**@brief       Setup UART module to use FIFO and DMA
 * @param       ioRemap
 *              Pointer to IO mapped memory
 */
int32_t lldDMAFIFOInit(
    volatile uint8_t *  io);

/**@brief       Enable/disable finer granularity
 * @param       ioRemap
 *              Pointer to IO mapped memory
 * @param       state
 *  @arg        LLD_ENABLE
 *  @arg        LLD_DISABLE
 */
void lldFIFORxGranularityState(
    volatile uint8_t *  io,
    enum lldState       state);

/**@brief       Set Rx FIFO notification limit
 * @param       ioRemap
 *              Pointer to IO mapped memory
 * @param       bytes
 *              Number of bytes for notification
 */
void lldFIFORxGranularitySet(
    volatile uint8_t *  io,
    size_t              bytes);

size_t lldFIFORxOccupied(
    volatile uint8_t *  io);

size_t lldFIFOTxFree(
    volatile uint8_t *  io);

void lldFIFORxFlush(
    volatile uint8_t *  io);

void lldFIFOTxFlush(
    volatile uint8_t *  io);

/**@brief       This function controls the method of setting the Transmit DMA
 *              threshold value.
 * @details     The Transmit DMA threshold value can be set to a default value
 *              of 64 characters or can take the value in  TX_DMA_THRESHOLD
 *              register.
 */
void lldUARTTxDMAThresholdCtrl(
    volatile uint8_t *  io,
    enum lldDMATxThreshold threshold);

/**@brief       This function programs the TX_DMA_THRESHOLD register which holds
 *              Transmit DMA threshold value to be used.
 * @details     When a TX DMA threshold value other than 64 characters is
 *              required, this function can be used to program the same.
 * @param       io
 *              Memory IO address
 * @param       val
 *              The transmit DMA threshold value
 */
void lldUARTDMATxThresholdVal(
    volatile uint8_t *  io,
    uint32_t            val);

/**@} *//*----------------------------------------------------------------*//**
 * @name        Protocol
 * @{ *//*--------------------------------------------------------------------*/


/**@brief       Setup UART protocol configuration
 * @param       ioRemap
 *              Pointer to IO mapped memory
 * @param       uartCfg
 *              Protocol configuration structure, see @ref uartCfg. When NULL
 *              default protocol configuration is used.
 * @return
 *  @retval     RETVAL_SUCCESS : the operation was successful
 *  @retval     ENOTSUPP : the configuration is not supported
 *  @retval     EINVAL : argument value is invalied, using default value
 */
int32_t lldProtocolSet(
    volatile uint8_t *  io,
    const struct xUartProto * proto);

void lldProtocolPrint(
    const struct xUartProto * proto);

/**@} *//*--------------------------------------------------------------------*/

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of x-16c750_lld.h
 ******************************************************************************/
#endif /* X_16C750_LLD_H_ */
