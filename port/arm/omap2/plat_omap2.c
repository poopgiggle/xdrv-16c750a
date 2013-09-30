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

#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>
#include <plat/dma.h>

#include <rtdm/rtdm_driver.h>

#include "drv/x-16c750.h"
#include "drv/x-16c750_cfg.h"
#include "drv/x-16c750_lld.h"
#include "port/port.h"
#include "dbg/dbg.h"
#include "plat_omap2.h"
#include "log.h"

/*=========================================================  LOCAL MACRO's  ==*/

/**@brief       HWMOD UART name
 */
#define DEF_OMAP_UART_NAME              "uart"
#define DEF_UART_NAME_MAX_SIZE          10

#define BAUD_RATE_CFG_EXPAND_AS_BAUD(a, b, c)                                   \
    a,

#define BAUD_RATE_CFG_EXPAND_AS_MDR_DATA(a, b, c)                               \
    b,

#define BAUD_RATE_CFG_EXPAND_AS_DIV_DATA(a, b, c)                               \
    c,

#define DEVDATA_SIGNATURE               0xdead0bee

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

struct devData {
    volatile uint8_t *  ioRemap;
    volatile uint8_t *  ioPhy;
    struct platform_device * platDev;

#if (1 == CFG_DMA_ENABLE)
    struct {
        struct {
            rtdm_lock_t         lock;
            struct resource *   res;
            uint8_t *           buffRemap;
            volatile uint8_t *  buffPhy;
            size_t              buffSize;
            int                 chn;
            bool                actv;
            void (* callback)(void *);
            void *              data;
        }                   dma;
    }                   rx, tx;
    uint32_t            signature;
#endif
};

/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/

static void platCleanup(
    struct devData *    devData,
    enum platState      state);

static uint32_t baudRateCfgFindIndex(
    uint32_t            baudrate);

#if (1 == CFG_DMA_ENABLE)
static void portDMATxCallback(
    int                 chn,
    unsigned short int  status,
    void *              data);

static void portDMARxCallback(
    int                 chn,
    unsigned short int  status,
    void *              data);
#endif

/*=======================================================  LOCAL VARIABLES  ==*/

static const uint32_t BaudRateData[] = {
    BAUD_RATE_CFG_TABLE(BAUD_RATE_CFG_EXPAND_AS_BAUD)
    0
};

static const enum lldMode ModeData[] = {
    BAUD_RATE_CFG_TABLE(BAUD_RATE_CFG_EXPAND_AS_MDR_DATA)
};

static const uint32_t DIVdata[] = {
    BAUD_RATE_CFG_TABLE(BAUD_RATE_CFG_EXPAND_AS_DIV_DATA)
};

DECL_MODULE_INFO("plat_omap2", "Platform OMAP2", "Nenad Radulovic");

/*======================================================  GLOBAL VARIABLES  ==*/

const uint32_t PortIOmap[] = {
    UART_DATA_TABLE(UART_DATA_EXPAND_AS_MEM)
};

const uint32_t PortIRQ[] = {
    UART_DATA_TABLE(UART_DATA_EXPAND_AS_IRQ)
};

const uint32_t PortUartNum = LAST_UART_ENTRY;

/*============================================  LOCAL FUNCTION DEFINITIONS  ==*/

static uint32_t baudRateCfgFindIndex(
    uint32_t                 baudrate) {

    uint32_t                 cnt;

    LOG_DBG("finding index");
    cnt = 0U;

    while ((0U != BaudRateData[cnt]) && (baudrate != BaudRateData[cnt])) {
        cnt++;
    }

    if (0U == BaudRateData[cnt]) {
        LOG_DBG("index not found");

        return (RETVAL_FAILURE);
    } else {
        LOG_DBG("found index %d", cnt);

        return (cnt);
    }
}

