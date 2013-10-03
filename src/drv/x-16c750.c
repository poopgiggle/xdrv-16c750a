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
 * @brief       Driver for 16C750 UART hardware
 *********************************************************************//** @{ */

/*=========================================================  INCLUDE FILES  ==*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#include "arch/compiler.h"
#include "drv/x-16c750.h"
#include "drv/x-16c750_lld.h"
#include "drv/x-16c750_cfg.h"
#include "drv/x-16c750_ioctl.h"
#include "dbg/dbg.h"
#include "port/port.h"
#include "log.h"

/*=========================================================  LOCAL MACRO's  ==*/

#define DEF_DRV_VERSION_MAJOR           1
#define DEF_DRV_VERSION_MINOR           0
#define DEF_DRV_VERSION_PATCH           9
#define DEF_DRV_AUTHOR                  "Nenad Radulovic <nenad.radulovic@netico-group.com>"
#define DEF_DRV_DESCRIPTION             "Real-time 16C750 device driver"
#define DEF_DRV_SUPP_DEVICE             "UART 16C750A"

#define UART_CTX_SIGNATURE              0xDEADBEEF

#define NS_PER_US                       1000
#define US_PER_MS                       1000
#define MS_PER_S                        1000

#define NS_PER_MS                       (US_PER_MS * NS_PER_US)
#define NS_PER_S                        (MS_PER_S * NS_PER_MS)
#define US_TO_NS(us)                    (NS_PER_US * (us))
#define MS_TO_NS(ms)                    (NS_PER_MS * (ms))
#define SEC_TO_NS(sec)                  (NS_PER_S * (sec))

#if (0 == CFG_CRITICAL_INT_ENABLE)
#define CRITICAL_DECL(lockCtxName)                                              \
    rtdm_lockctx_t lockCtxName

#define CRITICAL_INIT(uartCtx)                                                  \
    rtdm_lock_init(&((uartCtx)->lock))

#define CRITICAL_ENTER(uartCtx, lockCtx)                                        \
    rtdm_lock_get_irqsave(&((uartCtx)->lock), lockCtx)

#define CRITICAL_ENTER_ISR(uartCtx)                                             \
    rtdm_lock_get(&(uartCtx)->lock)

#define CRITICAL_EXIT(uartCtx, lockCtx)                                         \
    rtdm_lock_put_irqrestore(&((uartCtx)->lock), lockCtx)

#define CRITICAL_EXIT_ISR(uartCtx)                                              \
    rtdm_lock_put(&(uartCtx)->lock)

#define CRITICAL_TYPE                                                           \
    rtdm_lockctx_t

#else
#define CRITICAL_DECL(lockCtxName)                                              \
    PORT_C_UNUSED uint8_t lockCtxName

#define CRITICAL_INIT(uartCtx)                                                  \
    (void)0

#define CRITICAL_ENTER(uartCtx, lockCtx)                                        \
    do {                                                                        \
        lldRegWr(                                                               \
            uartCtx->cache.ioRemap,                                                  \
            wIER,                                                               \
            0);                                                                 \
    } while (0)

#define CRITICAL_ENTER_ISR(uartCtx)                                             \
    (void)0

#define CRITICAL_EXIT(uartCtx, lockCtx)                                         \
    do {                                                                        \
        lldRegWr(                                                               \
            uartCtx->cache.ioRemap,                                                  \
            wIER,                                                               \
            uartCtx->cache.IER);                                                \
    } while (0)

#define CRITICAL_EXIT_ISR(uartCtx)                                              \
    (void)0

#define CRITICAL_TYPE                                                           \
    uint8_t

#endif

/*======================================================  LOCAL DATA TYPES  ==*/

enum cIntNum {
    C_INT_TX            = IER_THRIT,
    C_INT_RX            = IER_RHRIT,
    C_INT_RX_TIMEOUT    = IER_RHRIT
};

/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/

static void buffRxStartI(
    struct uartCtx *    uartCtx);

static void buffRxStopI(
    struct uartCtx *    uartCtx);

static void buffRxPendI(
    struct uartCtx *    uartCtx,
    size_t              pending);

#if (2 == CFG_DMA_MODE)
static void buffRxFinish(
    void *              data);
#endif

static int buffRxWait(
    struct uartCtx *    uartCtx,
    rtdm_toseq_t *      tmSeq);

static ssize_t buffRxCopyI(
    struct uartCtx *    uartCtx,
    CRITICAL_TYPE *     lockCtx,
    uint8_t *           dst,
    size_t              pending);

static void buffTxTrans(
    struct uartCtx *    uartCtx,
    size_t              size);

static void buffRxFlush(
    struct uartCtx *    uartCtx);

static void buffTxStartI(
    struct uartCtx *    uartCtx);

static void buffTxStopI(
    struct uartCtx *    uartCtx);

static void buffTxPendI(
    struct uartCtx *    uartCtx,
    size_t              pending);

static void buffTxFinishI(
    struct uartCtx *    uartCtx);

static int buffTxWait(
    struct uartCtx *    uartCtx,
    rtdm_toseq_t *      tmSeq);

static ssize_t buffTxCopyI(
    struct uartCtx *    uartCtx,
    CRITICAL_TYPE *     lockCtx,
    const uint8_t *     src,
    size_t              bytes);

#if (0 == CFG_DMA_MODE) || (1 == CFG_DMA_MODE)
static void buffTxFlushI(
    struct uartCtx *    uartCtx);
#endif

#if (0 == CFG_DMA_MODE) || (1 == CFG_DMA_MODE)
static void cIntEnable(
    struct uartCtx *    uartCtx,
    enum cIntNum        cIntNum);

