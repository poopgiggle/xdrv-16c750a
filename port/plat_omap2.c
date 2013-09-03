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
 * @brief       Port for OMAP2 platform
 *********************************************************************//** @{ */

/*=========================================================  INCLUDE FILES  ==*/

#include <linux/string.h>
#include <asm-generic/errno.h>
#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>
#include "x-16c750.h"
#include "x-16c750_cfg.h"
#include "x-16c750_lld.h"
#include "plat_omap2.h"
#include "port.h"
#include "log.h"

/*=========================================================  LOCAL MACRO's  ==*/

/**@brief       HWMOD UART name
 */
#define DEF_UART_NAME                   "uart"
#define DEF_UART_NAME_MAX_SIZE          10

#define BAUD_RATE_CFG_EXPAND_AS_BAUD(a, b, c)                                   \
    a,

#define BAUD_RATE_CFG_EXPAND_AS_MDR_DATA(a, b, c)                               \
    b,

#define BAUD_RATE_CFG_EXPAND_AS_DIV_DATA(a, b, c)                               \
    c,

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

enum hwUart {
    UART_DATA_TABLE(UART_DATA_EXPAND_AS_UART)
    LAST_UART_ENTRY
};

struct devRes {
    struct platform_device * platDev;
    volatile u8 *       io;
};

/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/

static void platCleanup(
    struct devRes *     devRes,
    enum platState      state);

static u32 baudRateCfgFindIndex(
    u32                 baudrate);

/*=======================================================  LOCAL VARIABLES  ==*/

static const u32 gBaudRateData[] = {
    BAUD_RATE_CFG_TABLE(BAUD_RATE_CFG_EXPAND_AS_BAUD)
    0
};

static const enum lldMode gModeData[] = {
    BAUD_RATE_CFG_TABLE(BAUD_RATE_CFG_EXPAND_AS_MDR_DATA)
};

static const u32 gDIVdata[] = {
    BAUD_RATE_CFG_TABLE(BAUD_RATE_CFG_EXPAND_AS_DIV_DATA)
};

/*======================================================  GLOBAL VARIABLES  ==*/

const u32 gPortIOmap[] = {
    UART_DATA_TABLE(UART_DATA_EXPAND_AS_MEM)
};

const u32 gPortIRQ[] = {
    UART_DATA_TABLE(UART_DATA_EXPAND_AS_IRQ)
};

const u32 gPortUartNum = LAST_UART_ENTRY;

/*============================================  LOCAL FUNCTION DEFINITIONS  ==*/

static u32 baudRateCfgFindIndex(
    u32                 baudrate) {

    u32                 cnt;

    LOG_DBG("finding index");
    cnt = 0U;

    while ((0U != gBaudRateData[cnt]) && (baudrate != gBaudRateData[cnt])) {
        cnt++;
    }

    if (0U == gBaudRateData[cnt]) {
        LOG_DBG("index not found");

        return (RETVAL_FAILURE);
    } else {
        LOG_DBG("found index %d", cnt);

        return (cnt);
    }
}

static void platCleanup(
    struct devRes *     devRes,
    enum platState      state) {

    switch (state) {

        case PLAT_STATE_ENABLE : {
            LOG_INFO("reversing action: enable device clocks");
            omap_device_disable_clocks(
                to_omap_device(devRes->platDev));
            /* fall down */
        }

        case PLAT_STATE_ECLK : {
            LOG_INFO("reversing action: build device");
            omap_device_delete(
                to_omap_device(devRes->platDev));
            /* fall down */
        }

        case PLAT_STATE_BUILD : {
            LOG_INFO("reversing action: device lookup");
            /* fall down */
        }

        case PLAT_STATE_LOOKUP : {
            LOG_INFO("reversing action: init");
            break;
        }

        default : {
            /* nothing */
        }
    }
}

/*===================================  GLOBAL PRIVATE FUNCTION DEFINITIONS  ==*/
/*====================================  GLOBAL PUBLIC FUNCTION DEFINITIONS  ==*/

