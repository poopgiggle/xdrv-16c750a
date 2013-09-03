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
 * @brief       IOCTL interface for 16C750 UART driver.
 *********************************************************************//** @{ */

#if !defined(X_16C750_IOCTL_H_)
#define X_16C750_IOCTL_H_

/*=========================================================  INCLUDE FILES  ==*/

#include <linux/ioctl.h>
#include <rtdm/rtdm.h>

/*===============================================================  MACRO's  ==*/

/*------------------------------------------------------------------------*//**
 * @name        Macro group
 * @brief       brief description
 * @{ *//*--------------------------------------------------------------------*/

#define XUART_IOCTL_VERSION             1

#define XUART_IOCTL_TYPE                RTDM_CLASS_SERIAL

#define XUART_CONFIG_GET                                                        \
    _IOR(XUART_IOCTL_TYPE, 0x00,struct xUartProtocol)

#define XUART_CONFIG_SET                                                        \
    _IOW(XUART_IOCTL_TYPE, 0x01,struct xUartProtocol)

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

enum xUartParity {
    XUART_PARITY_NONE,
    XUART_PARITY_EVEN,
    XUART_PARITY_ODD
};

enum xUartDataBits {
    XUART_DATA_8,
    XUART_DATA_5
};

enum xUartStopBits {
    XUART_STOP_1,
    XUART_STOP_1n5,
    XUART_STOP_2
};

struct xUartProtocol {
    u32                 baud;
    enum xUartParity    parity;
    enum xUartDataBits  dataBits;
    enum xUartStopBits  stopBits;
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
 * END of x-16c750_ioctl.h
 ******************************************************************************/
#endif /* X_16C750_IOCTL_H_ */