static void cIntSetEnable(
    struct uartCtx *    uartCtx,
    enum cIntNum        cIntNum);
#endif

static void cIntDisable(
    struct uartCtx *    uartCtx,
    enum cIntNum        cIntNum);

static void cIntSetDisable(
    struct uartCtx *    uartCtx,
    enum cIntNum        cIntNum);

/**@brief       Create UART context
 */
static int uartCtxInit(
    struct uartCtx *    uartCtx,
    struct devData *    devData,
    volatile uint8_t *  io);

/**@brief       Destroy UART context
 */
static void uartCtxTerm(
    struct uartCtx *    uartCtx);

static bool_T xProtoIsValid(
    const struct xUartProto * proto);

static void xProtoSet(
    struct uartCtx *    uartCtx,
    const struct xUartProto * proto);

static struct uartCtx * uartCtxFromDevCtx(
    struct rtdm_dev_context * devCtx);

/**@brief       Named device open handler
 */
static int handleOpen(
    struct rtdm_dev_context * devCtx,
    rtdm_user_info_t *  usrInfo,
    int                 oflag);

/**@brief       Device close handler
 */
static int handleClose(
    struct rtdm_dev_context * devCtx,
    rtdm_user_info_t *  usrInfo);

/**@brief       IOCTL handler
 */
static int handleIOctl(
    struct rtdm_dev_context * devCtx,
    rtdm_user_info_t *  usrInfo,
    unsigned int        req,
    void __user *       mem);

/**@brief       Read device handler
 */
static int handleIrqRd(
    struct rtdm_dev_context * devCtx,
    rtdm_user_info_t *  usrInfo,
    void *              buff,
    size_t              bytes);

/**@brief       Write device handler
 */
static int handleIrqWr(
    struct rtdm_dev_context * devCtx,
    rtdm_user_info_t *  usrInfo,
    const void *        buff,
    size_t              bytes);

static int handleIrq(
    rtdm_irq_t *        arg);

/*=======================================================  LOCAL VARIABLES  ==*/

static struct rtdm_device UartDev = {
    .struct_version     = RTDM_DEVICE_STRUCT_VER,
    .device_flags       = RTDM_NAMED_DEVICE | RTDM_EXCLUSIVE,
    .context_size       = sizeof(struct uartCtx),
    .device_name        = CFG_DRV_NAME,
    .protocol_family    = 0,
    .socket_type        = 0,
    .open_rt            = NULL,
    .open_nrt           = handleOpen,
    .socket_rt          = NULL,
    .socket_nrt         = NULL,
    .ops                = {
        .close_rt           = NULL,
        .close_nrt          = handleClose,
        .ioctl_rt           = handleIOctl,
        .ioctl_nrt          = handleIOctl,
        .select_bind        = NULL,
        .read_rt            = handleIrqRd,
        .read_nrt           = NULL,
        .write_rt           = handleIrqWr,
        .write_nrt          = NULL,
        .recvmsg_rt         = NULL,
        .recvmsg_nrt        = NULL,
        .sendmsg_rt         = NULL,
        .sendmsg_nrt        = NULL
    },
    .device_class       = RTDM_CLASS_SERIAL,
    .device_sub_class   = 0,
    .profile_version    = 0,
    .driver_name        = CFG_DRV_NAME,
    .driver_version     = RTDM_DRIVER_VER(DEF_DRV_VERSION_MAJOR, DEF_DRV_VERSION_MINOR, DEF_DRV_VERSION_PATCH),
    .peripheral_name    = DEF_DRV_SUPP_DEVICE,
    .provider_name      = DEF_DRV_AUTHOR,
    .proc_name          = CFG_DRV_NAME,
    .device_id          = 0,
    .device_data        = NULL
};

/*======================================================  GLOBAL VARIABLES  ==*/

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DEF_DRV_AUTHOR);
MODULE_DESCRIPTION(DEF_DRV_DESCRIPTION);
MODULE_SUPPORTED_DEVICE(DEF_DRV_SUPP_DEVICE);

/*============================================  LOCAL FUNCTION DEFINITIONS  ==*/

static void buffRxStartI(
    struct uartCtx *    uartCtx) {

#if (0 == CFG_DMA_MODE) || (1 == CFG_DMA_MODE)
    cIntSetEnable(
        uartCtx,
        C_INT_RX | C_INT_RX_TIMEOUT);
#elif (2 == CFG_DMA_MODE)
    uartCtx->rx.done = 0U;
    buffRxPendI(
        uartCtx,
        bytes);
#endif
}

static void buffRxStopI(
    struct uartCtx *    uartCtx) {

#if (0 == CFG_DMA_MODE) || (1 == CFG_DMA_MODE)
    uartCtx->rx.pend = 0U;
    cIntSetDisable(
        uartCtx,
        C_INT_RX | C_INT_RX_TIMEOUT);
#elif (2 == CFG_DMA_MODE)
    uartCtx->rx.pend = 0U;
    uartCtx->rx.done = 0U;
    portDMARxStopI(
        uartCtx->devData);
#endif
}

