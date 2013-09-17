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
#include "port/port.h"
#include "log.h"

/*=========================================================  LOCAL MACRO's  ==*/

#define DEF_DRV_VERSION_MAJOR           1
#define DEF_DRV_VERSION_MINOR           0
#define DEF_DRV_VERSION_PATCH           3
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

/*======================================================  LOCAL DATA TYPES  ==*/

/**@brief       States of the context process
 */
enum ctxState {
    CTX_STATE_INIT,                                                             /**<@brief STATE_INIT                                       */
    CTX_STATE_TX_BUFF,
    CTX_STATE_TX_BUFF_ALLOC,
    CTX_STATE_RX_BUFF,
    CTX_STATE_RX_BUFF_ALLOC,
};

enum cIntNum {
    C_INT_TX            = IER_THRIT,
    C_INT_RX            = IER_RHRIT,
    C_INT_RX_TIMEOUT    = IER_RHRIT
};

/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/

/**@brief       Cleanup all work done by xUartCtxCreate() in case it failed
 */
static void xUartCtxCleanup(
    struct uartCtx *    uartCtx,
    enum ctxState       state);

/**@brief       Create UART context
 */
static int xUartCtxInit(
    struct uartCtx *    uartCtx,
    volatile uint8_t *  io,
    uint32_t            id);

/**@brief       Destroy UART context
 */