static void platCleanup(
    struct devData *    devData,
    enum platState      state) {

    switch (state) {

        case PLAT_STATE_ENABLE : {
            LOG_INFO("reversing action: enable device clocks");
            omap_device_disable_clocks(
                to_omap_device(devData->platDev));
            /* fall down */
        }

        case PLAT_STATE_ECLK : {
            LOG_INFO("reversing action: build device");
            omap_device_delete(
                to_omap_device(devData->platDev));
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

#if (1 == CFG_DMA_ENABLE)
static void portDMATxCallback(
    int                 chn,
    unsigned short int  status,
    void *              data) {

    struct devData *    devData;
    void (* callback)(void *);
    void *              cbData;

    LOG_INFO("tx callback");
    devData = (struct devData *)data;
    (void)portDMATxStopI(
        devData);
    callback = devData->rx.dma.callback;
    cbData   = devData->rx.dma.data;

    if (NULL != callback) {
        callback(cbData);
    }
}

static void portDMARxCallback(
    int                 chn,
    unsigned short int  status,
    void *              data) {

    struct devData *    devData;
    void (* callback)(void *);
    void *              cbData;

    LOG_INFO("rx callback");
    devData = (struct devData *)data;
    (void)portDMARxStopI(
        devData);
    callback = devData->rx.dma.callback;
    cbData   = devData->rx.dma.data;

    if (NULL != callback) {
        callback(cbData);
    }
}
#endif

/*===================================  GLOBAL PRIVATE FUNCTION DEFINITIONS  ==*/
/*====================================  GLOBAL PUBLIC FUNCTION DEFINITIONS  ==*/

struct devData * portInit(
    uint32_t            id) {

    struct devData *    devData;
    struct omap_hwmod * hwmod;
    enum platState      state;
    int                 retval;
    char                uartName[DEF_UART_NAME_MAX_SIZE + 1U];
    char                hwmodUartName[DEF_UART_NAME_MAX_SIZE + 1U];

    /*-- Initializaion state -------------------------------------------------*/
    state = PLAT_STATE_INIT;
    devData = kmalloc(
        sizeof(struct devData),
        GFP_KERNEL);

    if (NULL == devData) {
        LOG_ERR("OMAP UART: failed to allocate device resources struct");

        return (NULL);
    }
    scnprintf(
        uartName,
        DEF_UART_NAME_MAX_SIZE,
        DEF_OMAP_UART_NAME "%d",
        id);                                                                    /* NOTE: Since hwmod UART count is messed up we need the right name now */
    LOG_DBG("OMAP UART: creating %s device", uartName);

    /*-- HWMOD lookup state --------------------------------------------------*/
    state = PLAT_STATE_LOOKUP;
    scnprintf(
        hwmodUartName,
        DEF_UART_NAME_MAX_SIZE,
        DEF_OMAP_UART_NAME "%d",
        id + 1U);                                                               /* NOTE: hwmod UART count starts at 1, so we must add 1 here */
    hwmod = omap_hwmod_lookup(
        hwmodUartName);

    if (NULL == hwmod) {
        LOG_ERR("OMAP UART: failed to find HWMOD %s device", uartName);
        platCleanup(
            devData,
            state);

        return (NULL);
    }

    /*-- Device build state --------------------------------------------------*/
    state = PLAT_STATE_BUILD;
    devData->platDev = omap_device_build(
        uartName,
        id,
        hwmod,
        NULL,
        0,
        NULL,
        0,
        0);

    if (NULL == devData->platDev) {
        LOG_ERR("OMAP UART: failed to build device");
        platCleanup(
            devData,
            state);

        return (NULL);
    }

    /*-- Device enable clocks state ------------------------------------------*/
    state = PLAT_STATE_ECLK;
    retval = omap_device_enable_clocks(
        to_omap_device(devData->platDev));

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("OMAP UART: failed to enable device clocks");
        platCleanup(
            devData,
            state);

        return (NULL);
    }

    /*-- Device enable state -------------------------------------------------*/
    state = PLAT_STATE_ENABLE;
    retval = omap_device_enable(
        devData->platDev);

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("OMAP UART: failed to enable device");
        platCleanup(
            devData,
            state);

        return (NULL);
    }

    /*-- Saving references to device data ------------------------------------*/
    devData->ioRemap = omap_device_get_rt_va(
        to_omap_device(devData->platDev));
    devData->ioPhy = (volatile uint8_t *)PortIOmap[id];

    ES_DBG_API_OBLIGATION(devData->signature = DEVDATA_SIGNATURE);
    /*
     * NOTE: Board initialization code should setup MUX accordingly
     */

    return (devData);
}

volatile uint8_t * portIORemapGet(
    struct devData *    devData) {

    volatile uint8_t *  ioremap;

    ioremap = omap_device_get_rt_va(
        to_omap_device(devData->platDev));

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
    struct devData *    devData) {

    int                 retval;

    LOG_DBG("OMAP UART: destroying device");
    retval = omap_device_shutdown(
        devData->platDev);
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "OMAP UART: failed to shutdown device, err: %d", retval);
    retval = omap_device_disable_clocks(
        to_omap_device(devData->platDev));
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "OMAP UART: failed to disable device clocks, err: %d", retval);
    omap_device_delete(
        to_omap_device(devData->platDev));
    platform_device_unregister(
        devData->platDev);
    kfree(
        devData);

    return (retval);
}

