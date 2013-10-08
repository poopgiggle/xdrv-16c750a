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
#include <linux/ioport.h>

#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>

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

/**@brief       This switch will suppress kernel coding rules violations for
 *              requesting access to IO memory.
 */
#define DEF_SUPPRESS_MEM_REQ_WARNING    1

#define BAUD_RATE_CFG_EXPAND_AS_BAUD(a, b, c)                                   \
    a,

#define BAUD_RATE_CFG_EXPAND_AS_MDR_DATA(a, b, c)                               \
    b,

#define BAUD_RATE_CFG_EXPAND_AS_DIV_DATA(a, b, c)                               \
    c,

#define BIT_EXTRACT(val, bit)                                                   \
    (0x1u & (val >> bit))

#define BIT_RESET(val, bit)                                                     \
    (val & (0x1u << bit))

#define DEVDATA_SIGNATURE               0xDEADBEEA

/*======================================================  LOCAL DATA TYPES  ==*/

/**@brief       Platform setup states
 */
enum portState {
    PLAT_STATE_INIT,                                                            /**<@brief PLAT_STATE_INIT                                  *///!< PLAT_STATE_INIT
    PLAT_STATE_LOOKUP,                                                          /**<@brief PLAT_STATE_LOOKUP                                *///!< PLAT_STATE_LOOKUP
    PLAT_STATE_BUILD,                                                           /**<@brief PLAT_STATE_BUILD                                 *///!< PLAT_STATE_BUILD
    PLAT_STATE_ECLK,                                                            /**<@brief PLAT_STATE_ECLK                                  *///!< PLAT_STATE_ECLK
    PLAT_STATE_ENABLE                                                           /**<@brief PLAT_STATE_ENABLE                                *///!< PLAT_STATE_ENABLE
};

enum uartId {
    UART_DATA_TABLE(UART_DATA_EXPAND_AS_UART)
    LAST_UART_ENTRY
};

enum edmaReg {
    EDMA_IER            = 0x50,
    EDMA_IERH           = 0x54,
    EDMA_IPR            = 0x68,
    EDMA_IPRH           = 0x6c,
    EDMA_ICR            = 0x70,
    EDMA_ICRH           = 0x74
};

struct hwAddr {
    volatile uint8_t *  phy;
    uint8_t *           remap;
    size_t              size;
};

struct devData {
    struct hwAddr       ioAddr;
    struct platform_device * platDev;

#if (1 == CFG_DMA_MODE) || (2 == CFG_DMA_MODE)
    struct dma {
        struct hwAddr       addr;
        struct dmaPerUnit{
            struct edmacc_param param;
            int                 chn;
            bool_T              isRunning;
            void (* callback)(void *);
            void *              arg;
        }                   rx, tx;
        rtdm_irq_t          irqHandle;
    }                   dma;
#endif
    uint32_t            signature;
};

/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/

static int32_t baudRateCfgFindIndex(
    uint32_t            baudrate);

static void portCleanup(
    struct devData *    devData,
    enum portState      state);

#if (1 == CFG_DMA_MODE) || (2 == CFG_DMA_MODE)

#if (1u == CFG_LOG_DBG_ENABLE)
static void printPaRAM(
    uint32_t            chn);
#endif /* (1u == CFG_LOG_DBG_ENABLE) */

static inline uint32_t edmaRd(
    volatile uint8_t *  io,
    enum edmaReg        reg);

static inline void edmaWr(
    volatile uint8_t *  io,
    enum edmaReg        reg,
    uint32_t            val);

static inline uint32_t edmaShRd(
    volatile uint8_t *  io,
    enum edmaReg        reg);

static inline void edmaShWr(
    volatile uint8_t *  io,
    enum edmaReg        reg,
    uint32_t            val);

static void edmaIntrClear(
    volatile uint8_t *  io,
    uint32_t            tcc);

static int edmaHandleIrq(
    rtdm_irq_t *        handle);

static int32_t edmaInit(
    struct devData *    devData);

static int32_t edmaTerm(
    struct devData *    devData);

static void edmaDummyCallback(
    unsigned int        chn,
    uint16_t            status,
    void *              data);
#endif

/*=======================================================  LOCAL VARIABLES  ==*/

DECL_MODULE_INFO("plat_omap2", "Platform port for OMAP2", "Nenad Radulovic");

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

