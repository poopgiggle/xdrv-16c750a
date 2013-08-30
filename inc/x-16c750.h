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
#include <rtdm/rtserial.h>
#include <rtdm/rtdm_driver.h>
#include <native/queue.h>

/*===============================================================  MACRO's  ==*/

/**@brief       Return value: operation was successful
 */
#define RETVAL_SUCCESS                  0
#define RETVAL_FAILURE                  -1

/*------------------------------------------------------  C++ extern begin  --*/
#ifdef __cplusplus
extern "C" {
#endif

/*============================================================  DATA TYPES  ==*/

struct uartCfg {
    rtser_config_t      rtser;
    size_t              qTxSize;
    size_t              qRxSize;
};

/**@brief       UART device context structure
 */
struct uartCtx {
    rtdm_lock_t         lock;                                                   /**<@brief Lock to protect this structure                   */
    rtdm_irq_t          irqHandle;                                              /**<@brief IRQ routine handler structure                    */
    struct rtdm_device *        rtdev;                                          /**<@brief Real-time device driver                          */
    struct platform_device *    platDev;                                        /**<@brief Linux kernel device driver                       */
    u32                 id;                                                     /**<@brief UART ID number (maybe unused)                    */
    rtser_config_t      cfg;                                                    /**<@brief Current device configuration                     */
    u8 * __iomem        io;                                                     /**<@brief Remaped IO memory area                           */
    RT_QUEUE            qTxHandle;                                              /**<@brief TX buffer handle                                 */
    RT_QUEUE            qRxHandle;                                              /**<@brief RX buffer handle                                 */
    void *              qTx;                                                    /**<@brief TX buffer storage                                */
    void *              qRx;                                                    /**<@brief RX buffer storage                                */
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
