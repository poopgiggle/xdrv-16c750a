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
#include <linux/ioport.h>

#include "x-16c750.h"
#include "x-16c750_lld.h"
#include "x-16c750_cfg.h"
#include "x-16c750_ioctl.h"
#include "port.h"
#include "log.h"

/*=========================================================  LOCAL MACRO's  ==*/

#define DEF_DRV_VERSION_MAJOR           1
#define DEF_DRV_VERSION_MINOR           0
#define DEF_DRV_VERSION_PATCH           0
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

#define RTDMDEVCTX_TO_UARTCTX(rtdm_dev_context)                                 \
    (struct uartCtx *)rtdm_dev_context->dev_private

/*======================================================  LOCAL DATA TYPES  ==*/

/**@brief       States of the context process
 */
enum ctxState {
    CTX_STATE_INIT,                                                             /**<@brief STATE_INIT                                       */
    CTX_STATE_TX_ALLOC,                                                         /**<@brief STATE_TX_ALLOC                                   */
    CTX_STATE_RX_ALLOC,                                                         /**<@brief STATE_RX_ALLOC                                   */
    CTX_STATE_TX_BUFF,
    CTX_STATE_TX_BUFF_ALLOC,
    CTX_STATE_RX_BUFF,
    CTX_STATE_RX_BUFF_ALLOC,
};

enum moduleState {
    MOD_STATE_PORT,
    MOD_STATE_DEV_REG
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
    volatile u8 *       io,
    u32                 id);

/**@brief       Destroy UART context
 */
static int xUartCtxTerm(
    struct uartCtx *    uartCtx);

static void xUartProtoSet(
    struct uartCtx *    uartCtx,
    const struct xUartProto *  proto);

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
    void __user *       arg);

/**@brief       Read device handler
 */
static int handleRd(
    struct rtdm_dev_context * ctx,
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

static void moduleCleanup(
    enum moduleState    state);

/*=======================================================  LOCAL VARIABLES  ==*/

static struct rtdm_device gUartDev = {
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
            LOG_INFO("reversing action: RX allocate");
            (void)rt_queue_flush(
                &uartCtx->rx.queueHandle);
            (void)rt_queue_delete(
                &uartCtx->rx.queueHandle);
            /* fall through */
        }

        case CTX_STATE_RX_ALLOC: {
            LOG_INFO("reversing action: TX allocate");
            (void)rt_queue_flush(
                &uartCtx->tx.queueHandle);
            (void)rt_queue_delete(
                &uartCtx->tx.queueHandle);
            /* fall through */
        }

        case CTX_STATE_TX_ALLOC: {
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
    volatile u8 *       io,
    u32                 id) {

    char                qTxName[CFG_Q_NAME_MAX_SIZE + 1U];
    char                qRxName[CFG_Q_NAME_MAX_SIZE + 1U];
    enum ctxState       state;
    int                 retval;
    void *              buff;

    /*-- STATE: init ---------------------------------------------------------*/
    state = CTX_STATE_INIT;
    LOG_DBG("creating device context");
    scnprintf(
        qTxName,
        CFG_Q_NAME_MAX_SIZE,
        CFG_Q_TX_NAME ".%d",
        id);
    scnprintf(
        qRxName,
        CFG_Q_NAME_MAX_SIZE,
        CFG_Q_RX_NAME ".%d",
        id);

    /*-- STATE: TX allocate --------------------------------------------------*/
    state = CTX_STATE_TX_ALLOC;
    LOG_INFO("TX queue: %s, size: %ld", qTxName, CFG_Q_TX_SIZE);
    retval = rt_queue_create(
        &uartCtx->tx.queueHandle,
        qTxName,
        CFG_Q_TX_SIZE,
        Q_UNLIMITED,
        Q_PRIO | Q_SHARED);

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("failed to create queue");
        xUartCtxCleanup(
            uartCtx,
            state);

        return (retval);
    }

    /*-- STATE: RX allocate --------------------------------------------------*/
    state = CTX_STATE_RX_ALLOC;
    LOG_INFO("RX queue: %s, size: %ld", qRxName, CFG_Q_RX_SIZE);
    retval = rt_queue_create(
        &uartCtx->rx.queueHandle,
        qRxName,
        CFG_Q_RX_SIZE,
        Q_UNLIMITED,
        Q_PRIO | Q_SHARED);

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("failed to create queue");
        xUartCtxCleanup(
            uartCtx,
            state);

        return (retval);
    }

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
    uartCtx->ioCache = io;
    uartCtx->idCache = id;
    uartCtx->tx.timeout = MS_TO_NS(CFG_WAIT_WR_MS);
    uartCtx->tx.status  = UART_STATUS_NORMAL;
    uartCtx->rx.timeout = MS_TO_NS(CFG_WAIT_WR_MS);
    uartCtx->rx.status  = UART_STATUS_NORMAL;
    uartCtx->signature  = UART_CTX_SIGNATURE;

    return (retval);
}

