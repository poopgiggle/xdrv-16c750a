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
#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>
#include <mach/edma.h>

#include <rtdm/rtdm_driver.h>

#include "drv/x-16c750.h"
#include "drv/x-16c750_cfg.h"
#include "drv/x-16c750_lld.h"
#include "port/port.h"
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

struct hwAddr {
    volatile uint8_t *  phy;
    uint8_t *           remap;
};

struct devData {
    struct hwAddr       ioAddr;
    struct platform_device * platDev;

#if (1 == CFG_DMA_MODE) || (2 == CFG_DMA_MODE)
    struct {
        struct {
#if (2 == CFG_DMA_MODE)
            struct resource *       res;
#endif
            rtdm_lock_t             lock;
            struct hwAddr           buffAddr;
            size_t                  buffSize;
            int                     chn;
            bool                    actv;
            bool                    run;
            void (* callback)(void *);
            void *                  arg;
        }                   dma;
    }                   rx, tx;
#endif
};

/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/

static void platCleanup(
    struct devData *    devData,
    enum platState      state);

static int32_t baudRateCfgFindIndex(
    uint32_t            baudrate);

#if (1 == CFG_DMA_MODE) || (2 == CFG_DMA_MODE)
static void portDMATxCallbackI(
    unsigned int        chn,
    unsigned short int  status,
    void *              data);

static void portDMARxCallbackI(
    unsigned int        chn,
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

/*======================================================  GLOBAL VARIABLES  ==*/

const uint32_t PortIOmap[] = {
    UART_DATA_TABLE(UART_DATA_EXPAND_AS_MEM)
};

const uint32_t PortIRQ[] = {
    UART_DATA_TABLE(UART_DATA_EXPAND_AS_IRQ)
};

const uint32_t PortUartNum = LAST_UART_ENTRY;

/*============================================  LOCAL FUNCTION DEFINITIONS  ==*/

static int32_t baudRateCfgFindIndex(
    uint32_t                 baudrate) {

    uint32_t                 cnt;

    LOG_DBG("finding index");
    cnt = 0U;

    while ((0U != BaudRateData[cnt]) && (baudrate != BaudRateData[cnt])) {
        cnt++;
    }

    if (0U == BaudRateData[cnt]) {
        LOG_DBG("index not found");

        return (-ENXIO);
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
            LOG_INFO("PORT: reversing action: enable device clocks");
            omap_device_disable_clocks(
                to_omap_device(devData->platDev));
        } /* fall down */
        case PLAT_STATE_ECLK : {
            LOG_INFO("PORT: reversing action: build device");
            omap_device_delete(
                to_omap_device(devData->platDev));
        } /* fall down */
        case PLAT_STATE_BUILD : {
            LOG_INFO("PORT: reversing action: device lookup");
        } /* fall down */
        case PLAT_STATE_LOOKUP : {
            LOG_INFO("PORT: reversing action: init");
            break;
        }
        default : {
            break;
        }
    }
}

#if (1 == CFG_DMA_MODE) || (2 == CFG_DMA_MODE)
static void portDMATxCallbackI(
    unsigned int        chn,
    unsigned short int  status,
    void *              data) {

    struct devData *    devData;

    LOG_DBG("DMA: tx callback");
    devData = (struct devData *)data;
    devData->tx.dma.run = false;

    if (NULL != devData->tx.dma.callback) {
        devData->tx.dma.callback(devData->tx.dma.arg);
    }
}

static void portDMARxCallbackI(
    unsigned int        chn,
    unsigned short int  status,
    void *              data) {

    struct devData *    devData;

    LOG_DBG("DMA: rx callback");
    devData = (struct devData *)data;
    devData->tx.dma.run = false;

    if (NULL != devData->rx.dma.callback) {
        devData->rx.dma.callback(devData->rx.dma.arg);
    }
}
#endif