static void buffRxPendI(
    struct uartCtx *    uartCtx,
    size_t              pending) {

#if (0 == CFG_DMA_MODE) || (1 == CFG_DMA_MODE)

    if (pending > circSizeGet(&uartCtx->rx.buff.handle)) {
        uartCtx->rx.pend = circSizeGet(&uartCtx->rx.buff.handle) - CFG_BUFF_BACKOFF;
    } else {
        uartCtx->rx.pend = pending;
    }
    rtdm_event_clear(
        &uartCtx->rx.opr);
#elif (2 == CFG_DMA_MODE)
    uint8_t *           dst;
    size_t              free;

    uartCtx->rx.pend = pending;
    dst = circMemHeadGet(
        &uartCtx->rx.buffHandle);
    free = circRemainingFreeGet(
        &uartCtx->rx.buffHandle);

    if (free >= pending) {
        /* single DMA transfer */
        uartCtx->rx.done += pending;
        uartCtx->rx.chunk = pending;
        portDMARxStart(
            uartCtx->devData,
            dst,
            pending,
            buffRxFinish,
            uartCtx);
    } else {
        /* multiple DMA transfers */
        uartCtx->rx.done += free;
        uartCtx->rx.chunk = free;
        portDMARxStart(
            uartCtx->devData,
            dst,
            free,
            buffRxContinue,
            uartCtx);
    }
#endif
}

static int buffRxWait(
    struct uartCtx *    uartCtx,
    rtdm_toseq_t *      tmSeq) {

    int                 retval;

    retval = rtdm_event_timedwait(
        &uartCtx->rx.opr,
        uartCtx->rx.oprTimeout,
        tmSeq);

    return (retval);
}

static ssize_t buffRxCopyI(
    struct uartCtx *    uartCtx,
    CRITICAL_TYPE *     lockCtx,
    uint8_t *           dst,
    size_t              pending) {

    size_t              transfer;
    size_t              cpd;
    size_t              occ;

    transfer = 0U;
    cpd = 0U;
    occ = circRemainingOccGet(
        &uartCtx->rx.buff.handle);

    if (0 == occ) {

        return (cpd);
    }

    do {
        uint8_t *       src;

        CRITICAL_EXIT(uartCtx, *lockCtx);
        dst += transfer;
        src = circMemTailGet(
            &uartCtx->rx.buff.handle);
        transfer = min(pending, occ);

        if (NULL != uartCtx->rx.user) {
            int         retval;

            retval = rtdm_copy_from_user(
                uartCtx->rx.user,
                dst,
                src,
                transfer);

            if (0 != retval) {
                CRITICAL_ENTER(uartCtx, *lockCtx);

                return (retval);
            }
        } else {
            memcpy(
                dst,
                src,
                transfer);
        }
        pending -= transfer;
        cpd   += transfer;
        CRITICAL_ENTER(uartCtx, *lockCtx);
        circPosTailSet(
            &uartCtx->rx.buff.handle,
            transfer);
        occ = circRemainingOccGet(
            &uartCtx->rx.buff.handle);
    } while ((0 != occ) && (0 != pending));

    return (cpd);
}

static void buffRxTrans(
    struct uartCtx *    uartCtx,
    size_t              size) {

    do {
        uint16_t        item;

        size--;
        item = lldRegRd(
            uartCtx->cache.io,
            RHR);
        circItemPut(
            &uartCtx->rx.buff.handle,
            item);
    } while (0U != size);
}

static void buffRxFlush(
    struct uartCtx *    uartCtx) {

    uartCtx->rx.pend = 0U;
    circFlush(
        &uartCtx->rx.buff.handle);
}

static void buffTxStartI(
    struct uartCtx *    uartCtx) {

#if (0 == CFG_DMA_MODE) || (1 == CFG_DMA_MODE)
    uartCtx->tx.pend = 0U;
    cIntEnable(
        uartCtx,
        C_INT_TX);
#elif (2 == CFG_DMA_MODE)
#endif
}

static void buffTxStopI(
    struct uartCtx *    uartCtx) {

#if (0 == CFG_DMA_MODE)
    uartCtx->tx.pend = 0U;
    cIntSetDisable(
        uartCtx,
        C_INT_TX);
#elif (1 == CFG_DMA_MODE)
    uartCtx->tx.pend = 0U;
    cIntSetDisable(
        uartCtx,
        C_INT_TX);
    portDMATxStopI(
        uartCtx->devData);
#elif (2 == CFG_DMA_MODE)
#endif
}

static void buffTxPendI(
    struct uartCtx *    uartCtx,
    size_t              pending) {

#if (0 == CFG_DMA_MODE) || (1 == CFG_DMA_MODE)

    if (pending > circSizeGet(&uartCtx->tx.buff.handle)) {
        uartCtx->tx.pend = circSizeGet(&uartCtx->tx.buff.handle);
    } else {
        uartCtx->tx.pend = pending;
    }
    rtdm_event_clear(
        &uartCtx->tx.opr);
    cIntEnable(
        uartCtx,
        C_INT_TX);

#elif (2 == CFG_DMA_MODE)
    uint8_t *           src;
    size_t              occ;

    uartCtx->tx.pend = pending;
    src = circMemTailGet(
        &uartCtx->tx.buffHandle);
    occ = circRemainingOccGet(
        &uartCtx->tx.buffHandle);

    if (occ >= pending) {
        /* single DMA transfer */
        LOG_INFO("single Tx DMA transfer");
        portDMATxStart(
            uartCtx->devData,
            src,
            pending,
            buffTxFinishI,
            uartCtx);
    } else {
        /* multiple DMA transfers*/
        LOG_INFO("multiple Tx DMA transfers");
    }
#endif
}

static void buffTxFinishI(
    struct uartCtx *    uartCtx) {

#if (0 == CFG_DMA_MODE)
    buffTxStopI(
        uartCtx);
#elif (1 == CFG_DMA_MODE)
    circPosTailSet(
        &uartCtx->tx.buff.handle,
        uartCtx->tx.chunk);

    if (FALSE == circIsEmpty(&uartCtx->tx.buff.handle)) {
        cIntEnable(
            uartCtx,
            C_INT_TX);
    }
#elif (2 == CFG_DMA_MODE)
#endif
}