static int xUartCtxTerm(
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
static int handleRd(
    struct rtdm_dev_context * devCtx,
    rtdm_user_info_t *  usrInfo,
    void *              buff,
    size_t              bytes);

/**@brief       Write device handler
 */
static int handleWr(
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
        .close_rt       = NULL,
        .close_nrt      = handleClose,
        .ioctl_rt       = handleIOctl,
        .ioctl_nrt      = handleIOctl,
        .select_bind    = NULL,
        .read_rt        = handleRd,
        .read_nrt       = NULL,
        .write_rt       = handleWr,
        .write_nrt      = NULL,
        .recvmsg_rt     = NULL,
        .recvmsg_nrt    = NULL,
        .sendmsg_rt     = NULL,
        .sendmsg_nrt    = NULL
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

static void cIntEnable(
    struct uartCtx *    uartCtx,
    enum cIntNum        cIntNum) {

    uint32_t            tmp;

    tmp = uartCtx->cache.IER | cIntNum;

    if (uartCtx->cache.IER != tmp) {
        uartCtx->cache.IER |= cIntNum;
        lldRegWr(
            uartCtx->cache.io,
            wIER,
            tmp);
    }
}

static void cIntDisable(
    struct uartCtx *    uartCtx,
    enum cIntNum        cIntNum) {

    uint32_t            tmp;

    tmp = uartCtx->cache.IER & ~cIntNum;

    if (uartCtx->cache.IER != tmp) {
        uartCtx->cache.IER &= ~cIntNum;
        lldRegWr(
            uartCtx->cache.io,
            wIER,
            uartCtx->cache.IER);
    }
}

static void xUartCtxCleanup(
    struct uartCtx *    uartCtx,
    enum ctxState       state) {

    switch (state) {

        case CTX_STATE_RX_BUFF_ALLOC: {
            LOG_INFO("reversing action: create internal RX buffer");
            (void)rt_heap_delete(
                &uartCtx->rx.heapHandle);
        }

        case CTX_STATE_RX_BUFF: {
            LOG_INFO("reversing action: allocate internal TX buffer");
            (void)rt_heap_free(
                &uartCtx->tx.heapHandle,
                circMemBaseGet(&uartCtx->tx.buffHandle));
            /* fall through */
        }

        case CTX_STATE_TX_BUFF_ALLOC: {
            LOG_INFO("reversing action: create internal TX buffer");
            (void)rt_heap_delete(
                &uartCtx->tx.heapHandle);
            /* fall through */
        }

        case CTX_STATE_TX_BUFF : {
            LOG_INFO("reversing action: init");
            break;
        }

        default : {
            /* nothing */
        }
    }
}

static int xUartCtxInit(
    struct uartCtx *    uartCtx,
    volatile uint8_t *  io,
    uint32_t            id) {

    enum ctxState       state;
    int                 retval;
    void *              buff;

    if (UART_CTX_SIGNATURE == uartCtx->signature) {
        LOG_ERR("UART context already initialized");

        return (-EINVAL);
    }
    /*-- STATE: init ---------------------------------------------------------*/
    state = CTX_STATE_INIT;

    /*-- STATE: Create TX buffer ---------------------------------------------*/
    state  = CTX_STATE_TX_BUFF;
    retval = rt_heap_create(
        &uartCtx->tx.heapHandle,
        NULL,
        CFG_DRV_BUFF_SIZE,
        H_SINGLE);

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("failed to create internal TX buffer");
        xUartCtxCleanup(
            uartCtx,
            state);

        return (retval);
    }

    /*-- STATE: Alloc TX buffer ----------------------------------------------*/
    state  = CTX_STATE_TX_BUFF_ALLOC;
    retval = rt_heap_alloc(
        &uartCtx->tx.heapHandle,
        0U,
        TM_INFINITE,
        &buff);

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("failed to allocate internal TX buffer");
        xUartCtxCleanup(
            uartCtx,
            state);

        return (retval);
    }
    circInit(
        &uartCtx->tx.buffHandle,
        buff,
        CFG_DRV_BUFF_SIZE);

    /*-- STATE: Create RX buffer ---------------------------------------------*/
    state  = CTX_STATE_RX_BUFF;
    retval = rt_heap_create(
        &uartCtx->rx.heapHandle,
        NULL,
        CFG_DRV_BUFF_SIZE,
        H_SINGLE);

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("failed to create internal RX buffer");
        xUartCtxCleanup(
            uartCtx,
            state);

        return (retval);
    }

    /*-- STATE: Alloc RX buffer ----------------------------------------------*/
    state  = CTX_STATE_RX_BUFF_ALLOC;
    retval = rt_heap_alloc(
        &uartCtx->rx.heapHandle,
        0U,
        TM_INFINITE,
        &buff);

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("failed to allocate internal RX buffer");
        xUartCtxCleanup(
            uartCtx,
            state);

        return (retval);
    }
    circInit(
        &uartCtx->rx.buffHandle,
        buff,
        CFG_DRV_BUFF_SIZE);

    /*-- Prepare UART data ---------------------------------------------------*/
    uartCtx->cache.io       = io;
    uartCtx->cache.IER      = lldRegRd(io, IER);
    uartCtx->cache.LSR      = lldRegRd(io, LSR);
    uartCtx->cache.MCR      = lldRegRd(io, MCR);
    uartCtx->cache.SCR      = lldRegRd(io, SCR);
    uartCtx->tx.accTimeout  = MS_TO_NS(CFG_TIMEOUT_MS);
    uartCtx->tx.oprTimeout  = MS_TO_NS(CFG_TIMEOUT_MS);
    uartCtx->tx.pend        = 0U;
    uartCtx->tx.status      = UART_STATUS_NORMAL;
    uartCtx->rx.accTimeout  = MS_TO_NS(CFG_TIMEOUT_MS);
    uartCtx->rx.oprTimeout  = MS_TO_NS(CFG_TIMEOUT_MS);
    uartCtx->rx.pend        = 0U;
    uartCtx->rx.status      = UART_STATUS_NORMAL;
    uartCtx->signature      = UART_CTX_SIGNATURE;

    xProtoSet(
        uartCtx,
        &DefProtocol);
    rtdm_mutex_init(
        &uartCtx->tx.acc);
    rtdm_event_init(
        &uartCtx->tx.opr,
        0);
    rtdm_mutex_init(
        &uartCtx->rx.acc);
    rtdm_event_init(
        &uartCtx->rx.opr,
        0);

    return (retval);
}

static int xUartCtxTerm(
    struct uartCtx *    uartCtx) {

    int                 retval;

    uartCtx->signature = ~UART_CTX_SIGNATURE;
    rtdm_event_destroy(
        &uartCtx->rx.opr);
    rtdm_mutex_destroy(
        &uartCtx->rx.acc);
    rtdm_event_destroy(
        &uartCtx->tx.opr);
    rtdm_mutex_destroy(
        &uartCtx->tx.acc);
    retval = rt_heap_free(
        &uartCtx->rx.heapHandle,
        circMemBaseGet(&uartCtx->rx.buffHandle));
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "failed to free internal RX buffer");
    retval = rt_heap_delete(
        &uartCtx->rx.heapHandle);
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "failed to delete internal RX buffer");
    retval = rt_heap_free(
        &uartCtx->tx.heapHandle,
        circMemBaseGet(&uartCtx->tx.buffHandle));
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "failed to free internal TX buffer");
    retval = rt_heap_delete(
        &uartCtx->tx.heapHandle);
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "failed to delete internal TX buffer");

    return (retval);
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