static int xUartCtxTerm(
    struct uartCtx *    uartCtx) {

    int                 retval;

    LOG_DBG("destroying device context");
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
    retval = rt_queue_flush(
        &uartCtx->rx.queueHandle);
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "failed to flush RX queue");
    retval = rt_queue_delete(
        &uartCtx->rx.queueHandle);
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "failed to delete RX queue");
    retval = rt_queue_flush(
        &uartCtx->tx.queueHandle);
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "failed to flush TX queue");
    retval = rt_queue_delete(
        &uartCtx->tx.queueHandle);
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "failed to delete TX queue");
    uartCtx->signature = ~UART_CTX_SIGNATURE;

    return (retval);
}

static BOOLEAN xUartIsProtoValid(
    const struct xUartProto * proto) {

    return (TRUE);
}

static void xUartProtoSet(
    struct uartCtx *    uartCtx,
    const struct xUartProto *  proto) {

    (void)lldProtocolSet(
        uartCtx->ioCache,
        proto);
    memcpy(
        &uartCtx->proto,
        proto,
        sizeof(struct xUartProto));
}

static struct uartCtx * uartCtxFromDevCtx(
    struct rtdm_dev_context * devCtx) {

    return ((struct uartCtx *)devCtx->dev_private);
}

static int handleOpen(
    struct rtdm_dev_context * devCtx,
    rtdm_user_info_t *  usrInfo,
    int                 oflag) {

    int                 retval;
    struct uartCtx *    uartCtx;
    rtdm_lockctx_t      lockCtx;
    volatile u8 *       io;

    uartCtx = uartCtxFromDevCtx(
        devCtx);
    io = portIORemapGet(devCtx->device->device_data);
    LOG_INFO("open UART: %d", devCtx->device->device_id);
    rtdm_lock_init(&uartCtx->lock);
    rtdm_lock_get_irqsave(
        &uartCtx->lock,
        lockCtx);
    retval = xUartCtxInit(
        uartCtx,
        io,
        devCtx->device->device_id);
    xUartProtoSet(
        uartCtx,
        &gDefProtocol);
    (void)lldSoftReset(
        io);
    (void)lldFIFOSetup(
        io);

    if (RETVAL_SUCCESS != retval) {
        rtdm_lock_put_irqrestore(
            &uartCtx->lock,
            lockCtx);
        LOG_ERR("failed to create device context");

        return (retval);
    }
    retval = rtdm_irq_request(
        &uartCtx->irqHandle,
        gPortIRQ[devCtx->device->device_id],
        handleIrq,
        RTDM_IRQTYPE_EDGE,
        devCtx->device->proc_name,
        uartCtx);

    if (RETVAL_SUCCESS != retval) {
        rtdm_lock_put_irqrestore(
            &uartCtx->lock,
            lockCtx);
        LOG_ERR("failed to register interrupt");

        return (retval);
    }
    lldIntEnable(
        io,
        LLD_INT_RX);
    rtdm_lock_put_irqrestore(
        &uartCtx->lock,
        lockCtx);

    return (retval);
}