static int buffTxWait(
    struct uartCtx *    uartCtx,
    rtdm_toseq_t *      tmSeq) {

    int                 retval;

    retval = rtdm_event_timedwait(
        &uartCtx->tx.opr,
        uartCtx->tx.oprTimeout,
        tmSeq);

    return (retval);
}

static ssize_t buffTxCopyI(
    struct uartCtx *    uartCtx,
    CRITICAL_TYPE *     lockCtx,
    const uint8_t *     src,
    size_t              bytes) {

    size_t              transfer;
    size_t              cpd;
    size_t              rem;

    transfer = 0U;
    cpd = 0U;
    rem = circRemainingFreeGet(
        &uartCtx->tx.buff.handle);

    if (0 == rem) {

        return (cpd);
    }

    do {
        uint8_t *       dst;

        CRITICAL_EXIT(uartCtx, *lockCtx);
        src += transfer;
        dst = circMemHeadGet(
            &uartCtx->tx.buff.handle);
        transfer = min(bytes, rem);

        if (NULL != uartCtx->tx.user) {
            int             retval;

            retval = rtdm_copy_from_user(
                uartCtx->tx.user,
                dst,
                src,
                transfer);

            if (0 != retval) {
                CRITICAL_ENTER(uartCtx, *lockCtx);

                return (retval);
            }
        } else {
            memcpy(
                dst,
                src,
                transfer);
        }
        bytes -= transfer;
        cpd   += transfer;
        CRITICAL_ENTER(uartCtx, *lockCtx);
        circPosHeadSet(
            &uartCtx->tx.buff.handle,
            transfer);
        rem = circRemainingFreeGet(
            &uartCtx->tx.buff.handle);
    } while ((0U != bytes) && (0U != rem));

    return (cpd);
}

static void buffTxTrans(
    struct uartCtx *    uartCtx,
    size_t              size) {

#if (0 == CFG_DMA_MODE)
    do {
        uint16_t        item;

        size--;
        item = circItemGet(
            &uartCtx->tx.buffHandle);
        lldRegWr(
            uartCtx->cache.ioRemap,
            wTHR,
            item);
    } while (0U != size);
#elif (1 == CFG_DMA_MODE)
    size_t              rem;
    uint8_t *           tail;

    rem = circOccGet(
        &uartCtx->tx.buff.handle);
    tail = circMemTailGet(
        &uartCtx->tx.buff.handle);

    if (size > rem) {
        /* two DMA transfers */
        LOG_INFO("two DMA transfers");
    } else {
        /* single DMA transfer */
        LOG_INFO("single DMA transfer");
        uartCtx->tx.chunk = rem;
        portDMATxStart(
            uartCtx->devData,
            tail,
            rem,
            (void (*)(void *))buffTxFinishI,
            uartCtx);
    }
    cIntSetDisable(
        uartCtx,
        C_INT_TX);
#endif
}

#if (0 == CFG_DMA_MODE) || (1 == CFG_DMA_MODE)
static void buffTxFlushI(
    struct uartCtx *    uartCtx) {

    uartCtx->tx.pend = 0U;
    circFlush(
        &uartCtx->tx.buff.handle);
}
#endif

#if (0 == CFG_DMA_MODE) || (1 == CFG_DMA_MODE)
static void cIntEnable(
    struct uartCtx *    uartCtx,
    enum cIntNum        cIntNum) {

#if (0 == CFG_CRITICAL_INT_ENABLE)
    uint32_t            tmp;

    tmp = uartCtx->cache.IER | cIntNum;

    if (tmp != uartCtx->cache.IER) {
        uartCtx->cache.IER = tmp;
        lldRegWr(
            uartCtx->cache.io,
            wIER,
            uartCtx->cache.IER);
    }
#else
    uartCtx->cache.IER |= cIntNum;
#endif
}

static void cIntSetEnable(
    struct uartCtx *    uartCtx,
    enum cIntNum        cIntNum) {

    uartCtx->cache.IER |= cIntNum;
    lldRegWr(
        uartCtx->cache.io,
        wIER,
        uartCtx->cache.IER);
}
#endif

#if (0 == CFG_DMA_MODE) || (1 == CFG_DMA_MODE)
static void cIntDisable(
    struct uartCtx *    uartCtx,
    enum cIntNum        cIntNum) {

#if (0 == CFG_CRITICAL_INT_ENABLE)
    uint32_t            tmp;

    tmp = uartCtx->cache.IER & ~cIntNum;

    if (tmp != uartCtx->cache.IER) {
        uartCtx->cache.IER = tmp;
        lldRegWr(
            uartCtx->cache.io,
            wIER,
            uartCtx->cache.IER);
    }
#else
    uartCtx->cache.IER &= ~cIntNum;
#endif
}
#endif

static void cIntSetDisable(
    struct uartCtx *    uartCtx,
    enum cIntNum        cIntNum) {

    uartCtx->cache.IER &= ~cIntNum;
    lldRegWr(
        uartCtx->cache.io,
        wIER,
        uartCtx->cache.IER);
}