enum lldMode portModeGet(
    uint32_t            baudrate) {

    uint32_t            indx;

    indx = baudRateCfgFindIndex(baudrate);

    if (RETVAL_FAILURE == indx) {

        return (indx);
    } else {

        return (ModeData[indx]);
    }
}

uint32_t portDIVdataGet(
    uint32_t            baudrate) {

    uint32_t            indx;

    indx = baudRateCfgFindIndex(baudrate);

    if (RETVAL_FAILURE == indx) {

        return (indx);
    } else {

        return (DIVdata[indx]);
    }
}

bool_T portIsOnline(
    uint32_t            id) {

    bool_T              ans;

    /*
     * TODO: This function should check if UART is not managed by Linux kernel
     */
    if (3 == id) {
        ans = TRUE;
    } else {
        ans = FALSE;
    }

    return (ans);
}

#if (1 == CFG_DMA_ENABLE)

int portDMARxInit(
    struct devData *    devData,
    uint8_t **          buff,
    size_t              size) {

    struct platform_device * platDev;

    ES_DBG_API_REQUIRE(ES_DBG_OBJECT_NOT_VALID, devData->signature == DEVDATA_SIGNATURE);

    platDev = devData->platDev;

    LOG_INFO("rx init");
    devData->rx.dma.res = platform_get_resource_byname(
        platDev,
        IORESOURCE_DMA,
        "rx");

    if (NULL == devData->rx.dma.res) {
        LOG_ERR("failed to obtain DMA RX resource, err: %d", -EINVAL);

        return (-EINVAL);
    }

    rtdm_lock_init(&devData->rx.dma.lock);
    devData->rx.dma.chn = -1;
    devData->rx.dma.actv = false;

    /* Mozda prvi argument treba da bude NULL:
     * @dev: valid struct device pointer, or NULL for ISA and EISA-like devices
     */
    devData->rx.dma.buffRemap = dma_alloc_coherent(
        NULL,
        size,
        (dma_addr_t *)&devData->rx.dma.buffPhy,
        0);

    if ((NULL == devData->rx.dma.buffRemap) || (NULL == devData->rx.dma.buffPhy)) {

        return (-ENOMEM);
    }
    *buff = devData->rx.dma.buffRemap;
    devData->rx.dma.buffSize = size;

    LOG_INFO("rx init: res name : %s", devData->rx.dma.res->name);
    LOG_INFO("rx init: res start: %x", devData->rx.dma.res->start);
    LOG_INFO("rx init: res stop : %x", devData->rx.dma.res->end);

    if (devData->rx.dma.res->parent) {
        LOG_INFO("rx init: res parent");
        LOG_INFO("rx init: res name : %s", devData->rx.dma.res->parent->name);
        LOG_INFO("rx init: res start: %x", devData->rx.dma.res->parent->start);
        LOG_INFO("rx init: res stop : %x", devData->rx.dma.res->parent->end);
    }

    if (devData->rx.dma.res->sibling) {
        LOG_INFO("rx init: res parent");
        LOG_INFO("rx init: res name : %s", devData->rx.dma.res->sibling->name);
        LOG_INFO("rx init: res start: %x", devData->rx.dma.res->sibling->start);
        LOG_INFO("rx init: res stop : %x", devData->rx.dma.res->sibling->end);
    }
    LOG_INFO("rx init: BUFF remap: %p", devData->rx.dma.buffRemap);
    LOG_INFO("rx init: BUFF phy  : %p", devData->rx.dma.buffPhy);
    LOG_INFO("rx init: BUFF size : %d", devData->rx.dma.buffSize);

    return (RETVAL_SUCCESS);
}

int portDMARxTerm(
    struct devData *    devData) {

    rtdm_lockctx_t      lockCtx;
    struct platform_device * platDev;

    ES_DBG_API_REQUIRE(ES_DBG_OBJECT_NOT_VALID, devData->signature == DEVDATA_SIGNATURE);

    platDev = devData->platDev;

    rtdm_lock_get_irqsave(&devData->rx.dma.lock, lockCtx);
    (void)portDMARxStopI(
        devData);
    rtdm_lock_put_irqrestore(&devData->rx.dma.lock, lockCtx);
    dma_free_coherent(
        NULL,
        devData->rx.dma.buffSize,
        devData->rx.dma.buffRemap,
        (dma_addr_t)devData->rx.dma.buffPhy);

    return (RETVAL_SUCCESS);
}

