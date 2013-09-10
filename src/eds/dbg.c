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
 * @author      nenad
 * @brief       Short desciption of file
 * @addtogroup  module_impl
 *********************************************************************//** @{ */

/*=========================================================  INCLUDE FILES  ==*/

#include "eds/dbg.h"

/*=========================================================  LOCAL MACRO's  ==*/
/*======================================================  LOCAL DATA TYPES  ==*/
/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/
/*=======================================================  LOCAL VARIABLES  ==*/
/*======================================================  GLOBAL VARIABLES  ==*/
/*============================================  LOCAL FUNCTION DEFINITIONS  ==*/
/*===================================  GLOBAL PRIVATE FUNCTION DEFINITIONS  ==*/
/*====================================  GLOBAL PUBLIC FUNCTION DEFINITIONS  ==*/

PORT_C_NORETURN void dbgAssert(
    const struct cObj * cObj,
    const char *    expr,
    enum esDbgMsg   msg) {

    const char *    msgTxt;

    switch (msg) {
        case ES_DBG_NOT_ENOUGH_MEM : {
            msgTxt = "not enough memory";
            break;
        }
        case ES_DBG_OBJECT_NOT_VALID : {
            msgTxt = "object is not valid";
            break;
        }
        case ES_DBG_OUT_OF_RANGE : {
            msgTxt = "out of range";
            break;
        }
        case ES_DBG_POINTER_NULL : {
            msgTxt = "pointer is NULL";
            break;
        }
        case ES_DBG_UNKNOWN_ERROR : {
            msgTxt = "unknown error";
            break;
        }
        case ES_DBG_USAGE_FAILURE : {
            msgTxt = "usage failure";
            break;
        }
        default : {
            msgTxt = "unspecified error";
            break;
        }
    }
    userAssert(
        cObj->mod->name,
        cObj->mod->desc,
        cObj->mod->auth,
        cObj->mod->file,
        cObj->fn,
        expr,
        msgTxt,
        (u32)msg);

    while (TRUE);
}

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of dbg.c
 ******************************************************************************/
