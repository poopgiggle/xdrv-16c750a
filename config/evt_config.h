/*
 * This file is part of x-16c750
 *
 * Template version: 1.1.15 (03.07.2013)
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
 * @author  	nenad
 * @brief   	Configuration of evt_config.
 * @addtogroup  module_cfg
 *********************************************************************//** @{ */

#if !defined(EVT_CONFIG_H_)
#define EVT_CONFIG_H_

/*=========================================================  INCLUDE FILES  ==*/

#include "port/compiler.h"

/*===============================================================  DEFINES  ==*/
/** @cond */

/** @endcond */
/*==============================================================  SETTINGS  ==*/

/*------------------------------------------------------------------------*//**
 * @name        Definition group
 * @brief       brief description
 * @{ *//*--------------------------------------------------------------------*/

#if !defined(OPT_EVT_STRUCT_ATTRIB)
# define OPT_EVT_STRUCT_ATTRIB
#endif

#if !defined(OPT_EVT_ID_T)
# define OPT_EVT_ID_T                   3U
#endif

#if (0U == OPT_EVT_ID_T)
typedef uint8_t esEvtId_T;
#elif (1U == OPT_EVT_ID_T)
typedef uint16_t esEvtId_T;
#else
typedef uint32_t esEvtId_T;
#endif

/** @} *//*-------------------------------------------------------------------*/
/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of evt_config.h
 ******************************************************************************/
#endif /* EVT_CONFIG_H_ */
