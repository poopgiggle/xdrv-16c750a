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
 * @brief       Interface of plat.
 * @addtogroup  module_intf
 *********************************************************************//** @{ */

#if !defined(PLAT_H_)
#define PLAT_H_

/*=========================================================  INCLUDE FILES  ==*/

#include "x-16c750.h"
#include "x-16c750_cfg.h"
#include "x-16c750_regs.h"

/*===============================================================  MACRO's  ==*/

#if defined(PLAT_H_VAR)
# define PLAT_H_EXT
#else
/** @cond                                                                     */
# define PLAT_H_EXT extern
/** @endcond                                                                  */
#endif

/*------------------------------------------------------------------------*//**
 * @name        Macro group
 * @brief       brief description
 * @{ *//*--------------------------------------------------------------------*/

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

/** @} *//*-------------------------------------------------------------------*/
/*======================================================  GLOBAL VARIABLES  ==*/

/*------------------------------------------------------------------------*//**
 * @name        Variables group
 * @brief       brief description
 * @{ *//*--------------------------------------------------------------------*/

int platInit(
    struct uartCtx *    uartCtx);

int platTerm(
    void);

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
 * END of plat.h
 ******************************************************************************/
#endif /* PLAT_H_ */