static int uartCtxInit(
    struct uartCtx *    uartCtx,
    struct devData *    devData,
    volatile uint8_t *  io) {

    int                 retval;
    uint8_t *           buff;

    if (UART_CTX_SIGNATURE == uartCtx->signature) {
        LOG_ERR("UART context already initialized");

        return (-EINVAL);
    }
    /*-- STATE: init ---------------------------------------------------------*/
    rtdm_sem_init(
        &uartCtx->tx.acc,
        1U);
    rtdm_event_init(
        &uartCtx->tx.opr,
        0U);
    rtdm_sem_init(
        &uartCtx->rx.acc,
        1U);
    rtdm_event_init(
        &uartCtx->rx.opr,
        0U);
    uartCtx->state = CTX_STATE_LOCKS;

    /*-- STATE: Create TX buffer ---------------------------------------------*/
#if (0 == CFG_DMA_MODE)
    retval = rt_heap_create(
        &uartCtx->tx.heapHandle,
        NULL,
        CFG_DRV_BUFF_SIZE,
        H_SINGLE | H_DMA);
#elif (1 == CFG_DMA_MODE)
    retval = portDMATxInit(
        devData,
        &buff,
        CFG_DRV_BUFF_SIZE);
#elif (2 == CFG_DMA_MODE)
#endif
    if (0 != retval) {
        LOG_ERR("failed to create internal TX buffer, err: %d", retval);

        return (retval);
    }
    uartCtx->state = CTX_STATE_TX_BUFF;

    /*-- STATE: Alloc TX buffer ----------------------------------------------*/
#if (0 == CFG_DMA_MODE)
    retval = rt_heap_alloc(
        &uartCtx->tx.heapHandle,
        0U,
        TM_INFINITE,
        (void **)&buff);

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("failed to allocate internal TX buffer, err: %d", retval);

        return (retval);
    }
#endif
    circInit(
        &uartCtx->tx.buff.handle,
        buff,
        CFG_DRV_BUFF_SIZE);
    uartCtx->state = CTX_STATE_TX_BUFF_ALLOC;

    /*-- STATE: Create RX buffer ---------------------------------------------*/
#if (0 == CFG_DMA_MODE)
    retval = rt_heap_create(
        &uartCtx->rx.heapHandle,
        NULL,
        CFG_DRV_BUFF_SIZE,
        H_SINGLE | H_DMA);
#elif (1 == CFG_DMA_MODE)
    retval = portDMARxInit(
        devData,
        &buff,
        CFG_DRV_BUFF_SIZE);
#elif (2 == CFG_DMA_MODE)
#endif

    if (0 != retval) {
        LOG_ERR("failed to create internal RX buffer, err: %d", retval);

        return (retval);
    }
    uartCtx->state = CTX_STATE_RX_BUFF;

    /*-- STATE: Alloc RX buffer ----------------------------------------------*/
#if (0 == CFG_DMA_MODE)
    retval = rt_heap_alloc(
        &uartCtx->rx.heapHandle,
        0U,
        TM_INFINITE,
        (void **)&buff);

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("failed to allocate internal RX buffer, err: %d", retval);

        return (retval);
    }
#endif
    circInit(
        &uartCtx->rx.buff.handle,
        buff,
        CFG_DRV_BUFF_SIZE);
    uartCtx->state = CTX_STATE_RX_BUFF_ALLOC;

    /*-- Prepare UART data ---------------------------------------------------*/
    uartCtx->cache.io       = io;
    uartCtx->cache.IER      = lldRegRd(io, IER);
    uartCtx->tx.accTimeout  = MS_TO_NS(CFG_TIMEOUT_MS);
    uartCtx->tx.oprTimeout  = MS_TO_NS(CFG_TIMEOUT_MS);
    uartCtx->tx.pend        = 0U;
    uartCtx->tx.status      = UART_STATUS_NORMAL; /* not used */
    uartCtx->rx.accTimeout  = MS_TO_NS(CFG_TIMEOUT_MS);
    uartCtx->rx.oprTimeout  = MS_TO_NS(CFG_TIMEOUT_MS);
    uartCtx->rx.pend        = 0U;
    uartCtx->rx.status      = UART_STATUS_NORMAL; /* not used */
    uartCtx->devData        = devData;
    uartCtx->signature      = UART_CTX_SIGNATURE;
    xProtoSet(
        uartCtx,
        &DefProtocol);

    return (retval);
}

static void uartCtxTerm(
    struct uartCtx *    uartCtx) {

    int                 retval;

    switch (uartCtx->state) {
        case CTX_STATE_RX_BUFF_ALLOC : {
#if (0 == CFG_DMA_MODE)
            LOG_INFO("freeing Rx buffer");
            retval = rt_heap_free(
                &uartCtx->rx.heapHandle,
                circMemBaseGet(&uartCtx->rx.buffHandle));

            if (0 != retval) {
                LOG_ERR("failed to free Rx buffer, err: %d", retval);
            }
#endif
        } /* fall through */
        case CTX_STATE_RX_BUFF : {
#if (0 == CFG_DMA_MODE)
            LOG_INFO("deleting Rx buffer");
            retval = rt_heap_delete(
                &uartCtx->rx.heapHandle);

            if (0 != retval) {
                LOG_ERR("failed to delete Rx buffer, err: %d", retval);
            }
#elif (1 == CFG_DMA_MODE)
            LOG_INFO("deleting DMA Rx buffer");
            retval = portDMARxTerm(
                uartCtx->devData);

            if (0 != retval) {
                LOG_ERR("failed to delete DMA Rx buffer, err: %d", retval);
            }
#endif
        } /* fall through */
        case CTX_STATE_TX_BUFF_ALLOC : {
#if (0 == CFG_DMA_MODE)
            LOG_INFO("freeing Tx buffer");
            retval = rt_heap_free(
                &uartCtx->tx.heapHandle,
                circMemBaseGet(&uartCtx->tx.buffHandle));

            if (0 != retval) {
                LOG_ERR("failed to free Tx buffer, err: %d", retval);
            }
#endif

        } /* fall through */
        case CTX_STATE_TX_BUFF : {
#if (0 == CFG_DMA_MODE)
            LOG_INFO("deleting Tx buffer");

            retval = rt_heap_delete(
                &uartCtx->tx.heapHandle);

            if (0 != retval) {
                LOG_ERR("failed to delete Tx buffer, err: %d", retval);
            }
#elif (1 == CFG_DMA_MODE)
            LOG_INFO("deleting DMA Tx buffer");
            retval = portDMATxTerm(
                uartCtx->devData);

            if (0 != retval) {
                LOG_ERR("failed to delete DMA Tx buffer, err: %d", retval);
            }
#endif
        } /* fall through */
        case CTX_STATE_LOCKS : {
            rtdm_event_destroy(
                &uartCtx->rx.opr);
            rtdm_sem_destroy(
                &uartCtx->rx.acc);
            rtdm_event_destroy(
                &uartCtx->tx.opr);
            rtdm_sem_destroy(
                &uartCtx->tx.acc);
        } /* fall through */
        case CTX_STATE_INIT : {
            break;
        }
        default : {
            break;
        }
    }
}

