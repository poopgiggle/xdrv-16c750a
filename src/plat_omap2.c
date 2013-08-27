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
 * @author      Nenad Radulovic
 * @brief       Platform level device initalization
 *********************************************************************//** @{ */

/*=========================================================  INCLUDE FILES  ==*/

#include <linux/string.h>
#include <asm-generic/errno.h>
#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>
#include "x-16c750_cfg.h"
#include "x-16c750_regs.h"
#include "plat.h"
#include "log.h"

/*=========================================================  LOCAL MACRO's  ==*/
/*======================================================  LOCAL DATA TYPES  ==*/
/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/
/*=======================================================  LOCAL VARIABLES  ==*/

struct omap_hwmod * gHwmod;

/*======================================================  GLOBAL VARIABLES  ==*/
/*============================================  LOCAL FUNCTION DEFINITIONS  ==*/
/*===================================  GLOBAL PRIVATE FUNCTION DEFINITIONS  ==*/
/*====================================  GLOBAL PUBLIC FUNCTION DEFINITIONS  ==*/

int platInit(
    struct uartCtx *    uartCtx) {

    struct platform_device * platDev;
    int                 retval;
    char                uartName[10];

    scnprintf(
        uartName,
        sizeof(uartName),
        "uart%d",
        uartCtx->id + 1U);
    LOG_INFO("OMAP HWMOD: creating %s device", uartName);
    gHwmod = omap_hwmod_lookup(
        uartName);
    scnprintf(
        uartName,
        sizeof(uartName),
        "uart%d",
        uartCtx->id);

    if (NULL == gHwmod) {
        LOG_WARN("OMAP UART: device not found");

        return (-ENODEV);
    }
    platDev = omap_device_build(
        uartName,
        uartCtx->id,
        gHwmod,
        NULL,
        0,
        NULL,
        0,
        0);

    if (NULL == platDev) {
        LOG_WARN("OMAP UART: device build failed");

        return (-ENOTSUPP);
    }
    omap_device_reset(
        &platDev->dev);
    retval = omap_device_enable_clocks(
        to_omap_device(platDev));

    if (RETVAL_SUCCESS != retval) {
        LOG_WARN("OMAP UART: can't enable device clocks");

        return (retval);
    }
    retval = omap_device_enable(
        platDev);

    if (RETVAL_SUCCESS != retval) {
        LOG_WARN("OMAP UART: can't enable device");

        return (retval);
    }
    uartCtx->ioremap = omap_device_get_rt_va(
        platDev->archdata.od);
    LOG_INFO("using ioremap addr: %p", uartCtx->ioremap);

    return (retval);
}

int platCleanup(
    struct uartCtx *    uartCtx) {

    return (RETVAL_SUCCESS);
}

int platTerm(
    void) {

    return (RETVAL_SUCCESS);
}

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of plat_omap.c
 ******************************************************************************/
