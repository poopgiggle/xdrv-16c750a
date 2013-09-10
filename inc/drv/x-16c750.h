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

#include <linux/platform_device.h>

#include <rtdm/rtdm_driver.h>
#include <native/heap.h>
#include <native/queue.h>

#include "drv/x-16c750_ctrl.h"
#include "circbuff/circbuff.h"
#include "port/compiler.h"
#include "eds/smp.h"

/*===============================================================  MACRO's  ==*/

/**@brief       Return value: operation was successful
 */
#define RETVAL_SUCCESS                  0
#define RETVAL_FAILURE                  !RETVAL_SUCCESS

#define UARTCTX_SIGNATURE               ((u32)0xDEADDEEEU)

#define UARTCTX_NAME_SIZE               12U

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
    esFsm_T             fsm;
    rtdm_lock_t         lock;                                                   /**<@brief Lock to protect this structure                   */
    rtdm_irq_t          irqHandle;                                              /**<@brief IRQ routine handler structure                    */
    struct {
        RT_HEAP             heapHandle;                                         /**<@brief Heap for internal buffers                        */
        RT_QUEUE            queueHandle;                                        /**<@brief Queue for RT comms                               */
        circBuff_T          buffHandle;                                         /**<@brief Buffer handle                                    */
        nanosecs_rel_t      accTimeout;
        nanosecs_rel_t      oprTimeout;
        rtdm_event_t        opr;                                                /**<@brief Operational event                                */
        rtdm_mutex_t        acc;                                                /**<@brief Access mutex                                     */
        void *              queue;                                              /**<@brief Buffer storage                                   */
        enum uartStatus     status;
    }                   tx, rx;                                                 /**<@brief TX and RX channel                                */
    struct {
        volatile u8 *       io;
        u32                 IER;
    }                   cache;
    struct protocol {
        u32                 baud;
        enum xUartParity    parity;
        enum xUartDataBits  dataBits;
        enum xUartStopBits  stopBits;
    }                   proto;
    struct hwInfo {
        bool_T              initialized;
        void *              resource;
    }                   hw;
    u32                 id;
    char                name[UARTCTX_NAME_SIZE];
    u32                 signature;
};

/*======================================================  GLOBAL VARIABLES  ==*/
/*===================================================  FUNCTION PROTOTYPES  ==*/

void drvManRegisterFSM(
    u32                 id);

void drvManUnregisterFSM(
    u32                 id);

/*--------------------------------------------------------  C++ extern end  --*/
#ifdef __cplusplus
}
#endif

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of x-16c750.h
 ******************************************************************************/
#endif /* X_16C750_H_ */