static struct uartCtx * uartCtxFromDevCtx(
    struct rtdm_dev_context * devCtx) {

    return ((struct uartCtx *)rtdm_context_to_private(devCtx));
}

static bool_T xProtoIsValid(
    const struct xUartProto * proto) {

    /*
     * TODO: Check proto values here
     */
    return (TRUE);
}

static void xProtoSet(
    struct uartCtx *    uartCtx,
    const struct xUartProto * proto) {

    (void)lldProtocolSet(
        uartCtx->cache.io,
        proto);
    memcpy(
        &uartCtx->proto,
        proto,
        sizeof(struct xUartProto));
}

static int handleOpen(
    struct rtdm_dev_context * devCtx,
    rtdm_user_info_t *  usrInfo,
    int                 oflag) {

    int                 retval;
    struct uartCtx *    uartCtx;

    uartCtx = uartCtxFromDevCtx(
        devCtx);
    CRITICAL_INIT(uartCtx);
    retval = uartCtxInit(
        uartCtx,
        devCtx->device->device_data,
        portIORemapGet(devCtx->device->device_data));

    if (0 != retval) {

        return (retval);
    }
    lldFIFORxFlush(
        uartCtx->cache.io);
    lldFIFOTxFlush(
        uartCtx->cache.io);
#if (0 == CFG_DMA_MODE) || (1 == CFG_DMA_MODE)
    retval = rtdm_irq_request(
        &uartCtx->irqHandle,
        PortIRQ[devCtx->device->device_id],
        handleIrq,
        RTDM_IRQTYPE_EDGE,
        devCtx->device->proc_name,
        uartCtx);
#endif

    return (retval);
}

static int handleClose(
    struct rtdm_dev_context * devCtx,
    rtdm_user_info_t *  usrInfo) {

    struct uartCtx *    uartCtx;
    int                 retval;

    uartCtx = uartCtxFromDevCtx(devCtx);
    retval = 0;

    if (UART_CTX_SIGNATURE == uartCtx->signature) {                             /* Driver must be ready to accept close callbacks for       */
                                                                                /* already closed devices.                                  */
        CRITICAL_DECL(lockCtx);

        /*
         * TODO: Here should be some sync mechanism to wait for driver shutdown
         */
        CRITICAL_ENTER(uartCtx, lockCtx);
        cIntSetDisable(
            uartCtx,
            C_INT_TX | C_INT_RX | C_INT_RX_TIMEOUT);                            /* Turn off all interrupts                                  */
        retval = rtdm_irq_free(
            &uartCtx->irqHandle);

        if (0 != retval) {
            LOG_ERR("failed to free irq, err: %d", retval);
        }
        uartCtxTerm(
            uartCtx);
        CRITICAL_EXIT(uartCtx, lockCtx);
    }

    return (retval);
}

static int handleIOctl(
    struct rtdm_dev_context * devCtx,
    rtdm_user_info_t *  usrInfo,
    unsigned int        req,
    void __user *       mem) {

    struct uartCtx *    uartCtx;
    int                 retval;

    uartCtx = uartCtxFromDevCtx(devCtx);
    retval = 0;

    switch (req) {
        case XUART_PROTOCOL_GET : {

            if (NULL != usrInfo) {
                retval = rtdm_safe_copy_to_user(
                    usrInfo,
                    mem,
                    &uartCtx->proto,
                    sizeof(struct xUartProto));
            } else {
                memcpy(
                    mem,
                    &uartCtx->proto,
                    sizeof(struct xUartProto));
            }
            break;
        }

        case XUART_PROTOCOL_SET : {
            struct xUartProto proto;

            if (NULL != usrInfo) {
                retval = rtdm_safe_copy_from_user(
                    usrInfo,
                    &proto,
                    mem,
                    sizeof(struct xUartProto));
            } else {
                memcpy(
                    &proto,
                    mem,
                    sizeof(struct xUartProto));
            }

            if (TRUE == xProtoIsValid(&proto)) {
                CRITICAL_DECL(lockCtx);

                CRITICAL_ENTER(uartCtx, lockCtx);
                xProtoSet(
                    uartCtx,
                    &proto);
                CRITICAL_EXIT(uartCtx, lockCtx);
            }

            break;
        }

        default : {
            retval = -ENOTSUPP;
        }
    }

    return (retval);
}