static struct uartCtx * uartCtxFromDevCtx(
    struct rtdm_dev_context * devCtx) {

    return ((struct uartCtx *)rtdm_context_to_private(devCtx));
}

static int handleOpen(
    struct rtdm_dev_context * devCtx,
    rtdm_user_info_t *  usrInfo,
    int                 oflag) {

    int                 retval;
    struct uartCtx *    uartCtx;
    rtdm_lockctx_t      lockCtx;
    volatile uint8_t *  io;
    uint32_t            id;

    uartCtx = uartCtxFromDevCtx(
        devCtx);

    io = portIORemapGet(devCtx->device->device_data);
    id = devCtx->device->device_id;
    rtdm_lock_init(&uartCtx->lock);
    rtdm_lock_get_irqsave(&uartCtx->lock, lockCtx);
    retval = xUartCtxInit(
        uartCtx,
        io,
        id);

    if (RETVAL_SUCCESS != retval) {
        rtdm_lock_put_irqrestore(&uartCtx->lock, lockCtx);
        LOG_ERR("failed to create device context");

        return (retval);
    }
    retval = rtdm_irq_request(
        &uartCtx->irqHandle,
        PortIRQ[id],
        handleIrq,
        RTDM_IRQTYPE_EDGE,
        devCtx->device->proc_name,
        uartCtx);
    cIntEnable(
        uartCtx,
        C_INT_RX | C_INT_RX_TIMEOUT);
    rtdm_lock_put_irqrestore(&uartCtx->lock, lockCtx);
    LOG_ERR_IF(RETVAL_SUCCESS != retval, "failed to register interrupt handler");

    return (retval);
}

static int handleClose(
    struct rtdm_dev_context * devCtx,
    rtdm_user_info_t *  usrInfo) {

    struct uartCtx *    uartCtx;
    int                 retval;

    retval = RETVAL_SUCCESS;
    uartCtx = uartCtxFromDevCtx(devCtx);

    if (UART_CTX_SIGNATURE == uartCtx->signature) {                             /* Driver must be ready to accept close callbacks for       */
                                                                                /* already closed devices.                                  */
        rtdm_lockctx_t  lockCtx;

        /*
         * TODO: Here should be some sync mechanism to wait for driver shutdown
         */
        rtdm_lock_get_irqsave(&uartCtx->lock, lockCtx);
        cIntDisable(
            uartCtx,
            C_INT_TX | C_INT_RX | C_INT_RX_TIMEOUT);                            /* Turn off all interrupts                                  */
        retval = rtdm_irq_free(
            &uartCtx->irqHandle);
        LOG_ERR_IF(RETVAL_SUCCESS != retval, "failed to unregister interrupt");
        retval = xUartCtxTerm(
            uartCtx);
        rtdm_lock_put_irqrestore(
            &uartCtx->lock,
            lockCtx);
        LOG_ERR_IF(RETVAL_SUCCESS != retval, "failed to destroy device context");
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
    retval = RETVAL_SUCCESS;

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
                rtdm_lockctx_t  lockCtx;

                rtdm_lock_get_irqsave(&uartCtx->lock, lockCtx);
                xProtoSet(
                    uartCtx,
                    &proto);
                rtdm_lock_put_irqrestore(&uartCtx->lock, lockCtx);
            }

            break;
        }

        default : {
            retval = -ENOTSUPP;
        }
    }

    return (retval);
}