/* For debugging only */
#if 0
static void printPaRAM(uint32_t chn) {
    struct edmacc_param paramSet;

    edma_read_slot(chn, &paramSet);
    LOG_INFO(" OPT     : %x", paramSet.opt);
    LOG_INFO(" SRC     : %p", paramSet.src);
    LOG_INFO(" BCNT    : %d", paramSet.a_b_cnt >> 16U);
    LOG_INFO(" ACNT    : %d", paramSet.a_b_cnt && 0x00FFU);
    LOG_INFO(" DST     : %p", paramSet.dst);
    LOG_INFO(" DSTBIDX : %d", paramSet.src_dst_bidx >> 16U);
    LOG_INFO(" SRCBIDX : %d", paramSet.src_dst_bidx && 0x00FFU);
    LOG_INFO(" BCNTRLD : %d", paramSet.link_bcntrld >> 16U);
    LOG_INFO(" LINK    : %x", paramSet.link_bcntrld && 0x00FFU);
    LOG_INFO(" DSTCIDX : %d", paramSet.src_dst_cidx >> 16U);
    LOG_INFO(" SRCCIDX : %d", paramSet.src_dst_cidx && 0x00FFU);
    LOG_INFO(" CCNT    : %d", paramSet.ccnt && 0x00FFU);
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

    if (0 != retval) {
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

    if (0 != retval) {
        LOG_ERR("OMAP UART: failed to enable device");
        platCleanup(
            devData,
            state);

        return (NULL);
    }

    /*-- Saving references to device data ------------------------------------*/
    devData->ioAddr.remap = omap_device_get_rt_va(
        to_omap_device(devData->platDev));
    devData->ioAddr.phy = (volatile uint8_t *)PortIOmap[id];

    /*
     * XXX, NOTE: Board initialization code should setup MUX accordingly
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

    if (0 != retval) {
        LOG_ERR("OMAP UART: failed to shutdown device, err: %d", retval);
    }
    retval = omap_device_disable_clocks(
        to_omap_device(devData->platDev));

    if (0 != retval) {
        LOG_ERR("OMAP UART: failed to disable device clocks, err: %d", retval);
    }
    omap_device_delete(
        to_omap_device(devData->platDev));
    platform_device_unregister(
        devData->platDev);
    kfree(
        devData);

    return (retval);
}

int32_t portModeGet(
    uint32_t            baudrate) {

    uint32_t            indx;

    indx = baudRateCfgFindIndex(baudrate);

    if (0 > indx) {

        return (indx);
    } else {

        return ((int32_t)ModeData[indx]);
    }
}

int32_t portDIVdataGet(
    uint32_t            baudrate) {

    uint32_t            indx;

    indx = baudRateCfgFindIndex(baudrate);

    if (0 > indx) {

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

#if (1 == CFG_DMA_MODE) || (2 == CFG_DMA_MODE)

int32_t portDMARxInit(
    struct devData *    devData,
    uint8_t **          buff,
    size_t              size) {

#if (2 == CFG_DMA_MODE)
    struct platform_device * platDev;

    platDev = devData->platDev;

    devData->rx.dma.res = platform_get_resource_byname(
        platDev,
        IORESOURCE_DMA,
        "rx");

    if (NULL == devData->rx.dma.res) {
        LOG_ERR("failed to obtain DMA RX resource, err: %d", -EINVAL);

        return (-EINVAL);
    }
#endif
    rtdm_lock_init(&devData->rx.dma.lock);
    devData->rx.dma.chn = -1;
    devData->rx.dma.actv = false;
    devData->rx.dma.run = false;
    *buff = devData->rx.dma.buffAddr.remap;
    devData->rx.dma.buffSize = size;

    return (0);
}

int32_t portDMARxTerm(
    struct devData *    devData) {

    rtdm_lockctx_t      lockCtx;

    rtdm_lock_get_irqsave(&devData->rx.dma.lock, lockCtx);
    (void)portDMARxStopI(
        devData);
    rtdm_lock_put_irqrestore(&devData->rx.dma.lock, lockCtx);

    return (0);
}

int32_t portDMARxStart(
    struct devData *    devData,
    uint8_t *           dst,
    size_t              size,
    void (* callback)(void *),
    void *              arg) {

    struct edmacc_param paramSet;
    rtdm_lockctx_t      lockCtx;
    int32_t             retval;
    ptrdiff_t           pos;

    rtdm_lock_get_irqsave(&devData->rx.dma.lock, lockCtx);

    if (false == devData->rx.dma.actv) {

        devData->rx.dma.actv = true;
        rtdm_lock_put_irqrestore(&devData->rx.dma.lock, lockCtx);
        retval = (int32_t)edma_alloc_channel(
#if (1 == CFG_DMA_MODE)
            EDMA_CHANNEL_ANY,
#elif (2 == CFG_DMA_MODE)
            devData->rx.dma.res->start,
#endif
            portDMARxCallbackI,
            devData,
            EVENTQ_0);

        if (0 > retval) {
            LOG_ERR("DMA: failed to alloc Rx channel, err: %d", retval);

            return (retval);
        }
        devData->rx.dma.chn = retval;
    } else {
        rtdm_lock_put_irqrestore(&devData->rx.dma.lock, lockCtx);

        if (true == devData->rx.dma.run) {

            return (-EBUSY);
        }
    }
    devData->rx.dma.run = true;
    devData->rx.dma.callback = callback;
    devData->rx.dma.arg = arg;
    pos = dst - devData->rx.dma.buffAddr.remap;
    edma_set_src(
        devData->rx.dma.chn,
        (dma_addr_t)devData->ioAddr.phy,
        FIFO,
        W8BIT);
    edma_set_src_index(
        devData->rx.dma.chn,
        0,
        0);
    edma_set_dest_index(
        devData->rx.dma.chn,
        1,
        0);
    edma_read_slot(
        devData->rx.dma.chn,
        &paramSet);
    paramSet.opt |= TCINTEN;
    paramSet.opt |= EDMA_TCC(EDMA_CHAN_SLOT(devData->rx.dma.chn));
    edma_write_slot(
        devData->rx.dma.chn,
        &paramSet);
    edma_set_dest(
        devData->rx.dma.chn,
        (dma_addr_t)(devData->rx.dma.buffAddr.phy + pos),
        INCR,
        W8BIT);
    edma_set_transfer_params(
        devData->rx.dma.chn,
        1,
        size,
        1,
        size,
        ABSYNC);
    retval = (int32_t)edma_start(
        devData->rx.dma.chn);

    if (0 != retval) {
        LOG_ERR("DMA: failed to start Rx channel, err %d", retval);

        return (retval);
    }

    return (0);
}

int32_t portDMARxStopI(
    struct devData *    devData) {

    if (true == devData->rx.dma.actv) {
        devData->rx.dma.actv = false;
        edma_stop(
            devData->rx.dma.chn);
        edma_free_channel(
            devData->rx.dma.chn);
    }

    return (0);
}

int32_t portDMATxInit(
    struct devData *    devData,
    uint8_t **          buff,
    size_t              size) {

#if (2 == CFG_DMA_MODE)
    struct platform_device * platDev;

    platDev = devData->platDev;

    devData->tx.dma.res = platform_get_resource_byname(
        platDev,
        IORESOURCE_DMA,
        "tx");

    if (NULL == devData->tx.dma.res) {
        LOG_ERR("failed to obtain DMA TX resource");

        return (-EINVAL);
    }
#endif
    rtdm_lock_init(&devData->tx.dma.lock);
    devData->tx.dma.chn = -1;
    devData->tx.dma.actv = false;
    devData->tx.dma.run = false;
    *buff = devData->tx.dma.buffAddr.remap;
    devData->tx.dma.buffSize = size;

    return (0);
}

int32_t portDMATxTerm(
    struct devData *    devData) {

    rtdm_lockctx_t      lockCtx;

    rtdm_lock_get_irqsave(&devData->tx.dma.lock, lockCtx);
    (void)portDMATxStopI(
        devData);
    rtdm_lock_put_irqrestore(&devData->tx.dma.lock, lockCtx);

    return (0);
}

int32_t portDMATxStart(
    struct devData *    devData,
    const uint8_t *     src,
    size_t              size,
    void (* callback)(void *),
    void *              arg) {

    struct edmacc_param paramSet;
    rtdm_lockctx_t      lockCtx;
    ptrdiff_t           pos;
    int32_t             retval;

    rtdm_lock_get_irqsave(&devData->tx.dma.lock, lockCtx);

    if (false == devData->tx.dma.actv) {
        devData->tx.dma.actv = true;
        rtdm_lock_put_irqrestore(&devData->tx.dma.lock, lockCtx);

        retval = (int32_t)edma_alloc_channel(
#if (1 == CFG_DMA_MODE)
            EDMA_CHANNEL_ANY,
#elif (2 == CFG_DMA_MODE)
            devData->tx.dma.res->start,
#endif
            portDMATxCallbackI,
            devData,
            EVENTQ_1);

        if (0 > retval) {
            LOG_ERR("DMA: failed to alloc Tx channel, err %d", retval);

            return (retval);
        }
        devData->tx.dma.chn = retval;
        LOG_DBG("DMA: SRC addr: %p", devData->ioAddr.phy);
    } else {
        rtdm_lock_put_irqrestore(&devData->tx.dma.lock, lockCtx);

        if (true == devData->tx.dma.run) {

            return (-EBUSY);
        }
    }
    devData->tx.dma.run = true;
    devData->tx.dma.callback = callback;
    devData->tx.dma.arg = arg;
    pos = src - devData->tx.dma.buffAddr.remap;
    edma_set_dest(
        devData->tx.dma.chn,
        (dma_addr_t)devData->ioAddr.phy,
        FIFO,
        W8BIT);
    edma_set_dest_index(
        devData->tx.dma.chn,
        0,
        0);
    edma_set_src_index(
        devData->tx.dma.chn,
        1,
        0);
    edma_read_slot(
        devData->tx.dma.chn,
        &paramSet);
    paramSet.opt |= TCINTEN;
    paramSet.opt |= EDMA_TCC(EDMA_CHAN_SLOT(devData->tx.dma.chn));
    edma_write_slot(
        devData->tx.dma.chn,
        &paramSet);
    edma_set_src(
        devData->tx.dma.chn,
        (dma_addr_t)(devData->tx.dma.buffAddr.phy + pos),
        INCR,
        W8BIT);
    edma_set_transfer_params(
        devData->tx.dma.chn,
        1,
        size,
        1,
        size,
        ABSYNC);
    retval = (int32_t)edma_start(
        devData->tx.dma.chn);

    if (0 != retval) {
        LOG_ERR("DMA: failed to start Tx channel, err %d", retval);

        return (retval);
    }

    return (0);
}

int portDMATxStopI(
    struct devData *    devData) {

    if (true == devData->tx.dma.actv) {
        devData->tx.dma.actv = false;
        edma_stop(
            devData->tx.dma.chn);
        edma_free_channel(
            devData->tx.dma.chn);
    }

    return (0);
}
#endif

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of plat_omap.c
 ******************************************************************************/