static int handleIrqRd(
    struct rtdm_dev_context * devCtx,
    rtdm_user_info_t *  usrInfo,
    void *              buff,
    size_t              bytes) {

    CRITICAL_DECL(lockCtx);
    rtdm_toseq_t        tmSeq;
    struct uartCtx *    uartCtx;
    uint8_t *           dst;
    size_t              read;
    int                 retval;

    if (NULL != usrInfo) {

        if (0 == rtdm_rw_user_ok(usrInfo, buff, bytes)) {
            uartCtx->rx.status = UART_STATUS_FAULT_USAGE;

            return (-EFAULT);
        }
    }
    uartCtx = uartCtxFromDevCtx(devCtx);
    uartCtx->rx.user = usrInfo;
    retval = rtdm_sem_timeddown(
        &uartCtx->rx.acc,
        uartCtx->rx.accTimeout,
        NULL);

    if (0 != retval) {
        uartCtx->rx.status = UART_STATUS_BUSY;

        return (-EBUSY);
    }
    rtdm_toseq_init(
        &tmSeq,
        uartCtx->rx.oprTimeout);
    read = 0U;
    dst = (uint8_t *)buff;
    buffRxFlush(
        uartCtx);
    lldFIFORxFlush(
        uartCtx->cache.io);
    CRITICAL_ENTER(uartCtx, lockCtx);
    buffRxStartI(
        uartCtx);

    do {
        size_t          transfer;
        int             retval;

        buffRxPendI(
            uartCtx,
            bytes);
        CRITICAL_EXIT(uartCtx, lockCtx);
        retval = buffRxWait(
            uartCtx,
            &tmSeq);

        if (0 > retval) {

             break;
        }
        CRITICAL_ENTER(uartCtx, lockCtx);
        transfer = buffRxCopyI(
            uartCtx,
            &lockCtx,
            dst,
            bytes);

        if (0 > transfer) {

            break;
        }
        dst   += transfer;
        bytes -= transfer;
        read  += transfer;
    } while (0 < bytes);

    buffRxStopI(
        uartCtx);

    if (0 != circOccGet(&uartCtx->rx.buff.handle)) {
        /*
         * TODO: Is this overflow?
         */
    }
    CRITICAL_EXIT(uartCtx, lockCtx);
    rtdm_sem_up(
        &uartCtx->rx.acc);

    if (0 == retval) {
        retval = read;
    }

    return (retval);
}

static int handleIrqWr(
    struct rtdm_dev_context * devCtx,
    rtdm_user_info_t *  usrInfo,
    const void *        buff,
    size_t              bytes) {

    CRITICAL_DECL(lockCtx);
    rtdm_toseq_t        tmSeq;
    struct uartCtx *    uartCtx;
    const uint8_t *     src;
    size_t              written;
    int                 retval;
    ssize_t             transfer;

    if (NULL != usrInfo) {

        if (0 == rtdm_read_user_ok(usrInfo, buff, bytes)) {
            uartCtx->tx.status = UART_STATUS_FAULT_USAGE;

            return (-EFAULT);
        }
    }
    uartCtx = uartCtxFromDevCtx(devCtx);
    uartCtx->rx.user = usrInfo;
    retval = rtdm_sem_timeddown(
        &uartCtx->tx.acc,
        uartCtx->tx.accTimeout,
        NULL);

    if (0 != retval) {
        uartCtx->tx.status = UART_STATUS_BUSY;

        return (-EBUSY);
    }
    rtdm_toseq_init(
        &tmSeq,
        uartCtx->tx.oprTimeout);

    if (TRUE == circIsEmpty(&uartCtx->tx.buff.handle)) {
        buffTxFlushI(
            uartCtx);
    }
    src = (const uint8_t *)buff;
    CRITICAL_ENTER(uartCtx, lockCtx);
    transfer = buffTxCopyI(
        uartCtx,
        &lockCtx,
        src,
        bytes);

    if (0 > transfer) {
        buffTxStopI(
            uartCtx);
        CRITICAL_EXIT(uartCtx, lockCtx);
        rtdm_sem_up(
            &uartCtx->tx.acc);

        return (transfer);
    }
    buffTxStartI(
        uartCtx);
    src     += transfer;
    bytes   -= transfer;
    written  = transfer;

    while (0 < bytes) {
        buffTxPendI(
            uartCtx,
            bytes);
        CRITICAL_EXIT(uartCtx, lockCtx);
        retval = buffTxWait(
            uartCtx,
            &tmSeq);

        if (0 > retval) {

            break;
        }
        CRITICAL_ENTER(uartCtx, lockCtx);
        transfer = buffTxCopyI(
            uartCtx,
            &lockCtx,
            src,
            bytes);

        if (0 > transfer) {

            break;
        }
        src     += transfer;
        bytes   -= transfer;
        written += transfer;
    }
    CRITICAL_EXIT(uartCtx, lockCtx);
    rtdm_sem_up(
        &uartCtx->tx.acc);

    if (0 == retval) {

        return (written);
    }

    return (retval);
}