static int handleClose(
    struct rtdm_dev_context * devCtx,
    rtdm_user_info_t *  usrInfo) {

    struct uartCtx *    uartCtx;
    int                 retval;

    retval = RETVAL_SUCCESS;
    uartCtx = uartCtxFromDevCtx(devCtx);

    if (UART_CTX_SIGNATURE == uartCtx->signature) {
        int             retval2;
        rtdm_lockctx_t  lockCtx;
        volatile u8 *   io;

        LOG_INFO("close UART: %d", devCtx->device->device_id);
        rtdm_lock_get_irqsave(&uartCtx->lock, lockCtx);
        io = uartCtx->ioCache;
        lldIntDisable(                                                              /* Turn off all interrupts                                  */
            io,
            LLD_INT_TX);
        lldIntDisable(
            io,
            LLD_INT_RX);
        lldIntDisable(
            io,
            LLD_INT_RX_TIMEOUT);
        retval = rtdm_irq_free(
            &uartCtx->irqHandle);
        retval2 = xUartCtxTerm(
                uartCtx);
        rtdm_lock_put_irqrestore(
            &uartCtx->lock,
            lockCtx);
        LOG_ERR_IF(RETVAL_SUCCESS != retval, "failed to unregister interrupt");
        LOG_ERR_IF(RETVAL_SUCCESS != retval2, "failed to destroy device context");
    }

    return (retval);
}