int portDMARxStart(
    struct devData *    devData,
    uint8_t *           buff,
    size_t              size,
    void (* callback)(void *),
    void *              data) {

    ptrdiff_t           pos;

    LOG_INFO("rx start: buff: %p", buff);
    LOG_INFO("rx start: size: %d", size);

    ES_DBG_API_REQUIRE(ES_DBG_OBJECT_NOT_VALID, devData->signature == DEVDATA_SIGNATURE);

    if (false == devData->rx.dma.actv) {
        int             retval;

        devData->rx.dma.actv = true;
        LOG_INFO("rx start: req start: %x", devData->rx.dma.res->start);
        retval = omap_request_dma(
            71,
            CFG_DRV_NAME " Rx DMA",
            portDMARxCallback,
            devData,
            &devData->rx.dma.chn);
        LOG_INFO("rx start: req retval %d", retval);
        LOG_INFO("rx start: req chn    %d", devData->rx.dma.chn);

        if (0 != retval) {
            LOG_ERR("Rx DMA request failed, err: %d", retval);

            return (retval);
        }
        LOG_INFO("rx start: set src    %p", devData->ioPhy);
        omap_set_dma_src_params(
            devData->rx.dma.chn,
            0,
            OMAP_DMA_AMODE_CONSTANT,
            (unsigned long int)(devData->ioPhy + RHR),
            0,
            0);
    }
    devData->rx.dma.callback = callback;
    devData->rx.dma.data = data;
    pos = buff - devData->rx.dma.buffRemap;
    LOG_INFO("rx start: pos        %d", pos);
    LOG_INFO("rx start: set dst    %p", devData->rx.dma.buffPhy + pos);
    omap_set_dma_dest_params(
        devData->rx.dma.chn,
        0,
        OMAP_DMA_AMODE_POST_INC,
        (unsigned long int)(devData->rx.dma.buffPhy + pos),
        0,
        0);
    omap_set_dma_transfer_params(
        devData->rx.dma.chn,
        OMAP_DMA_DATA_TYPE_S8,
        size,
        1,
        OMAP_DMA_SYNC_ELEMENT,
        devData->rx.dma.res->start,
        0);
    omap_start_dma(
        devData->rx.dma.chn);

    return (RETVAL_SUCCESS);
}

int portDMARxStopI(
    struct devData *    devData) {

    LOG_INFO("rx stop");
    ES_DBG_API_REQUIRE(ES_DBG_OBJECT_NOT_VALID, devData->signature == DEVDATA_SIGNATURE);

    if (true == devData->rx.dma.actv) {
        devData->rx.dma.actv = false;
        omap_stop_dma(
            devData->rx.dma.chn);
        omap_free_dma(
            devData->rx.dma.chn);
    }

    return (RETVAL_SUCCESS);
}

int portDMATxInit(
    struct devData *    devData,
    uint8_t **          buff,
    size_t              size) {

    struct platform_device * platDev;

    platDev = devData->platDev;

    LOG_INFO("DMA Tx init");
    devData->tx.dma.res = platform_get_resource_byname(
        platDev,
        IORESOURCE_DMA,
        "tx");

    if (NULL == devData->tx.dma.res) {
        LOG_ERR("failed to obtain DMA TX resource");

        return (-EINVAL);
    }
    rtdm_lock_init(&devData->tx.dma.lock);
    devData->tx.dma.chn = -1;
    devData->tx.dma.actv = false;

    /* Mozda prvi argument treba da bude NULL:
     * @dev: valid struct device pointer, or NULL for ISA and EISA-like devices
     */
    devData->tx.dma.buffRemap = dma_alloc_coherent(
        NULL,
        size,
        (dma_addr_t *)&devData->tx.dma.buffPhy,
        0);

    if ((NULL == devData->tx.dma.buffRemap) || (NULL == devData->tx.dma.buffPhy)) {
        LOG_ERR("DMA Tx failed to alloc memory");

        return (-ENOMEM);
    }
    *buff = devData->tx.dma.buffRemap;
    devData->tx.dma.buffSize = size;

    LOG_INFO("tx init: res name : %s", devData->tx.dma.res->name);
    LOG_INFO("tx init: res start: %x", devData->tx.dma.res->start);
    LOG_INFO("tx init: res stop : %x", devData->tx.dma.res->end);

    if (devData->tx.dma.res->parent) {
        LOG_INFO("tx init: res parent");
        LOG_INFO("tx init: res name : %s", devData->tx.dma.res->parent->name);
        LOG_INFO("tx init: res start: %x", devData->tx.dma.res->parent->start);
        LOG_INFO("tx init: res stop : %x", devData->tx.dma.res->parent->end);
    }

    if (devData->tx.dma.res->sibling) {
        LOG_INFO("tx init: res parent");
        LOG_INFO("tx init: res name : %s", devData->tx.dma.res->sibling->name);
        LOG_INFO("tx init: res start: %x", devData->tx.dma.res->sibling->start);
        LOG_INFO("tx init: res stop : %x", devData->tx.dma.res->sibling->end);
    }
    LOG_INFO("tx init: BUFF remap: %p", devData->tx.dma.buffRemap);
    LOG_INFO("tx init: BUFF phy  : %p", devData->tx.dma.buffPhy);
    LOG_INFO("tx init: BUFF size : %d", devData->tx.dma.buffSize);

    return (RETVAL_SUCCESS);
}

