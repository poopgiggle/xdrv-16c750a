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

/**@brief       HWMOD UART name
 */
#define DEF_UART_NAME                   "uart"

/*======================================================  LOCAL DATA TYPES  ==*/

/**@brief       Platform setup states
 */
enum platState {
    PLAT_STATE_INIT,                                                            /**<@brief PLAT_STATE_INIT                                  */
    PLAT_STATE_LOOKUP,                                                          /**<@brief PLAT_STATE_LOOKUP                                */
    PLAT_STATE_BUILD,                                                           /**<@brief PLAT_STATE_BUILD                                 */
    PLAT_STATE_ECLK,                                                            /**<@brief PLAT_STATE_ECLK                                  */
    PLAT_STATE_ENABLE                                                           /**<@brief PLAT_STATE_ENABLE                                */
};

/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/

static void platCleanup(
    struct uartCtx *    uartCtx,
    enum platState      state);

/*=======================================================  LOCAL VARIABLES  ==*/
/*======================================================  GLOBAL VARIABLES  ==*/
/*============================================  LOCAL FUNCTION DEFINITIONS  ==*/

static void platCleanup(
    struct uartCtx *    uartCtx,
    enum platState      state) {

    switch (state) {

        case PLAT_STATE_ENABLE : {
            omap_device_disable_clocks(
                to_omap_device(uartCtx->platDev));
            /* fall down */
        }

        case PLAT_STATE_ECLK : {
            omap_device_delete(
                to_omap_device(uartCtx->platDev));
            /* fall down */
        }

        case PLAT_STATE_BUILD : {
            /* nothing */
            /* fall down */
        }
        case PLAT_STATE_LOOKUP : {
            /* nothing */
            break;
        }

        default : {
            /* nothing */
        }
    }
}

/*===================================  GLOBAL PRIVATE FUNCTION DEFINITIONS  ==*/
/*====================================  GLOBAL PUBLIC FUNCTION DEFINITIONS  ==*/

int platInit(
    struct uartCtx *    uartCtx) {

    struct platform_device * platDev;
    struct omap_hwmod * hwmod;
    enum platState      state;
    int                 retval;
    char                uartName[10];
    char                hwmodUartName[10];

    /*-- Initializaion state -------------------------------------------------*/
    state = PLAT_STATE_INIT;
    scnprintf(
        uartName,
        sizeof(uartName),
        DEF_UART_NAME "%d",
        uartCtx->id);                                                           /* NOTE: Since hwmod UART count is messed up we need the right name now */
    LOG_DBG("OMAP UART: creating %s device", uartName);

    /*-- HWMOD lookup state --------------------------------------------------*/
    state = PLAT_STATE_LOOKUP;
    scnprintf(
        hwmodUartName,
        sizeof(hwmodUartName),
        "uart%d",
        uartCtx->id + 1U);                                                      /* NOTE: hwmod UART count starts at 1, so we must add 1 here */
    hwmod = omap_hwmod_lookup(
        hwmodUartName);

    if (NULL == hwmod) {
        LOG_WARN("OMAP UART: device not found");
        platCleanup(
            uartCtx,
            state);

        return (-ENODEV);
    }

    /*-- Device build state --------------------------------------------------*/
    state = PLAT_STATE_BUILD;
    platDev = omap_device_build(
        uartName,
        uartCtx->id,
        hwmod,
        NULL,
        0,
        NULL,
        0,
        0);

    if (NULL == platDev) {
        LOG_WARN("OMAP UART: device build failed");
        platCleanup(
            uartCtx,
            state);

        return (-ENOTSUPP);
    }

    /*-- Device enable clocks state ------------------------------------------*/
    state = PLAT_STATE_ECLK;
    retval = omap_device_enable_clocks(
        to_omap_device(platDev));

    if (RETVAL_SUCCESS != retval) {
        LOG_WARN("OMAP UART: can't enable device clocks");
        platCleanup(
            uartCtx,
            state);

        return (retval);
    }

    /*-- Device enable state -------------------------------------------------*/
    state = PLAT_STATE_ENABLE;
    retval = omap_device_enable(
        platDev);

    if (RETVAL_SUCCESS != retval) {
        LOG_WARN("OMAP UART: can't enable device");
        platCleanup(
            uartCtx,
            state);

        return (retval);
    }

    /*-- Saving references to device data ------------------------------------*/
    uartCtx->ioRemap = omap_device_get_rt_va(
        to_omap_device(platDev));
    uartCtx->platDev = platDev;

    return (retval);
}

/*NOTE:     This function should release all used resources, unfortunately TI
 *          has really dropped the ball here because they did NOT implemented
 *          release functions like they should have.
 */
int platTerm(
    struct uartCtx *    uartCtx) {

    int                 retval;

    LOG_DBG("OMAP UART: destroying device");
    retval = omap_device_shutdown(
        uartCtx->platDev);
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "OMAP UART: can't shutdown device");
    retval = omap_device_disable_clocks(
        to_omap_device(uartCtx->platDev));
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "OMAP UART: can't disable device clocks");
    omap_device_delete(
        to_omap_device(uartCtx->platDev));
    platform_device_unregister(
        uartCtx->platDev);

    return (retval);
}

int platDMAInit(
    struct uartCtx *    uartCtx) {

    return (RETVAL_SUCCESS);
}

int platDMATerm(
    struct uartCtx *    uartCtx) {

    return (RETVAL_SUCCESS);
}

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of plat_omap.c
 ******************************************************************************/