static int handleIOctl(
    struct rtdm_dev_context * devCtx,
    rtdm_user_info_t *  usrInfo,
    unsigned int        req,
    void __user *       arg) {

    struct uartCtx *    uartCtx;
    int                 retval;

    uartCtx = uartCtxFromDevCtx(devCtx);
    retval = RETVAL_SUCCESS;

    switch (req) {
        case XUART_PROTOCOL_GET : {

            if (NULL != usrInfo) {
                retval = rtdm_safe_copy_to_user(
                    usrInfo,
                    arg,
                    &uartCtx->proto,
                    sizeof(struct xUartProto));
            } else {
                memcpy(
                    arg,
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
                    arg,
                    sizeof(struct xUartProto));
            } else {
                memcpy(
                    &proto,
                    arg,
                    sizeof(struct xUartProto));
            }

            if (TRUE == xUartIsProtoValid(&proto)) {
                rtdm_lockctx_t  lockCtx;

                rtdm_lock_get_irqsave(&uartCtx->lock, lockCtx);
                xUartProtoSet(
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
    struct rtdm_dev_context * ctx,
    rtdm_user_info_t *  usrInfo,
    void *              buff,
    size_t              bytes) {

    struct uartCtx *    uartCtx;
    uartCtx = RTDMDEVCTX_TO_UARTCTX(ctx);

    if (NULL != usrInfo) {
    }
    return (RETVAL_SUCCESS);
}

static int handleWr(
    struct rtdm_dev_context * devCtx,
    rtdm_user_info_t *  usrInfo,
    const void *        buff,
    size_t              bytes) {

    struct uartCtx *    uartCtx;
    rtdm_toseq_t        timeoutSeq;
    rtdm_lockctx_t      lockCtx;
    int                 retval;
    u8 *                src;
    u8 *                dst;
    size_t              written;

    if (NULL != usrInfo) {

        if (0 == rtdm_read_user_ok(usrInfo, buff, bytes)) {

            return (-EFAULT);
        }
    }
    uartCtx = uartCtxFromDevCtx(devCtx);
    rtdm_toseq_init(
        &timeoutSeq,
        uartCtx->rx.timeout);
    retval = rtdm_mutex_timedlock(
        &uartCtx->rx.mtx,
        uartCtx->rx.timeout,
        &timeoutSeq);

    if (RETVAL_SUCCESS != retval) {

        return (retval);
    }
    written = 0U;
    src = (u8 *)buff;
    dst = circMemHeadGet(
        &uartCtx->tx.buffHandle);

    while (0 < bytes) {
        size_t          remaining;
        size_t          transfer;

        rtdm_lock_get_irqsave(&uartCtx->lock, lockCtx);
        remaining = circRemainingGet(
            &uartCtx->tx.buffHandle);

        if (0U != remaining) {

            if (remaining < bytes) {
                transfer = remaining;
            } else {
                transfer = bytes;
            }

            if (NULL != usrInfo) {
                retval = rtdm_copy_from_user(
                    usrInfo,
                    dst,
                    src,
                    transfer);

                if (RETVAL_SUCCESS != retval) {
                    retval = -EFAULT;
                    bytes  = 0;                                                 /* Exit from loop                                           */
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
            circHeadPosSet(
                &uartCtx->tx.buffHandle,
                transfer);
            dst = circMemHeadGet(
                &uartCtx->tx.buffHandle);
        } else {
            retval = -ENOBUFS;
            bytes  = 0;                                                         /* Exit from loop                                           */
        }
        rtdm_lock_put_irqrestore(&uartCtx->lock, lockCtx);
    }
    rtdm_mutex_unlock(
        &uartCtx->rx.mtx);

    if (RETVAL_SUCCESS == retval) {

        retval = written;
    }
    return (retval);
}

static int handleIrq(
    rtdm_irq_t *        arg) {

    struct uartCtx *    uartCtx;
    volatile u8 *       io;
    int                 retval;
    enum lldINT         intNum;

    retval = RTDM_IRQ_NONE;

    uartCtx = rtdm_irq_get_arg(arg, struct uartCtx);
    io = uartCtx->ioCache;
    rtdm_lock_get(&uartCtx->lock);

    uartCtx->rx.status = UART_STATUS_NORMAL;
    intNum = lldIntGet(
        io);

    while (LLD_INT_NONE != intNum) {                                            /* Loop until there are interrupts to process               */
        retval = RTDM_IRQ_HANDLED;

        /*-- Receive interrupt -----------------------------------------------*/
        if ((LLD_INT_RX_TIMEOUT == intNum) || (LLD_INT_RX == intNum)) {

            if (FALSE == circIsFull(&uartCtx->rx.buffHandle)) {
                u32 item;

                item = lldRegRd(
                    io,
                    RHR);
                circItemPut(
                    &uartCtx->rx.buffHandle,
                    item);
            } else {
                uartCtx->rx.status = UART_STATUS_SOFT_OVERFLOW;
                lldRegRd(
                    io,
                    RHR);
            }

        /*-- Transmit interrupt ----------------------------------------------*/
        } else if (LLD_INT_TX == intNum) {

            if (FALSE == circIsEmpty(&uartCtx->tx.buffHandle)) {
                u32 item;

                item = circItemGet(
                    &uartCtx->tx.buffHandle);
                lldRegWr(
                    io,
                    wTHR,
                    (u16)item);
            } else {
                lldIntDisable(
                    io,
                    LLD_INT_TX);
            }
        }
        intNum = lldIntGet(                                                     /* Get new interrupt                                        */
            io);
    }
    LOG_WARN_IF(UART_STATUS_SOFT_OVERFLOW == uartCtx->rx.status, "RX buffer overflow");
    rtdm_lock_put(&uartCtx->lock);

    return (retval);
}

static void moduleCleanup(
    enum moduleState    state) {

    switch (state) {
        case MOD_STATE_DEV_REG : {
            portTerm(
                gUartDev.device_data);
            /* fall down */
        }

        case MOD_STATE_PORT : {
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

int __init moduleInit(
    void) {

    int                 retval;
    enum moduleState    state;

    gUartDev.device_id = CFG_UART_ID;
    memcpy(&gUartDev.device_name, CFG_DRV_NAME, sizeof(CFG_DRV_NAME));

    state = MOD_STATE_PORT;
    LOG_INFO(DEF_DRV_DESCRIPTION);
    LOG_INFO("version: %d.%d.%d", DEF_DRV_VERSION_MAJOR, DEF_DRV_VERSION_MINOR, DEF_DRV_VERSION_PATCH);
    gUartDev.device_data = portInit(
        gUartDev.device_id);                                                    /* Initialize Linux device driver                           */

    if (NULL == gUartDev.device_data) {
        LOG_ERR("failed to initialize port driver");
        moduleCleanup(
            state);

        return (-ENOTSUPP);
    }

    /*-- STATE: Xenomai device registration ----------------------------------*/
    state  = MOD_STATE_DEV_REG;
    LOG_INFO("registering device: %s, id: %d", (char *)&gUartDev.device_name, gUartDev.device_id);
    retval = rtdm_dev_register(
            &gUartDev);

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("failed to register to Real-Time DM");
        moduleCleanup(
            state);

        return (retval);
    }

    return (retval);
}

void __exit moduleTerm(
    void) {
    int             retval;

    LOG_INFO("removing driver for UART: %d", gUartDev.device_id);
    retval = rtdm_dev_unregister(
        &gUartDev,
        CFG_WAIT_EXIT_MS);
    LOG_WARN_IF(-EAGAIN == retval, "the device is busy with open instances");
    retval = portTerm(
        gUartDev.device_data);
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "failed terminate platform device driver");
}

module_init(moduleInit);
module_exit(moduleTerm);

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of x-16c750.c
 ******************************************************************************/