static int handleIrq(
    rtdm_irq_t *        arg) {

#if (0 == CFG_DMA_MODE) || (1 == CFG_DMA_MODE)
    struct uartCtx *    uartCtx;
    volatile uint8_t *  io;
    int                 retval;
    enum lldIntNum      intNum;

    uartCtx = rtdm_irq_get_arg(arg, struct uartCtx);
    io = uartCtx->cache.io;
    retval = RTDM_IRQ_HANDLED;
    uartCtx->rx.status = UART_STATUS_NORMAL;
    CRITICAL_ENTER_ISR(uartCtx);

    while (LLD_INT_NONE != (intNum = lldIntGet(io))) {                                          /* Loop until there are interrupts to process               */

        /*-- Receive interrupt -----------------------------------------------*/
        if ((LLD_INT_RX == intNum) || (LLD_INT_RX_TIMEOUT == intNum)) {
            size_t      transfer;

            transfer = lldFIFORxOccupied(
                io);

            if (transfer > circFreeGet(&uartCtx->rx.buff.handle)) {
                cIntSetDisable(
                    uartCtx,
                    C_INT_RX | C_INT_RX_TIMEOUT);
                lldFIFORxFlush(
                    io);
                uartCtx->rx.status = UART_STATUS_SOFT_OVERFLOW;
            } else {
                buffRxTrans(
                    uartCtx,
                    transfer);

                if (uartCtx->rx.pend <= circOccGet(&uartCtx->rx.buff.handle)) {
                    uartCtx->rx.pend = 0U;
                    rtdm_event_signal(
                        &uartCtx->rx.opr);
                }
            }

        /*-- Transmit interrupt ----------------------------------------------*/
        } else if (LLD_INT_TX == intNum) {
            size_t      transfer;

            transfer = min(lldFIFOTxFree(io), circOccGet(&uartCtx->tx.buff.handle));

            buffTxTrans(
                uartCtx,
                transfer);

            if (0 != uartCtx->tx.pend) {

                if (circFreeGet(&uartCtx->tx.buff.handle) >= uartCtx->tx.pend) {
                    uartCtx->tx.pend = 0U;
                    rtdm_event_signal(
                        &uartCtx->tx.opr);
                }
            }

            if (TRUE == circIsEmpty(&uartCtx->tx.buff.handle)) {
                buffTxStopI(
                    uartCtx);
            }

        /*-- Other interrupts ------------------------------------------------*/
        } else {
            /*
             * We don't know what happened here, so we disable all interrupts
             * regardless the cache and notify all listeners. This is kind of a
             * bug.
             */
            retval = RTDM_IRQ_NONE;
            uartCtx->rx.status = UART_STATUS_UNHANDLED_INTERRUPT;
            uartCtx->tx.status = UART_STATUS_UNHANDLED_INTERRUPT;
            lldRegWr(
                uartCtx->cache.io,
                wIER,
                0);

            break;
        }
    }
    CRITICAL_EXIT_ISR(uartCtx);

    return (retval);
#elif (2 == CFG_DMA_MODE)
    LOG_ERR("unhandled interrupt occured");

    return (RTDM_IRQ_NONE);
#endif

}

/*===================================  GLOBAL PRIVATE FUNCTION DEFINITIONS  ==*/
/*====================================  GLOBAL PUBLIC FUNCTION DEFINITIONS  ==*/

int __init moduleInit(
    void) {

    int                 retval;

    LOG_INFO(DEF_DRV_DESCRIPTION);
    LOG_INFO("version: %d.%d.%d", DEF_DRV_VERSION_MAJOR, DEF_DRV_VERSION_MINOR, DEF_DRV_VERSION_PATCH);

    UartDev.device_id = CFG_UART_ID;
    memcpy(&UartDev.device_name, CFG_DRV_NAME, sizeof(CFG_DRV_NAME));

    /*-- STATE: Port initialization ------------------------------------------*/
    UartDev.device_data = portInit(
        UartDev.device_id);

    if (NULL == UartDev.device_data) {
        LOG_ERR("failed to initialize port driver, err: %d", -ENOTSUPP);

        return (-ENOTSUPP);
    }

    /*-- STATE: Low-level driver initialization ------------------------------*/
    retval = lldInit(
        portIORemapGet(UartDev.device_data));                                   /* Initialize Linux device driver                           */

    if (0 != retval) {
        LOG_ERR("failed to initialize low-level driver, err: %d", retval);
        portTerm(
            UartDev.device_data);

        return (retval);
    }

    /*-- STATE: Xenomai device registration ----------------------------------*/
    LOG_INFO("registering device: %s, id: %d", (char *)&UartDev.device_name, UartDev.device_id);
    retval = rtdm_dev_register(
        &UartDev);

    if (0 != retval) {
        LOG_ERR("failed to register to Real-Time DM, err: %d", retval);
        lldTerm(
            UartDev.device_data);
        portTerm(
            UartDev.device_data);
    }

    return (retval);
}

void __exit moduleTerm(
    void) {
    int             retval;

    LOG_INFO("removing driver for UART: %d", UartDev.device_id);
    retval = rtdm_dev_unregister(
        &UartDev,
        CFG_TIMEOUT_MS);

    if (0 != retval) {
        LOG_ERR("failed to unregister device, err: %d", retval);
    }
    retval = lldTerm(
        portIORemapGet(UartDev.device_data));

    if (0 != retval) {
        LOG_ERR("failed terminate platform device driver, err: %d", retval);
    }
    retval = portTerm(
        UartDev.device_data);

    if (0 != retval) {
        LOG_ERR("failed terminate low-level device driver, err: %d", retval);
    }
}

void userAssert(
    const struct esDbgReport * dbgReport) {

    printk(KERN_ERR "\n ----\n");
    printk(KERN_ERR DEF_DRV_DESCRIPTION " ASSERTION FAILED\n");
    printk(KERN_ERR " Module name: %s\n", dbgReport->modName);
    printk(KERN_ERR " Module desc: %s\n", dbgReport->modDesc);
    printk(KERN_ERR " Module file: %s\n", dbgReport->modFile);
    printk(KERN_ERR " Module author: %s\n", dbgReport->modAuthor);
    printk(KERN_ERR " --\n");
    printk(KERN_ERR " Function   : %s\n", dbgReport->fnName);
    printk(KERN_ERR " Line       : %d\n", dbgReport->line);
    printk(KERN_ERR " Expression : %s\n", dbgReport->expr);
    printk(KERN_ERR " --\n");
    printk(KERN_ERR " Msg num    : %d\n", dbgReport->msgNum);
    printk(KERN_ERR " Msg text   : %s\n", dbgReport->msgText);
    printk(KERN_ERR " ----\n");
}

module_init(moduleInit);
module_exit(moduleTerm);

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of x-16c750.c
 ******************************************************************************/