int portDMATxTerm(
    struct devData *    devData) {

    rtdm_lockctx_t      lockCtx;
    struct platform_device * platDev;

    platDev = devData->platDev;

    rtdm_lock_get_irqsave(&devData->tx.dma.lock, lockCtx);
    (void)portDMATxStopI(
        devData);
    rtdm_lock_put_irqrestore(&devData->tx.dma.lock, lockCtx);
    dma_free_coherent(
        NULL,
        devData->tx.dma.buffSize,
        devData->tx.dma.buffRemap,
        (dma_addr_t)devData->tx.dma.buffPhy);

    return (RETVAL_SUCCESS);
}

int portDMATxStart(
    struct devData *    devData,
    const uint8_t *     buff,
    size_t              size,
    void (* callback)(void *),
    void *              data) {

    ptrdiff_t           pos;

    LOG_INFO("tx start: buff: %p", buff);
    LOG_INFO("tx start: size: %d", size);

    ES_DBG_API_REQUIRE(ES_DBG_OBJECT_NOT_VALID, devData->signature == DEVDATA_SIGNATURE);

    if (false == devData->tx.dma.actv) {
        int             retval;

        devData->tx.dma.actv = true;
        LOG_INFO("tx start: req start: %x", devData->tx.dma.res->start);
        retval = omap_request_dma(
            70,
            CFG_DRV_NAME " Tx DMA",
            portDMATxCallback,
            devData,
            &devData->tx.dma.chn);
        LOG_INFO("rx start: req retval %d", retval);
        LOG_INFO("rx start: req chn    %d", devData->tx.dma.chn);

        if (0 != retval) {
            LOG_ERR("Tx DMA request failed, err: %d", retval);

            return (retval);
        }
        LOG_INFO("tx start: set dst    %p", devData->ioPhy);
        omap_set_dma_dest_params(
            devData->tx.dma.chn,
            0,
            OMAP_DMA_AMODE_CONSTANT,
            (unsigned long int)(devData->ioPhy + wTHR),
            0,
            0);
    }
    devData->tx.dma.callback = callback;
    devData->tx.dma.data = data;
    pos = buff - devData->tx.dma.buffRemap;
    LOG_INFO("tx start: pos        %d", pos);
    LOG_INFO("tx start: set src    %p", devData->tx.dma.buffPhy + pos);
    omap_set_dma_src_params(
        devData->tx.dma.chn,
        0,
        OMAP_DMA_AMODE_POST_INC,
        (unsigned long int)(devData->tx.dma.buffPhy + pos),
        0,
        0);
    omap_set_dma_transfer_params(
        devData->tx.dma.chn,
        OMAP_DMA_DATA_TYPE_S8,
        size,
        1,
        OMAP_DMA_SYNC_ELEMENT,
        devData->tx.dma.res->start,
        0);
    omap_start_dma(
        devData->tx.dma.chn);

    return (RETVAL_SUCCESS);
}

int portDMATxStopI(
    struct devData *    devData) {

    if (true == devData->tx.dma.actv) {

        if (omap_get_dma_active_status(devData->tx.dma.chn)) {

            return (-EBUSY);
        }
        devData->tx.dma.actv = false;
        omap_stop_dma(
            devData->tx.dma.chn);
        omap_free_dma(
            devData->tx.dma.chn);
    }

    return (RETVAL_SUCCESS);
}
#endif

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of plat_omap.c
 ******************************************************************************/
