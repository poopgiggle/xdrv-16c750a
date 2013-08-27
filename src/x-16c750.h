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
 * @brief       Interface of x-16c175.
 * @addtogroup  module_intf
 *********************************************************************//** @{ */

#if !defined(X_16C175_H_)
#define X_16C175_H_

/*=========================================================  INCLUDE FILES  ==*/

#include <rtdm/rtserial.h>
#include <rtdm/rtdm_driver.h>
#include <native/queue.h>

/*===============================================================  MACRO's  ==*/

/** @} *//*-------------------------------------------------------------------*/
/*------------------------------------------------------  C++ extern begin  --*/
#ifdef __cplusplus
extern "C" {
#endif

/*============================================================  DATA TYPES  ==*/

/*------------------------------------------------------------------------*//**
 * @name        Data types group
 * @brief       brief description
 * @{ *//*--------------------------------------------------------------------*/

/**@brief       States of the context process
 */
enum ctxState {
    STATE_INIT,
    STATE_RX_ALLOC,                                                             /**<@brief STATE_RX_ALLOC                                   */
    STATE_TX_ALLOC,                                                             /**<@brief STATE_TX_ALLOC                                   */
    STATE_REQ_IO,
    STATE_REMAP_IO
};

/**@brief       UART device context structure
 */
struct uartCtx {
    rtdm_lock_t         lock;                                                   /**<@brief Lock to protect this structure                   */
    rtdm_irq_t          irqHandle;                                              /**<@brief IRQ routine handler structure                    */
    rtser_config_t      cfg;                                                    /**<@brief Current device configuration                     */
    unsigned long *     ioremap;
    RT_QUEUE            buffTxHandle;                                           /**<@brief TX buffer handle                                 */
    RT_QUEUE            buffRxHandle;                                           /**<@brief RX buffer handle                                 */
    void *              buffTx;                                                 /**<@brief TX buffer storage                                */
    void *              buffRx;                                                 /**<@brief RX buffer storage                                */
    enum ctxState       state;
    int                 id;                                                     /**<@brief UART ID number (maybe unused)                    */
};

/** @} *//*-------------------------------------------------------------------*/
/*======================================================  GLOBAL VARIABLES  ==*/

/*------------------------------------------------------------------------*//**
 * @name        Variables group
 * @brief       brief description
 * @{ *//*--------------------------------------------------------------------*/

/** @} *//*-------------------------------------------------------------------*/
/*===================================================  FUNCTION PROTOTYPES  ==*/

/*------------------------------------------------------------------------*//**
 * @name        Function group
 * @brief       brief description
 * @{ *//*--------------------------------------------------------------------*/

/** @} *//*-------------------------------------------------------------------*/
/*--------------------------------------------------------  C++ extern end  --*/
#ifdef __cplusplus
}
#endif

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of x-16c175.h
 ******************************************************************************/
#endif /* X_16C175_H_ */
