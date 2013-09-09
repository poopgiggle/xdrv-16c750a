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
 * @brief       Control interface for 16C750 UART driver.
 *********************************************************************//** @{ */

#if !defined(X_16C750_IOCTL_H_)
#define X_16C750_IOCTL_H_

/*=========================================================  INCLUDE FILES  ==*/
/*===============================================================  MACRO's  ==*/

#define XUART_CMD_SIGNATURE             0xDEADBEAFU

/*------------------------------------------------------  C++ extern begin  --*/
#ifdef __cplusplus
extern "C" {
#endif

/*============================================================  DATA TYPES  ==*/

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


enum xUartCmdId {
    XUART_CMD_TERM,
    XUART_CMD_CHN_OPEN,
    XUART_CMD_CHN_CLOSE,
    XUART_CMD_CHN_GET_PARAM,
    XUART_CMD_CHN_SET_PARAM
};

struct xUartCmd {
    unsigned int        cmdId;
    unsigned int        uartId;
    unsigned int        signature;
};

typedef struct xUartCmd xUartCmd_T;

struct xUartCmdProto {
    struct xUartCmd     cmd;
    unsigned int        baud;
    enum xUartParity    parity;
    enum xUartDataBits  dataBits;
    enum xUartStopBits  stopBits;
};

typedef struct xUartCmdProto xUartCmdProto_T;

/*======================================================  GLOBAL VARIABLES  ==*/
/*===================================================  FUNCTION PROTOTYPES  ==*/
/*--------------------------------------------------------  C++ extern end  --*/
#ifdef __cplusplus
}
#endif

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of x-16c750_ioctl.h
 ******************************************************************************/
#endif /* X_16C750_IOCTL_H_ */