static void portCleanup(
    struct devData *    devData,
    enum portState      state) {

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

#if (1u == CFG_LOG_DBG_ENABLE)

/* For debugging only                                                         */
static void printPaRAM(
    uint32_t            chn) {

    struct edmacc_param paramSet;

    edma_read_slot(chn, &paramSet);
    LOG_INFO(" OPT     : %x", paramSet.opt);
    LOG_INFO(" SRC     : %p", (void *)paramSet.src);
    LOG_INFO(" BCNT    : %d", paramSet.a_b_cnt >> 16U);
    LOG_INFO(" ACNT    : %d", paramSet.a_b_cnt && 0x00FFU);
    LOG_INFO(" DST     : %p", (void *)paramSet.dst);
    LOG_INFO(" DSTBIDX : %d", paramSet.src_dst_bidx >> 16U);
    LOG_INFO(" SRCBIDX : %d", paramSet.src_dst_bidx && 0x00FFU);
    LOG_INFO(" BCNTRLD : %d", paramSet.link_bcntrld >> 16U);
    LOG_INFO(" LINK    : %x", paramSet.link_bcntrld && 0x00FFU);
    LOG_INFO(" DSTCIDX : %d", paramSet.src_dst_cidx >> 16U);
    LOG_INFO(" SRCCIDX : %d", paramSet.src_dst_cidx && 0x00FFU);
    LOG_INFO(" CCNT    : %d", paramSet.ccnt && 0x00FFU);
}
#endif /* (1u == CFG_LOG_DBG_ENABLE) */

static inline uint32_t edmaRd(
    volatile uint8_t *  io,
    enum edmaReg        reg)  {

    uint32_t            retval;

    retval = __raw_readl(io + reg);

    LOG_DBG("EDMA rd: %p, %x : %x", io, reg, retval);

    return (retval);
}

static inline void edmaWr(
    volatile uint8_t *  io,
    enum edmaReg        reg,
    uint32_t            val) {

    LOG_DBG("EDMA wr: %p, %x = %x", io, reg, val);

    __raw_writel(val, io + reg);
}

static inline uint32_t edmaShRd(
    volatile uint8_t *  io,
    enum edmaReg        reg) {

    return (edmaRd(io, EDMA_SH_BASE + reg));
}

static inline void edmaShWr(
    volatile uint8_t *  io,
    enum edmaReg        reg,
    uint32_t            val) {

    edmaWr(
        io,
        EDMA_SH_BASE + reg,
        val);
}

static void edmaIntrClear(
    volatile uint8_t *  io,
    uint32_t            tcc) {

    if (31 < tcc) {
        edmaShWr(io, EDMA_ICRH, 0x1u << (tcc - 32));
    } else {
        edmaShWr(io, EDMA_ICR, 0x1u << tcc);
    }
}

static int edmaHandleIrq(
    rtdm_irq_t *        handle) {

    struct devData *    devData;
    volatile uint8_t *  io;
    uint64_t            ipr;

    LOG_DBG("ISR DMA");
    devData = rtdm_irq_get_arg(handle, struct devData);

    io = devData->dma.addr.remap;
    ipr = ((uint64_t)edmaShRd(io, EDMA_IPRH) << 32u) | (uint64_t)edmaShRd(io, EDMA_IPR);

    if ((uint64_t)0ul != (ipr & ((uint64_t)0x1ul << devData->dma.rx.chn))) {
        edmaShWr(
            io,
            EDMA_ICR,
            (uint32_t)(0xffffu & ipr));
        edmaShWr(
            io,
            EDMA_ICRH,
            (uint32_t)(0xffffu & (ipr >> 32)));
        ipr &= ~((uint64_t)0x1ul << devData->dma.rx.chn);
        devData->dma.rx.isRunning = FALSE;

        if (NULL != devData->dma.rx.callback) {
            devData->dma.rx.callback(devData->dma.rx.arg);
        }
    }

    if ((uint64_t)0ul != (ipr & ((uint64_t)0x1ul << devData->dma.tx.chn))) {
        edmaShWr(
            io,
            EDMA_ICR,
            (uint32_t)(0xffffu & ipr));
        edmaShWr(
            io,
            EDMA_ICRH,
            (uint32_t)(0xffffu & (ipr >> 32)));
        devData->dma.tx.isRunning = FALSE;
        ipr &= ~((uint64_t)0x1ul << devData->dma.tx.chn);

        if (NULL != devData->dma.tx.callback) {
            devData->dma.tx.callback(devData->dma.tx.arg);
        }
    }

    if ((uint64_t)0ul == ipr) {

        return (RTDM_IRQ_HANDLED);
    }

    return (RTDM_IRQ_NONE);
}

static int32_t edmaInit(
    struct devData *    devData) {

    uint32_t            retval;

#if (0 == DEF_SUPPRESS_MEM_REQ_WARNING)
    struct resource *   res;
#endif /* (0 == DEF_SUPPRESS_MEM_REQ_WARNING) */

    devData->dma.addr.phy = (volatile uint8_t *)EDMA_TPCC_BASE;
    devData->dma.addr.size = (size_t)EDMA_TPCC_SIZE;
    LOG_DBG("EDMA mem start: %p", devData->dma.addr.phy);
    LOG_DBG("EDMA mem size : %x", devData->dma.addr.size);

#if (0 == DEF_SUPPRESS_MEM_REQ_WARNING)
    res = request_mem_region(
        (resource_size_t)devData->dma.addr.phy,
        (resource_size_t)devData->dma.addr.size,
        CFG_DRV_NAME ".edma");

    if (NULL == res) {
        LOG_ERR("OMAP UART DMA: failed to request memory, err: %d", ENOMEM);

        return (-ENOMEM);
    }
#endif /* (0 == DEF_SUPPRESS_MEM_REQ_WARNING) */
    devData->dma.addr.remap = ioremap(
        (long unsigned int)devData->dma.addr.phy,
        devData->dma.addr.size);
    LOG_DBG("EDMA mem start (remap): %p", devData->dma.addr.remap);

    if (NULL == devData->dma.addr.remap) {
        LOG_ERR("OMAP UART DMA: failed to remap memory, err: %d", ENOMEM);
#if (0 == DEF_SUPPRESS_MEM_REQ_WARNING)
        release_mem_region(
            (resource_size_t)devData->dma.addr.phy,
            (resource_size_t)devData->dma.addr.size);
#endif /* (0 == DEF_SUPPRESS_MEM_REQ_WARNING) */

        return (-ENOMEM);
    }
    retval = (int32_t)rtdm_irq_request(
        &devData->dma.irqHandle,
        EDMA_COMP_IRQ,
        edmaHandleIrq,
        RTDM_IRQTYPE_EDGE,
        CFG_DRV_NAME " DMA",
        devData);

    if (0 != retval) {
        LOG_ERR("OMAP UART DMA: failed to request DMA IRQ, err: %d", -retval);
        iounmap(
            devData->dma.addr.phy);
#if (0 == DEF_SUPPRESS_MEM_REQ_WARNING)
        release_mem_region(
            (resource_size_t)devData->dma.addr.phy,
            (resource_size_t)devData->dma.addr.size);
#endif /* (0 == DEF_SUPPRESS_MEM_REQ_WARNING) */

        return (retval);
    }

    /*
     * NOTE: Since DMA stop functions are called within module deinitialization
     *       we must initialize these at this time.
     */
    devData->dma.rx.chn = EDMA_CHANNEL_ANY;
    devData->dma.tx.chn = EDMA_CHANNEL_ANY;

    return (0);
}

static int32_t edmaTerm(
    struct devData *    devData) {

    int32_t             retval;

    LOG_DBG("OMAP UART DMA: terminating");

    retval = 0;
    portDMATxStopI(
        devData);
    portDMARxStopI(
        devData);
    retval = (int32_t)rtdm_irq_free(
        &devData->dma.irqHandle);

    if (0 != retval) {
        LOG_ERR("OMAP UART DMA: failed to release IRQ, err: %d", -retval);
    }
    iounmap(
        devData->dma.addr.phy);
#if (0 == DEF_SUPPRESS_MEM_REQ_WARNING)
    release_mem_region(
        (resource_size_t)devData->dma.addr.phy,
        (resource_size_t)devData->dma.addr.size);
#endif /* (0 == DEF_SUPPRESS_MEM_REQ_WARNING) */

    return (retval);
}

/*
 * NOTE:        We need dummy callback function for Linux domain.
 */
static void edmaDummyCallback(
    unsigned int        chn,
    uint16_t            status,
    void *              data) {

    (void)chn;
    (void)status;
    (void)data;
}

#endif /* (1 == CFG_DMA_MODE) || (2 == CFG_DMA_MODE) */

/*===================================  GLOBAL PRIVATE FUNCTION DEFINITIONS  ==*/
/*====================================  GLOBAL PUBLIC FUNCTION DEFINITIONS  ==*/

struct devData * portInit(
    uint32_t            id) {

    struct devData *    devData;
    struct omap_hwmod * hwmod;
    enum portState      state;
    int                 retval;
    char                uartName[DEF_UART_NAME_MAX_SIZE + 1u];
    char                hwmodUartName[DEF_UART_NAME_MAX_SIZE + 1u];

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
        id + 1u);                                                               /* NOTE: hwmod UART count starts at 1, so we must add 1 here */
    hwmod = omap_hwmod_lookup(
        hwmodUartName);

    if (NULL == hwmod) {
        LOG_ERR("OMAP UART: failed to find HWMOD %s device", uartName);
        portCleanup(
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
        portCleanup(
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
        portCleanup(
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
        portCleanup(
            devData,
            state);

        return (NULL);
    }

    /*
     * NOTE: Board initialization code should setup MUX accordingly
     */

    /*-- Saving references to device data ------------------------------------*/
    devData->ioAddr.remap = omap_device_get_rt_va(
        to_omap_device(devData->platDev));
    devData->ioAddr.phy = (volatile uint8_t *)PortIOmap[id];

#if (1 == CFG_DMA_MODE) || (2 == CFG_DMA_MODE)
    retval = edmaInit(
        devData);

    if (0 != retval) {

        return (NULL);
    }
#endif

    ES_DBG_API_OBLIGATION(devData->signature = DEVDATA_SIGNATURE);

    return (devData);
}

/*NOTE:     This function should release all used resources, unfortunately TI
 *          has really dropped the ball here because they did NOT implemented
 *          release functions like they should have.
 *          Currently some resource are released (mem, iomap, VFS files) but
 *          some are never released (clock, clk_alias). Fortunately, reinserting
 *          the module will only generate some warnings, so everything should be
 *          just fine.
 */
int32_t portTerm(
    struct devData *    devData) {

    int32_t             retval;

    ES_DBG_API_REQUIRE(ES_DBG_OBJECT_NOT_VALID, DEVDATA_SIGNATURE == devData->signature);

#if (1 == CFG_DMA_MODE) || (2 == CFG_DMA_MODE)
    edmaTerm(
        devData);
#endif
    LOG_DBG("OMAP UART: destroying device");
    retval = (int32_t)omap_device_shutdown(
        devData->platDev);

    if (0 != retval) {
        LOG_ERR("OMAP UART: failed to shutdown device, err: %d", -retval);
    }
    retval = (int32_t)omap_device_disable_clocks(
        to_omap_device(devData->platDev));

    if (0 != retval) {
        LOG_ERR("OMAP UART: failed to disable device clocks, err: %d", -retval);
    }
    omap_device_delete(
        to_omap_device(devData->platDev));
    platform_device_unregister(
        devData->platDev);
    kfree(
        devData);

    return (retval);
}

volatile uint8_t * portIORemapGet(
    struct devData *    devData) {

    volatile uint8_t *  ioremap;

    ES_DBG_API_REQUIRE(ES_DBG_OBJECT_NOT_VALID, DEVDATA_SIGNATURE == devData->signature);

    ioremap = omap_device_get_rt_va(
        to_omap_device(devData->platDev));

    return (ioremap);
}

int32_t portModeGet(
    uint32_t            baudrate) {

    int32_t             indx;

    indx = baudRateCfgFindIndex(baudrate);

    if (0 > indx) {

        return (indx);
    } else {

        return ((int32_t)ModeData[indx]);
    }
}

int32_t portDIVdataGet(
    uint32_t            baudrate) {

    int32_t             indx;

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
    void (* callback)(void *),
    void *              arg) {

    int32_t             retval;

    ES_DBG_API_REQUIRE(ES_DBG_OBJECT_NOT_VALID, DEVDATA_SIGNATURE == devData->signature);

    LOG_DBG("DMA Rx: init");

    devData->dma.rx.chn = EDMA_CHANNEL_ANY;
    devData->dma.rx.isRunning = FALSE;
    devData->dma.rx.callback = callback;
    devData->dma.rx.arg = arg;
    retval = (int32_t)edma_alloc_channel(
        EDMA_CHN_RX,
        edmaDummyCallback,
        NULL,
        EVENTQ_0);
    devData->dma.rx.chn = retval;

    return (retval);
}

void portDMARxTerm(
    struct devData *    devData) {

    ES_DBG_API_REQUIRE(ES_DBG_OBJECT_NOT_VALID, DEVDATA_SIGNATURE == devData->signature);

    portDMARxStopI(
        devData);
}

bool_T portDMARxIsRunning(
    struct devData *    devData) {

    ES_DBG_API_REQUIRE(ES_DBG_OBJECT_NOT_VALID, DEVDATA_SIGNATURE == devData->signature);

    return (devData->dma.rx.isRunning);
}

void portDMARxBeginI(
    struct devData *    devData,
    volatile uint8_t *  dst,
    size_t              size) {

    struct edmacc_param paramSet;

    ES_DBG_API_REQUIRE(ES_DBG_OBJECT_NOT_VALID, DEVDATA_SIGNATURE == devData->signature);

    LOG_DBG("DMA Rx: begin: dst  : %p", dst);
    LOG_DBG("DMA Rx: begin: chn  : %d", devData->dma.rx.chn);
    LOG_DBG("DMA Rx: begin: src  : %p", devData->ioAddr.phy);
    LOG_DBG("DMA Rx: begin: size : %d", size);

    devData->dma.rx.isRunning = TRUE;
    edma_set_src(
        devData->dma.rx.chn,
        (dma_addr_t)devData->ioAddr.phy,
        FIFO,
        W8BIT);
    edma_set_src_index(
        devData->dma.rx.chn,
        0,
        0);
    edma_set_dest_index(
        devData->dma.rx.chn,
        1,
        0);
    edma_read_slot(
        devData->dma.rx.chn,
        &paramSet);
    paramSet.opt |= TCINTEN;
    paramSet.opt |= EDMA_TCC(EDMA_CHAN_SLOT(devData->dma.rx.chn));
    edma_write_slot(
        devData->dma.rx.chn,
        &paramSet);
    edma_set_dest(
        devData->dma.rx.chn,
        (dma_addr_t)dst,
        INCR,
        W8BIT);
    edma_set_transfer_params(
        devData->dma.rx.chn,
        1,
        size,
        1,
        size,
        ABSYNC);
}

void portDMARxContinueI(
    struct devData *    devData,
    uint8_t *           dst,
    size_t              size) {

    ES_DBG_API_REQUIRE(ES_DBG_OBJECT_NOT_VALID, DEVDATA_SIGNATURE == devData->signature);

    /*
     * TODO linkovanje kanala
     */
}

void portDMARxStartI(
    struct devData *    devData) {

    int32_t             retval;

    ES_DBG_API_REQUIRE(ES_DBG_OBJECT_NOT_VALID, DEVDATA_SIGNATURE == devData->signature);
#if (1u == CFG_LOG_DBG_ENABLE)
    printPaRAM(
        devData->dma.tx.chn);
#endif
    edmaIntrClear(
        devData->dma.addr.remap,
        devData->dma.rx.chn);
    retval = (int32_t)edma_start(
        devData->dma.rx.chn);
    ES_DBG_API_ENSURE(ES_DBG_UNKNOWN_ERROR, 0 == retval);
}

void portDMARxStopI(
    struct devData *    devData) {

    ES_DBG_API_REQUIRE(ES_DBG_OBJECT_NOT_VALID, DEVDATA_SIGNATURE == devData->signature);

    if (EDMA_CHANNEL_ANY != devData->dma.rx.chn) {
        LOG_DBG("DMA Rx: stop chn : %d", devData->dma.tx.chn);
        edma_stop(
            devData->dma.rx.chn);
        edma_free_channel(
            devData->dma.rx.chn);
        devData->dma.rx.chn = EDMA_CHANNEL_ANY;
    }
}

int32_t portDMATxInit(
    struct devData *    devData,
    void (* callback)(void *),
    void *              arg) {

    int32_t             retval;

    ES_DBG_API_REQUIRE(ES_DBG_OBJECT_NOT_VALID, DEVDATA_SIGNATURE == devData->signature);

    LOG_DBG("DMA Tx: init");

    devData->dma.tx.chn = EDMA_CHANNEL_ANY;
    devData->dma.tx.isRunning = FALSE;
    devData->dma.tx.callback = callback;
    devData->dma.tx.arg = arg;
    retval = (int32_t)edma_alloc_channel(
        EDMA_CHN_TX,
        edmaDummyCallback,
        NULL,
        EVENTQ_1);

    if (0 > retval) {
        LOG_ERR("DMA Tx: failed to alloc channel, err: %d", -retval);

        return (retval);
    }
    devData->dma.tx.chn = retval;

    return (0);
}

void portDMATxTerm(
    struct devData *    devData) {

    ES_DBG_API_REQUIRE(ES_DBG_OBJECT_NOT_VALID, DEVDATA_SIGNATURE == devData->signature);

    portDMATxStopI(
        devData);
}

bool_T portDMATxIsRunning(
    struct devData *    devData) {

    ES_DBG_API_REQUIRE(ES_DBG_OBJECT_NOT_VALID, DEVDATA_SIGNATURE == devData->signature);

    return (devData->dma.tx.isRunning);
}

void portDMATxBeginI(
    struct devData *    devData,
    volatile const uint8_t * src,
    size_t              size) {

    struct edmacc_param paramSet;

    ES_DBG_API_REQUIRE(ES_DBG_OBJECT_NOT_VALID, DEVDATA_SIGNATURE == devData->signature);

    LOG_DBG("DMA Tx: begin: dst  : %p", devData->ioAddr.phy);
    LOG_DBG("DMA Tx: begin: chn  : %d", devData->dma.tx.chn);
    LOG_DBG("DMA Tx: begin: src  : %p", src);
    LOG_DBG("DMA Tx: begin: size : %d", size);

    devData->dma.tx.isRunning = TRUE;
    edma_set_dest(
        devData->dma.tx.chn,
        (dma_addr_t)devData->ioAddr.phy,
        FIFO,
        W8BIT);
    edma_set_dest_index(
        devData->dma.tx.chn,
        0,
        0);
    edma_set_src_index(
        devData->dma.tx.chn,
        1,
        0);
    edma_read_slot(
        devData->dma.tx.chn,
        &paramSet);
    paramSet.opt |= TCINTEN;
    paramSet.opt |= EDMA_TCC(EDMA_CHAN_SLOT(devData->dma.tx.chn));
    edma_write_slot(
        devData->dma.tx.chn,
        &paramSet);
    edma_set_src(
        devData->dma.tx.chn,
        (dma_addr_t)src,
        INCR,
        W8BIT);
    edma_set_transfer_params(
        devData->dma.tx.chn,
        1,
        size,
        1,
        size,
        ABSYNC);
}

void portDMATxContinueI(
    struct devData *    devData,
    const uint8_t *     src,
    size_t              size) {

    ES_DBG_API_REQUIRE(ES_DBG_OBJECT_NOT_VALID, DEVDATA_SIGNATURE == devData->signature);

    /*
     * TODO linkovanje kanala
     */
}

void portDMATxStartI(
    struct devData *    devData) {

    int32_t             retval;

    ES_DBG_API_REQUIRE(ES_DBG_OBJECT_NOT_VALID, DEVDATA_SIGNATURE == devData->signature);

    (void)edmaShRd(devData->dma.addr.remap, EDMA_IPR);
    LOG_DBG("DMA Tx: start chn %d", devData->dma.tx.chn);

#if (1u == CFG_LOG_DBG_ENABLE)
    printPaRAM(
        devData->dma.tx.chn);
#endif
    edmaIntrClear(
        devData->dma.addr.remap,
        devData->dma.tx.chn);
    retval = (int32_t)edma_start(
        devData->dma.tx.chn);
    ES_DBG_API_ENSURE(ES_DBG_UNKNOWN_ERROR, 0 == retval);
}

void portDMATxStopI(
    struct devData *    devData) {

    ES_DBG_API_REQUIRE(ES_DBG_OBJECT_NOT_VALID, DEVDATA_SIGNATURE == devData->signature);

    if (EDMA_CHANNEL_ANY != devData->dma.tx.chn) {
        LOG_DBG("DMA Tx: stop chn : %d", devData->dma.tx.chn);
        edma_stop(
            devData->dma.tx.chn);
        edma_free_channel(
            devData->dma.tx.chn);
        devData->dma.tx.chn = EDMA_CHANNEL_ANY;
    }
}
#endif

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of plat_omap.c
 ******************************************************************************/