static int handleRd(
    struct rtdm_dev_context * devCtx,
    rtdm_user_info_t *  usrInfo,
    void *              buff,
    size_t              bytes) {

    struct uartCtx *    uartCtx;
    rtdm_toseq_t        oprTimeSeq;
    int                 retval;
    uint8_t *           src;
    uint8_t *           dst;
    size_t              read;

    if (NULL != usrInfo) {

        if (0 == rtdm_rw_user_ok(usrInfo, buff, bytes)) {
            uartCtx->rx.status = UART_STATUS_FAULT_USAGE;

            return (-EFAULT);
        }
    }
    uartCtx = uartCtxFromDevCtx(devCtx);
    retval = rtdm_mutex_timedlock(
        &uartCtx->rx.acc,
        uartCtx->rx.accTimeout,
        NULL);

    if (RETVAL_SUCCESS != retval) {
        uartCtx->rx.status = UART_STATUS_BUSY;

        return (-EBUSY);
    }
    rtdm_toseq_init(
        &oprTimeSeq,
        uartCtx->rx.oprTimeout);
    read = 0U;
    dst = (uint8_t *)buff;
    src = circMemTailGet(
        &uartCtx->rx.buffHandle);

    do {
        size_t          occupied;
        rtdm_lockctx_t  lockCtx;


        rtdm_lock_get_irqsave(&uartCtx->lock, lockCtx);
        occupied = circRemainingOccGet(
            &uartCtx->rx.buffHandle);

        if (0U != occupied) {
            size_t      transfer;

            transfer = min(bytes, occupied);

            if (NULL != usrInfo) {
                retval = rtdm_copy_to_user(
                    usrInfo,
                    dst,
                    src,
                    transfer);

                if (RETVAL_SUCCESS != retval) {
                    uartCtx->rx.status = UART_STATUS_FAULT_USAGE;
                    rtdm_lock_put_irqrestore(&uartCtx->lock, lockCtx);

                    break;
                }
            } else {
                memcpy(
                    dst,
                    src,
                    transfer);
            }
            read  += transfer;
            bytes -= transfer;
            dst   += transfer;
            circPosTailSet(
                &uartCtx->rx.buffHandle,
                transfer);
            src = circMemTailGet(
                &uartCtx->rx.buffHandle);
            cIntEnable(
                uartCtx,
                C_INT_RX | C_INT_RX_TIMEOUT);
            uartCtx->rx.pend = 0U;
            rtdm_lock_put_irqrestore(&uartCtx->lock, lockCtx);
        } else {
            cIntEnable(
                uartCtx,
                C_INT_RX | C_INT_RX_TIMEOUT);
            uartCtx->rx.pend = min(bytes, circSizeGet(&uartCtx->rx.buffHandle) - CFG_BUFF_BACKOFF);
            rtdm_lock_put_irqrestore(&uartCtx->lock, lockCtx);
            retval = rtdm_event_timedwait(
                &uartCtx->rx.opr,
                uartCtx->rx.oprTimeout,
                &oprTimeSeq);

            if (RETVAL_SUCCESS != retval) {

                break;
            }
        }
    } while (0U < bytes);
    rtdm_mutex_unlock(
        &uartCtx->rx.acc);

    if (RETVAL_SUCCESS == retval) {
        retval = read;
    }

    return (retval);
}

static int handleWr(
    struct rtdm_dev_context * devCtx,
    rtdm_user_info_t *  usrInfo,
    const void *        buff,
    size_t              bytes) {

    struct uartCtx *    uartCtx;
    rtdm_toseq_t        oprTimeSeq;
    int                 retval;
    uint8_t *           src;
    uint8_t *           dst;
    size_t              written;

    if (NULL != usrInfo) {

        if (0 == rtdm_read_user_ok(usrInfo, buff, bytes)) {
            uartCtx->tx.status = UART_STATUS_FAULT_USAGE;

            return (-EFAULT);
        }
    }
    uartCtx = uartCtxFromDevCtx(devCtx);
    retval = rtdm_mutex_timedlock(
        &uartCtx->tx.acc,
        uartCtx->tx.accTimeout,
        NULL);

    if (RETVAL_SUCCESS != retval) {
        uartCtx->tx.status = UART_STATUS_BUSY;

        return (-EBUSY);
    }
    rtdm_toseq_init(
        &oprTimeSeq,
        uartCtx->tx.oprTimeout);
    written = 0U;
    src = (uint8_t *)buff;
    dst = circMemHeadGet(
        &uartCtx->tx.buffHandle);

    do {
        size_t          remaining;
        rtdm_lockctx_t  lockCtx;

        rtdm_lock_get_irqsave(&uartCtx->lock, lockCtx);
        remaining = circRemainingFreeGet(&uartCtx->tx.buffHandle);

        if (0U != remaining) {
            size_t      transfer;

            transfer = min(bytes, remaining);

            if (NULL != usrInfo) {
                retval = rtdm_copy_from_user(
                    usrInfo,
                    dst,
                    src,
                    transfer);

                if (RETVAL_SUCCESS != retval) {
                    uartCtx->tx.status = UART_STATUS_FAULT_USAGE;
                    rtdm_lock_put_irqrestore(&uartCtx->lock, lockCtx);

                    break;                                                      /* Exit from loop                                           */
                }
            } else {
                memcpy(
                    dst,
                    src,
                    transfer);
            }
            written += transfer;
            bytes   -= transfer;
            src     += transfer;
            circPosHeadSet(
                &uartCtx->tx.buffHandle,
                transfer);
            dst = circMemHeadGet(
                &uartCtx->tx.buffHandle);
            cIntEnable(
                uartCtx,
                C_INT_TX);
            uartCtx->tx.pend = 0U;
            rtdm_lock_put_irqrestore(&uartCtx->lock, lockCtx);
        } else {
            cIntEnable(
                uartCtx,
                C_INT_TX);
            uartCtx->tx.pend = min(bytes, circSizeGet(&uartCtx->tx.buffHandle));
            rtdm_lock_put_irqrestore(&uartCtx->lock, lockCtx);
            retval = rtdm_event_timedwait(
                &uartCtx->tx.opr,
                uartCtx->tx.oprTimeout,
                &oprTimeSeq);

            if (RETVAL_SUCCESS != retval) {

                break;
            }
        }
    } while (0U < bytes);
    rtdm_mutex_unlock(
        &uartCtx->tx.acc);

    if (RETVAL_SUCCESS == retval) {
        retval = written;
    }

    return (retval);
}

