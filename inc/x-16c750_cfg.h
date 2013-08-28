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
 * @brief   	Configuration of x-16c750 driver.
 *********************************************************************//** @{ */

#if !defined(X_16C750_CFG_H_)
#define X_16C750_CFG_H_

/*=========================================================  INCLUDE FILES  ==*/

/*===============================================================  DEFINES  ==*/
/** @cond */

/** @endcond */
/*==============================================================  SETTINGS  ==*/

/*------------------------------------------------------------------------*//**
 * @name        Generic driver configuration
 * @{ *//*--------------------------------------------------------------------*/

#define CFG_UART                        UART3

#define CFG_DRV_NAME                    "xeno16C750A"

#define CFG_WAIT_EXIT_DELAY             1000

#define CFG_BUFF_TX_NAME                "uartBuffTx"
#define CFG_BUFF_TX_SIZE                4096UL

#define CFG_BUFF_RX_NAME                "uartBuffRx"
#define CFG_BUFF_RX_SIZE                4096UL

/** @} *//*---------------------------------------------------------------*//**
 * @name        Default driver settings
 * @{ *//*--------------------------------------------------------------------*/

#define CFG_DEFAULT_BAUD_RATE           115200

/** @} *//*-------------------------------------------------------------------*/
/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of x-16c750_cfg.h
 ******************************************************************************/
#endif /* X_16C750_CFG_H_ */
