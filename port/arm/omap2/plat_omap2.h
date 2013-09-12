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
 * @brief       Port for OMAP2 platform
 *********************************************************************//** @{ */

#if !defined(PLAT_OMAP2_H_)
#define PLAT_OMAP2_H_

/*=========================================================  INCLUDE FILES  ==*/

#include "drv/x-16c750_lld.h"

/*===============================================================  MACRO's  ==*/

#if defined(CONFIG_SOC_OMAPAM33XX)

/**@brief       Available UARTs on AM335x
 */
 /*
  *       | UART #    | IOMEM                     | IRQ
  */
#define UART_DATA_TABLE(entry)                                                  \
    entry(  UARTO,      0x44e09000UL,               72)                         \
    entry(  UART1,      0x48022000UL,               73)                         \
    entry(  UART2,      0x48024000UL,               74)                         \
    entry(  UART3,      0x481a6000UL,               44)                         \
    entry(  UART4,      0x481a8000UL,               45)                         \
    entry(  UART5,      0x481aa000UL,               46)

/*
 *        | Baud-rate | UART mode                 | Divisor
 */
#define BAUD_RATE_CFG_TABLE(entry)                                              \
    entry(  9600,       LLD_MODE_UART16,            313)                        \
    entry(  19200,      LLD_MODE_UART16,            156)                        \
    entry(  38400,      LLD_MODE_UART16,            78)                         \
    entry(  115200,     LLD_MODE_UART16,            26)                         \
    entry(  921600,     LLD_MODE_UART13,            4)

#endif

/*------------------------------------------------------  C++ extern begin  --*/
#ifdef __cplusplus
extern "C" {
#endif

/*============================================================  DATA TYPES  ==*/
/*======================================================  GLOBAL VARIABLES  ==*/
/*===================================================  FUNCTION PROTOTYPES  ==*/
/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of plat_omap2.h
 ******************************************************************************/
#endif /* PLAT_OMAP2_H_ */
