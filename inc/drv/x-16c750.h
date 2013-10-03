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
#include "drv/x-16c750_cfg.h"
#include "circbuff/circbuff.h"
#include "arch/compiler.h"

/*===============================================================  MACRO's  ==*/
/*------------------------------------------------------  C++ extern begin  --*/
#ifdef __cplusplus
extern "C" {
#endif

/*============================================================  DATA TYPES  ==*/

/**@brief       States of the context process
 */
enum ctxState {
    CTX_STATE_INIT,                                                             /**<@brief STATE_INIT                                       */
    CTX_STATE_LOCKS,
    CTX_STATE_TX_BUFF,
    CTX_STATE_TX_BUFF_ALLOC,
    CTX_STATE_RX_BUFF,
    CTX_STATE_RX_BUFF_ALLOC,
};

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
        rtdm_event_t        opr;                                                /**<@brief Operational event                                */
        nanosecs_rel_t      oprTimeout;
        rtdm_sem_t          acc;                                                /**<@brief Access mutex                                     */
        nanosecs_rel_t      accTimeout;
        rtdm_user_info_t *  user;
        struct buff {
            circBuff_T          handle;                                         /**<@brief Buffer handle                                    */
#if (0 == CFG_DMA_MODE)
            RT_HEAP             storage;                                        /**<@brief Heap for internal buffers                        */
#elif (1 == CFG_DMA_MODE)
            volatile uint8_t *  phy;
#elif (2 == CFG_DMA_MODE)
            volatile uint8_t *  phy;
#endif
        }                   buff;
        size_t              pend;
#if (1 == CFG_DMA_MODE)
        size_t              chunk;
#elif (2 == CFG_DMA_MODE)
#endif
        enum uartStatus     status;
    }                   tx, rx;                                                 /**<@brief TX and RX channel                                */
    struct cache {
        volatile uint8_t *  io;
        uint32_t            IER;
    }                   cache;
    struct devData *    devData;
    struct xUartProto   proto;
    enum ctxState       state;
    uint32_t            signature;
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
