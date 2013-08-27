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
 * @brief       Register definitions of 16C750 UART hardware
 * @addtogroup  module_impl
 *********************************************************************//** @{ */

#if !defined(X_16C750_REGS_H_)
#define X_16C750_REGS_H_

/*=========================================================  INCLUDE FILES  ==*/
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
    entry(  IIR,    FCR,    IIR,    FCR,    EFT,    EFT,    0x08)               \
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

#define SYSC_AUTOIDLE                   (1U << 0)
#define SYSC_SOFTRESET                  (1U << 1)
#define SYSC_ENAWAKEUP                  (1U << 2)

#define SYSS_RESETDONE                  (1U << 0)

/**@brief       Create HW regs from table
 */
#define REGS_EXPAND_AS_ENUM(rd0_reg, wr0_reg, rdN_reg, wrN_reg, rd_reg, wr_reg, addr)       \
    _r##rd0_reg = addr,                                                          \
    _w##wr0_reg = addr,                                                          \
    _rn##rdN_reg = addr,                                                         \
    _wn##wrN_reg = addr,                                                         \
    _rs##rd_reg = addr,                                                          \
    _ws##wr_reg = addr,

#define REGS_EXPAND_AS_ENUM_UART_MODE_RD(rd0_reg, wr0_reg, rdN_reg, wrN_reg, rd_reg, wr_reg, addr) \
    r##rd0_reg = addr,

#define REGS_EXPAND_AS_ENUM_UART_MODE_WR(rd0_reg, wr0_reg, rdN_reg, wrN_reg, rd_reg, wr_reg, addr) \
    w##wr0_reg = addr,

enum hwUartRegsRd {
    REGS_TABLE(REGS_EXPAND_AS_ENUM_UART_MODE_RD)
};

enum hwUartRegsWr {
    REGS_TABLE(REGS_EXPAND_AS_ENUM_UART_MODE_WR)
};

/**@brief       Hardware registers
 */
enum hwRegs {
    REGS_TABLE(REGS_EXPAND_AS_ENUM)
    LAST_REG_ENTRY
};

/**@brief       Register address alignment in bytes
 */
#define HW_IOMAP_REG_ALIGN              4U

/**@brief       Size of IOMEM space
 * @details     We add here +3U to acoount for the last register size
 */
#define HW_IOMAP_SIZE                   (LAST_REG_ENTRY + HW_IOMAP_REG_ALIGN)

/** @} *//*-------------------------------------------------------------------*/

/**@brief       Available UARTs on AM335x
 */
 /*
  *     | UART #  | IOMEM         | IRQ
  */
#define UART_DATA_TABLE(entry)                                                  \
    entry(UARTO,    0x44e09000,     72)                                         \
    entry(UART1,    0x48022000,     73)                                         \
    entry(UART2,    0x48024000,     74)                                         \
    entry(UART3,    0x481A6000,     44)                                         \
    entry(UART4,    0x481A8000,     45)                                         \
    entry(UART5,    0x481AA000,     46)

#define UART_DATA_EXPAND_AS_UART(uart, mem, iqr)                                \
    uart,

#define UART_DATA_EXPAND_AS_MEM(uart, mem, irq)                                 \
    (unsigned long)mem,

#define UART_DATA_EXPAND_AS_IRQ(uart, mem, irq)                                 \
    irq,

enum hwUart {
    UART_DATA_TABLE(UART_DATA_EXPAND_AS_UART)
};

extern const unsigned long hwIo[];

extern const unsigned long hwIrq[];

/*============================================================  DATA TYPES  ==*/
/*======================================================  GLOBAL VARIABLES  ==*/
/*===================================================  FUNCTION PROTOTYPES  ==*/
/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of x-16c750_regs.h
 ******************************************************************************/
#endif /* X_16C750_REGS_H_ */