void * portInit(
    u32                 id) {

    struct devRes *     devRes;
    struct omap_hwmod * hwmod;
    enum platState      state;
    int                 retval;
    char                uartName[DEF_UART_NAME_MAX_SIZE + 1U];
    char                hwmodUartName[DEF_UART_NAME_MAX_SIZE + 1U];

    /*-- Initializaion state -------------------------------------------------*/
    state = PLAT_STATE_INIT;
    devRes = kmalloc(
        sizeof(struct devRes),
        GFP_KERNEL);

    if (NULL == devRes) {
        LOG_ERR("OMAP UART: failed to allocate device resources struct");

        return (NULL);
    }
    scnprintf(
        uartName,
        DEF_UART_NAME_MAX_SIZE,
        DEF_UART_NAME "%d",
        id);                                                                    /* NOTE: Since hwmod UART count is messed up we need the right name now */
    LOG_DBG("OMAP UART: creating %s device", uartName);

    /*-- HWMOD lookup state --------------------------------------------------*/
    state = PLAT_STATE_LOOKUP;
    scnprintf(
        hwmodUartName,
        DEF_UART_NAME_MAX_SIZE,
        DEF_UART_NAME "%d",
        id + 1U);                                                               /* NOTE: hwmod UART count starts at 1, so we must add 1 here */
    hwmod = omap_hwmod_lookup(
        hwmodUartName);

    if (NULL == hwmod) {
        LOG_ERR("OMAP UART: failed to find HWMOD %s device", uartName);
        platCleanup(
            devRes,
            state);

        return (NULL);
    }

    /*-- Device build state --------------------------------------------------*/
    state = PLAT_STATE_BUILD;
    devRes->platDev = omap_device_build(
        uartName,
        id,
        hwmod,
        NULL,
        0,
        NULL,
        0,
        0);

    if (NULL == devRes->platDev) {
        LOG_ERR("OMAP UART: failed to build device");
        platCleanup(
            devRes,
            state);

        return (NULL);
    }

    /*-- Device enable clocks state ------------------------------------------*/
    state = PLAT_STATE_ECLK;
    retval = omap_device_enable_clocks(
        to_omap_device(devRes->platDev));

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("OMAP UART: failed to enable device clocks");
        platCleanup(
            devRes,
            state);

        return (NULL);
    }

    /*-- Device enable state -------------------------------------------------*/
    state = PLAT_STATE_ENABLE;
    retval = omap_device_enable(
        devRes->platDev);

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("OMAP UART: failed to enable device");
        platCleanup(
            devRes,
            state);

        return (NULL);
    }

    /*-- Saving references to device data ------------------------------------*/
    devRes->io = omap_device_get_rt_va(
        to_omap_device(devRes->platDev));

    return (devRes);
}

volatile u8 * portIORemapGet(
    void *              devResource) {

    struct devRes *     devRes;
    volatile u8 *       ioremap;

    devRes = (struct devRes *)devResource;
    ioremap = omap_device_get_rt_va(
        to_omap_device(devRes->platDev));

    return (ioremap);
}

/*NOTE:     This function should release all used resources, unfortunately TI
 *          has really dropped the ball here because they did NOT implemented
 *          release functions like they should have.
 *          Currently some resource are released (mem, iomap, VFS files) but
 *          some are never released (clock, clk_alias). Fortunately, reinserting
 *          the module will only generate some warnings, so everything should be
 *          just fine.
 */
int portTerm(
    void *              devResource) {

    struct devRes *     devRes;
    int                 retval;

    devRes = (struct devRes *)devResource;
    LOG_DBG("OMAP UART: destroying device");
    retval = omap_device_shutdown(
        devRes->platDev);
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "OMAP UART: failed to shutdown device");
    retval = omap_device_disable_clocks(
        to_omap_device(devRes->platDev));
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "OMAP UART: failed to disable device clocks");
    omap_device_delete(
        to_omap_device(devRes->platDev));
    platform_device_unregister(
        devRes->platDev);
    kfree(
        devResource);

    return (retval);
}

int portDMAInit(
    struct uartCtx *    uartCtx) {

    return (RETVAL_SUCCESS);
}

int portDMATerm(
    struct uartCtx *    uartCtx) {

    return (RETVAL_SUCCESS);
}

enum lldMode portModeGet(
    u32                 baudrate) {

    u32                 indx;

    indx = baudRateCfgFindIndex(baudrate);

    if (RETVAL_FAILURE == indx) {

        return (indx);
    } else {

        return (gModeData[indx]);
    }
}

u32 portDIVdataGet(
    u32                 baudrate) {

    u32                 indx;

    indx = baudRateCfgFindIndex(baudrate);

    if (RETVAL_FAILURE == indx) {

        return (indx);
    } else {

        return (gDIVdata[indx]);
    }
}

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of plat_omap.c
 ******************************************************************************/