static int handleIrq(
    rtdm_irq_t *        arg) {

    struct uartCtx *    uartCtx;
    volatile uint8_t *  io;
    int                 retval;
    enum lldIntNum      intNum;

    uartCtx = rtdm_irq_get_arg(arg, struct uartCtx);
    io = uartCtx->cache.io;
    retval = RTDM_IRQ_HANDLED;
    rtdm_lock_get(&uartCtx->lock);

    while (LLD_INT_NONE != (intNum = lldIntGet(io))) {                                          /* Loop until there are interrupts to process               */

        /*-- Receive interrupt -----------------------------------------------*/
        if ((LLD_INT_RX == intNum) || (LLD_INT_RX_TIMEOUT == intNum)) {
            size_t      occupied;

            uartCtx->rx.status = UART_STATUS_NORMAL;
            occupied = lldFIFORxOccupied(
                io);

            do {
                u16 item;

                occupied--;
                item = lldRegRd(
                    io,
                    RHR);

                if (FALSE == circIsFull(&uartCtx->rx.buffHandle)) {
                    circItemPut(
                        &uartCtx->rx.buffHandle,
                        item);

                    if (0U != uartCtx->rx.pend) {
                        uartCtx->rx.pend--;

                        if (0U == uartCtx->rx.pend) {
                            rtdm_event_pulse(
                                &uartCtx->rx.opr);
                        }
                    }
                } else {
                    cIntDisable(
                        uartCtx,
                        C_INT_RX | C_INT_RX_TIMEOUT);
                    uartCtx->rx.status = UART_STATUS_SOFT_OVERFLOW;
                    /*
                     * TODO: Flush FIFO here
                     */
                    break;
                }
            } while (0U != occupied);

        /*-- Transmit interrupt ----------------------------------------------*/
        } else if (LLD_INT_TX == intNum) {
            size_t      remaining;

            remaining = lldFIFOTxRemaining(
                io);

            do {
                remaining--;

                if (FALSE == circIsEmpty(&uartCtx->tx.buffHandle)) {
                    u16 item;

                    item = circItemGet(
                        &uartCtx->tx.buffHandle);
                    lldRegWr(
                        io,
                        wTHR,
                        item);

                    if (0U != uartCtx->tx.pend) {
                        uartCtx->tx.pend--;

                        if (0U == uartCtx->tx.pend) {
                            rtdm_event_pulse(
                                &uartCtx->tx.opr);
                        }
                    }
                } else {
                    cIntDisable(
                        uartCtx,
                        C_INT_TX);

                    break;
                }
            } while (0U != remaining);

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
            lldIntDisable(
                uartCtx->cache.io,
                LLD_INT_ALL);

            break;
        }
    }
    rtdm_lock_put(&uartCtx->lock);

    return (retval);
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
    UartDev.device_data = lldInit(
        UartDev.device_id);                                                    /* Initialize Linux device driver                           */

    if (NULL == UartDev.device_data) {
        LOG_ERR("failed to initialize port driver");

        return (-ENOTSUPP);
    }

    /*-- STATE: Xenomai device registration ----------------------------------*/
    LOG_INFO("registering device: %s, id: %d", (char *)&UartDev.device_name, UartDev.device_id);
    retval = rtdm_dev_register(
        &UartDev);

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("failed to register to Real-Time DM");
        lldTerm(
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
    LOG_WARN_IF(-EAGAIN == retval, "the device is busy with open instances");
    retval = lldTerm(
        UartDev.device_data);
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "failed terminate platform device driver");
}

module_init(moduleInit);
module_exit(moduleTerm);

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of x-16c750.c
 ******************************************************************************/
