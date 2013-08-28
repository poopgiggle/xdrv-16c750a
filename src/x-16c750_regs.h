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
#define FCR_RX_FIFO_TRIG_Mask           (0x3U << 6)
#define FCR_RX_FIFO_TRIG_8              (0x0U << 6)
#define FCR_RX_FIFO_TRIG_16             (0x1U << 6)
#define FCR_RX_FIFO_TRIG_56             (0x2U << 6)
#define FCR_RX_FIFO_TRIG_60             (0x3U << 6)
#define FCR_TX_FIFO_TRIG_Mask           (0x3U << 4)
#define FCR_TX_FIFO_TRIG_8              (0x0U << 4)
#define FCR_TX_FIFO_TRIG_16             (0x1U << 4)
#define FCR_TX_FIFO_TRIG_32             (0x2U << 4)
#define FCR_TX_FIFO_TRIG_56             (0x3U << 4)
#define FCR_DMA_MODE                    (0x1U << 3)
#define FCR_TX_FIFO_CLEAR               (0x1U << 2)
#define FCR_RX_FIFO_CLEAR               (0x1U << 1)
#define FCR_FIFO_EN                     (0x1U << 0)

/* Trigger Level Register (TLR) : register bits                               */
#define TLR_RX_FIFO_TRIG_DMA_Mask       (0xfU << 4)
#define TLR_RX_FIFO_TRIG_DMA_0          (0x0U << 4)
#define TLR_TX_FIFO_TRIG_DMA_Mask       (0xfU << 0)
#define TLR_TX_FIFO_TRIG_DMA_0          (0x0U << 0)

/* Supplementary Control Register (SCR) : register bits                       */
#define SCR_RXTRIGGRANU1                (0x1U << 7)
#define SCR_TXTRIGGRANU1                (0x1U << 6)
#define SCR_DMAMODE2_Mask               (0x3U << 1)
#define SCR_DMAMODE2_NO_DMA             (0x0U << 1)
#define SCR_DMAMODE2_TX_AND_RX          (0x1U << 1)
#define SCR_DMAMODE2_RX                 (0x2U << 1)
#define SCR_DMAMODE2_TX                 (0x3U << 1)
#define SCR_DMAMODECTL                  (0x1U << 0)

/** @} *//*---------------------------------------------------------------*//**
 * @name        Memory and IRQ description
 * @{ *//*--------------------------------------------------------------------*/

/**@brief       Register address alignment in bytes
 */
#define UART_IOMAP_REG_ALIGN            4U

/**@brief       Size of IOMEM space
 */
#define UART_IOMAP_SIZE                 ((u32)LAST_REG_ENTRY + UART_IOMAP_REG_ALIGN)

/**@brief       Available UARTs on AM335x
 */
 /*
  *     | UART #  | IOMEM         | IRQ
  */
#define UART_DATA_TABLE(entry)                                                  \
    entry(UARTO,    0x44e09000UL,   72)                                         \
    entry(UART1,    0x48022000UL,   73)                                         \
    entry(UART2,    0x48024000UL,   74)                                         \
    entry(UART3,    0x481a6000UL,   44)                                         \
    entry(UART4,    0x481a8000UL,   45)                                         \
    entry(UART5,    0x481aa000UL,   46)

#define UART_DATA_EXPAND_AS_UART(uart, mem, iqr)                                \
    uart,

enum hwUart {
    UART_DATA_TABLE(UART_DATA_EXPAND_AS_UART)
    LAST_UART_ENTRY
};

/** @} *//*---------------------------------------------------------------*//**
 * @name        UART configuration modes
 * @{ *//*--------------------------------------------------------------------*/

#define UART_CFG_MODE_A                 0x0080U
#define UART_CFG_MODE_B                 0x00bfU

/** @} *//*-------------------------------------------------------------------*/

#define UART_CFG_MODE_SET(ioRemap, mode)                                        \
    do {                                                                        \
        lldRegWr(ioRemap, LCR, mode);                                            \
    } while (0)

/*============================================================  DATA TYPES  ==*/
/*======================================================  GLOBAL VARIABLES  ==*/

/**@brief       Hardware IO memory maps
 */
extern const unsigned long gIOmap[];

/**@brief       Hardware IRQ numbers
 */
extern const unsigned long gIRQ[];

/*===================================================  FUNCTION PROTOTYPES  ==*/

static inline void lldRegWr(
    u8 *                ioRemap,
    enum hwReg          reg,
    u16                 val) {

    LOG_DBG("write base: %p, off: %2d,  val: %d", ioRemap, reg, val);
    iowrite16(val, ioRemap + (u32)reg);
}

static inline u16 lldRegRd(
    u8 *                ioRemap,
    enum hwReg          reg) {

    int                 retval;

    retval = ioread16(ioRemap + (u32)reg);
    LOG_DBG("read  base: %p, off: %2d,  val: %d", ioRemap, reg, retval);

    return (retval);
}

static inline u16 lldRegSetBits(
    u8 *                ioRemap,
    enum hwReg          reg,
    u16                 bits) {

    u16                 tmp;

    tmp = lldRegRd(ioRemap, reg);
    lldRegWr(ioRemap, reg, tmp | bits);

    return (tmp);
}

static inline u16 lldRegResetBits(
    u8 *                ioRemap,
    enum hwReg          reg,
    u16                 bits) {

    u16                 tmp;

    tmp = lldRegRd(ioRemap, reg);
    lldRegWr(ioRemap, reg, tmp & bits);

    return (tmp);
}

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of x-16c750_lld.h
 ******************************************************************************/
#endif /* X_16C750_LLD_H_ */
