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
 * @brief       Interface of log.
 * @addtogroup  module_intf
 *********************************************************************//** @{ */

#if !defined(LOG_H_)
#define LOG_H_

/*=========================================================  INCLUDE FILES  ==*/

#include <linux/printk.h>
#include "x-16c750_cfg.h"

/*===============================================================  MACRO's  ==*/

#if (1 == CFG_DBG_ENABLE)
#define LOG_DBG(msg, ...)                                                       \
    printk(KERN_INFO CFG_DRV_NAME "(DBG): " msg "\n", ##__VA_ARGS__);
#else
#define LOG_DBG(msg, ...)
#endif

#define LOG_INFO(msg, ...)                                                      \
    printk(KERN_INFO CFG_DRV_NAME ": " msg "\n", ##__VA_ARGS__)

#define LOG_WARN(msg, ...)                                                      \
    printk(KERN_WARNING CFG_DRV_NAME " <WARN>: line %d: " msg "\n", __LINE__, ##__VA_ARGS__)

#define LOG_WARN_IF(expr, msg, ...)                                             \
    do {                                                                        \
        if ((expr)) {                                                           \
            LOG_WARN(msg, ##__VA_ARGS__);                                       \
        }                                                                       \
    } while (0)

#define LOG_ERR(msg, ...)                                                       \
    printk(KERN_ERR CFG_DRV_NAME " <ERR>: line %d: " msg "\n", __LINE__, ##__VA_ARGS__)

#define LOG_ERR_IF(expr, msg, ...)                                              \
    do {                                                                        \
        if ((expr)) {                                                           \
            LOG_ERR(msg, ##__VA_ARGS__);                                        \
        }                                                                       \
    } while (0)

#define LOG_VAR(var)                                                            \
    printk(KERN_INFO CFG_DRV_NAME " VAR " #var " : %d\n", var )

#define LOG_PVAR(var)                                                           \
    printk(KERN_INFO CFG_DRV_NAME " PTR " #var " : %p\n", var )

/*============================================================  DATA TYPES  ==*/
/*======================================================  GLOBAL VARIABLES  ==*/
/*===================================================  FUNCTION PROTOTYPES  ==*/
/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of log.h
 ******************************************************************************/
#endif /* LOG_H_ */
