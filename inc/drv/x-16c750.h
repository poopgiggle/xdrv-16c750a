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
 * @brief       Driver level shared types
 *********************************************************************//** @{ */

#if !defined(X_16C750_H_)
#define X_16C750_H_

/*=========================================================  INCLUDE FILES  ==*/

#include <rtdm/rtdm_driver.h>
#include <native/heap.h>

#include "drv/x-16c750_ioctl.h"
#include "circbuff/circbuff.h"
#include "arch/compiler.h"

/*===============================================================  MACRO's  ==*/

/**@brief       Return value: operation was successful
 */
#define RETVAL_SUCCESS                  0
#define RETVAL_FAILURE                  !RETVAL_SUCCESS

/*------------------------------------------------------  C++ extern begin  --*/
#ifdef __cplusplus
extern "C" {
#endif

/*============================================================  DATA TYPES  ==*/

enum uartStatus {
    UART_STATUS_NORMAL,
    UART_STATUS_SOFT_OVERFLOW,
    UART_STATUS_TIMEOUT,
    UART_STATUS_FAULT_USAGE,
    UART_STATUS_BUSY,
    UART_STATUS_BAD_FILE_NUMBER,
    UART_STATUS_UNHANDLED_INTERRUPT
};

/**@brief       UART channel context structure
 */
struct uartCtx {
    rtdm_lock_t         lock;                                                   /**<@brief Lock to protect this structure                   */
    rtdm_irq_t          irqHandle;                                              /**<@brief IRQ routine handler structure                    */
    struct unit {
        RT_HEAP             heapHandle;                                         /**<@brief Heap for internal buffers                        */
        circBuff_T          buffHandle;                                         /**<@brief Buffer handle                                    */
        nanosecs_rel_t      accTimeout;
        nanosecs_rel_t      oprTimeout;
        rtdm_event_t        opr;                                                /**<@brief Operational event                                */
        rtdm_mutex_t        acc;                                                /**<@brief Access mutex                                     */
        uint32_t            pend;
        enum uartStatus     status;
    }                   tx, rx;                                                 /**<@brief TX and RX channel                                */
    struct cache {
        volatile uint8_t *  io;
        uint32_t            DLL;
        uint32_t            DLH;
        uint32_t            EFR;
        uint32_t            IER;
        uint32_t            LSR;
        uint32_t            MCR;
        uint32_t            SCR;
    }                   cache;
    struct xUartProto   proto;
    uint32_t                signature;
};

/*======================================================  GLOBAL VARIABLES  ==*/
/*===================================================  FUNCTION PROTOTYPES  ==*/
/*--------------------------------------------------------  C++ extern end  --*/
#ifdef __cplusplus
}
#endif

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of x-16c750.h
 ******************************************************************************/
#endif /* X_16C750_H_ */
