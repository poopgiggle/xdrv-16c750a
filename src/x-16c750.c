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
#define DEF_Q_NAME_MAX_SIZE             10

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
    CTX_STATE_DEV_REG                                                           /**<@brief STATE_DEV_REG                                    */
};

/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/

/**@brief       Cleanup all work done by xUartCtxCreate() in case it failed
 */
static void xUartCtxCleanup(
    struct uartCtx *    uartCtx,
    enum ctxState       state);

/**@brief       Create UART context
 */
static int xUartCtxCreate(
    struct uartCtx *    uartCtx);

/**@brief       Destroy UART context
 */
static int xUartCtxDestroy(
    struct uartCtx *    uartCtx);

/**@brief       Named device open handler
 */
static int xUartOpen(
    struct rtdm_dev_context * ctx,
    rtdm_user_info_t *  usrInfo,
    int                 oflag);

/**@brief       Device close handler
 */
static int xUartClose(
    struct rtdm_dev_context * ctx,
    rtdm_user_info_t *  usrInfo);

/**@brief       IOCTL handler
 */
static int xUartIOctl(
    struct rtdm_dev_context * ctx,
    rtdm_user_info_t *  usrInfo,
    unsigned int        req,
    void __user *       args);

/**@brief       Read device handler
 */
static int xUartRd(
    struct rtdm_dev_context * ctx,
    rtdm_user_info_t *  usrInfo,
    void *              buff,
    size_t              bytes);

/**@brief       Write device handler
 */
static int xUartWr(
    struct rtdm_dev_context * ctx,
    rtdm_user_info_t *  usrInfo,
    const void *        buff,
    size_t              bytes);

static int xUartIrqHandle(
    rtdm_irq_t *        irqHandle);

/*=======================================================  LOCAL VARIABLES  ==*/

static struct rtdm_device gUartDev = {
    .struct_version     = RTDM_DEVICE_STRUCT_VER,
    .device_flags       = RTDM_NAMED_DEVICE | RTDM_EXCLUSIVE,
    .context_size       = sizeof(struct uartCtx),
    .device_name        = CFG_DRV_NAME,
    .protocol_family    = 0,
    .socket_type        = 0,
    .open_rt            = NULL,
    .open_nrt           = xUartOpen,
    .socket_rt          = NULL,
    .socket_nrt         = NULL,
    .ops                = {
        .close_rt       = NULL,
        .close_nrt      = xUartClose,
        .ioctl_rt       = xUartIOctl,
        .ioctl_nrt      = xUartIOctl,
        .select_bind    = NULL,
        .read_rt        = xUartRd,
        .read_nrt       = NULL,
        .write_rt       = xUartWr,
        .write_nrt      = NULL,
        .recvmsg_rt     = NULL,
        .recvmsg_nrt    = NULL,
        .sendmsg_rt     = NULL,
        .sendmsg_nrt    = NULL
    },
    .device_class       = RTDM_CLASS_SERIAL,
    .device_sub_class   = RTDM_SUBCLASS_16550A,
    .profile_version    = RTSER_PROFILE_VER,
    .driver_name        = CFG_DRV_NAME,
    .driver_version     = RTDM_DRIVER_VER(DEF_DRV_VERSION_MAJOR, DEF_DRV_VERSION_MINOR, DEF_DRV_VERSION_PATCH),
    .peripheral_name    = DEF_DRV_SUPP_DEVICE,
    .provider_name      = DEF_DRV_AUTHOR,
    .proc_name          = CFG_DRV_NAME,
    .device_id          = 0,
    .device_data        = NULL
};

static struct uartCtx gUartCtx;

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

        case CTX_STATE_DEV_REG : {
            LOG_INFO("reversing action: allocate internal RX buffer");
            (void)rt_heap_free(
                &uartCtx->rx.heapHandle,
                circMemBaseGet(&uartCtx->rx.buffHandle));
        }

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

static int xUartCtxCreate(
    struct uartCtx *    uartCtx) {

    enum ctxState       state;
    int                 retval;
    char                qTxName[DEF_Q_NAME_MAX_SIZE + 1U];
    char                qRxName[DEF_Q_NAME_MAX_SIZE + 1U];
    void *              tmpPtr;

    /*-- STATE: init ---------------------------------------------------------*/
    state = CTX_STATE_INIT;
    LOG_DBG("creating device context");
    scnprintf(
        qTxName,
        DEF_Q_NAME_MAX_SIZE,
        CFG_Q_TX_NAME ".%d",
        uartCtx->id);
    scnprintf(
        qRxName,
        DEF_Q_NAME_MAX_SIZE,
        CFG_Q_RX_NAME ".%d",
        uartCtx->id);

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
        &tmpPtr);

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("failed to allocate internal TX buffer");
        xUartCtxCleanup(
            uartCtx,
            state);

        return (retval);
    }
    circInit(
        &uartCtx->tx.buffHandle,
        tmpPtr,
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
        &tmpPtr);

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("failed to allocate internal RX buffer");
        xUartCtxCleanup(
            uartCtx,
            state);

        return (retval);
    }
    circInit(
        &uartCtx->rx.buffHandle,
        tmpPtr,
        CFG_DRV_BUFF_SIZE);

    /*-- Prepare UART data ---------------------------------------------------*/
    uartCtx->tx.timeout = MS_TO_NS(CFG_WAIT_WR_MS);
    uartCtx->tx.status  = UART_STATUS_NORMAL;
    uartCtx->rx.timeout = MS_TO_NS(CFG_WAIT_WR_MS);
    uartCtx->rx.status  = UART_STATUS_NORMAL;

    /*-- Prepare UART hardware -----------------------------------------------*/
    (void)lldSoftReset(
        uartCtx->io);
    (void)lldFIFOSetup(
        uartCtx->io);
    (void)lldProtocolSet(
        uartCtx->io,
        &gDefProtocol);

    /*-- STATE: Xenomai device registration ----------------------------------*/
    state  = CTX_STATE_DEV_REG;
    LOG_DBG("registering device: %s, id: %d with proc name: %s", uartCtx->rtdev->device_name, uartCtx->rtdev->device_id, uartCtx->rtdev->proc_name);
    retval = rtdm_dev_register(
            uartCtx->rtdev);

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("failed to register to Real-Time DM");
        xUartCtxCleanup(
            uartCtx,
            state);

        return (retval);
    }

    return (retval);
}

static int xUartCtxDestroy(
    struct uartCtx *    uartCtx) {

    int                 retval;

    LOG_DBG("destroying device context");
    retval = rtdm_dev_unregister(
        uartCtx->rtdev,
        CFG_WAIT_EXIT_MS);
    LOG_WARN_IF(-EAGAIN == retval, "the device is busy with open instances");
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

    return (retval);
}

static int xUartOpen(
    struct rtdm_dev_context * ctx,
    rtdm_user_info_t *  usrInfo,
    int                 oflag) {

    int                 retval;
    struct uartCtx *    uartCtx;
    rtdm_lockctx_t      lockCtx;

    uartCtx = RTDMDEVCTX_TO_UARTCTX(ctx);
    memcpy(uartCtx, &gUartCtx,sizeof(gUartCtx));
    LOG_DBG("open device");
    rtdm_lock_init(&uartCtx->lock);
    retval = rtdm_irq_request(
        &uartCtx->irqHandle,
        gPortIRQ[uartCtx->id],
        xUartIrqHandle,
        RTDM_IRQTYPE_EDGE,
        ctx->device->proc_name,
        uartCtx);

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("failed to register interrupt");

        return (retval);
    }
    rtdm_lock_get_irqsave(
        &uartCtx->lock,
        lockCtx);
    /*
     * TODO: try to clear all pending interrupts
     */
    lldIntEnable(
        uartCtx->io,
        LLD_INT_RX);
    rtdm_lock_put_irqrestore(
        &uartCtx->lock,
        lockCtx);

    return (retval);
}

static int xUartClose(
    struct rtdm_dev_context * ctx,
    rtdm_user_info_t *  usrInfo) {

    rtdm_lockctx_t      lockCtx;
    struct uartCtx *    uartCtx;
    int                 retval;

    uartCtx = RTDMDEVCTX_TO_UARTCTX(ctx);

    LOG_DBG("close device");
    rtdm_lock_get_irqsave(&uartCtx->lock, lockCtx);
    lldIntDisable(
        uartCtx->io,
        LLD_INT_TX);
    lldIntDisable(
        uartCtx->io,
        LLD_INT_RX);
    lldIntDisable(
        uartCtx->io,
        LLD_INT_RX_TIMEOUT);
    /*
     * TODO: try to clear all pending interrupts
     */
    rtdm_lock_put_irqrestore(
        &uartCtx->lock,
        lockCtx);
    retval = rtdm_irq_free(
        &uartCtx->irqHandle);

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("failed to unregister interrupt");
    }

    return (retval);
}

static int xUartConfigSet(
    struct uartCtx *    ctx,
    const struct rtser_config * config,
    uint64_t **         history) {

    return (RETVAL_SUCCESS);
}

static int xUartIOctl(
    struct rtdm_dev_context * ctx,
    rtdm_user_info_t *  usrInfo,
    unsigned int        req,
    void __user *       args) {

    struct uartCtx *    uartCtx;
    int                 retval;

    uartCtx = RTDMDEVCTX_TO_UARTCTX(ctx);
    retval = RETVAL_SUCCESS;

    switch (req) {

        default : {
            retval = -ENOTSUPP;
        }
    }

    return (retval);
}

static int xUartRd(
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

static int xUartWr(
    struct rtdm_dev_context * ctx,
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

    uartCtx = RTDMDEVCTX_TO_UARTCTX(ctx);

    if (0U == bytes) {
        return (0);
    }

    if (NULL != usrInfo) {

        if (0 == rtdm_read_user_ok(usrInfo, buff, bytes)) {

            return (-EFAULT);
        }
    }
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

        if (0U == remaining) {

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

static int xUartIrqHandle(
    rtdm_irq_t *        irqHandle) {

    struct uartCtx *    uartCtx;
    int                 retval;
    enum lldINT         intNum;

    retval = RTDM_IRQ_NONE;

    uartCtx = rtdm_irq_get_arg(irqHandle, struct uartCtx);

    rtdm_lock_get(&uartCtx->lock);

    uartCtx->rx.status = UART_STATUS_NORMAL;
    intNum = lldIntGet(
        uartCtx->io);

    while (LLD_INT_NONE != intNum) {                                            /* Loop until there are interrupts to process               */
        retval = RTDM_IRQ_HANDLED;

        /*-- Receive interrupt -----------------------------------------------*/
        if ((LLD_INT_RX_TIMEOUT == intNum) || (LLD_INT_RX == intNum)) {

            if (FALSE == circIsFull(&uartCtx->rx.buffHandle)) {
                u32 item;

                item = lldRegRd(
                    uartCtx->io,
                    RHR);
                circItemPut(
                    &uartCtx->rx.buffHandle,
                    item);
            } else {
                uartCtx->rx.status = UART_STATUS_SOFT_OVERFLOW;
                lldRegRd(
                    uartCtx->io,
                    RHR);
            }

        /*-- Transmit interrupt ----------------------------------------------*/
        } else if (LLD_INT_TX == intNum) {

            if (FALSE == circIsEmpty(&uartCtx->tx.buffHandle)) {
                u32 item;

                item = circItemGet(
                    &uartCtx->tx.buffHandle);
                lldRegWr(
                    uartCtx->io,
                    wTHR,
                    (u16)item);
            } else {
                lldIntDisable(
                    uartCtx->io,
                    LLD_INT_TX);
            }
        }
        intNum = lldIntGet(                                                     /* Get new interrupt                                        */
            uartCtx->io);
    }
    LOG_WARN_IF(UART_STATUS_SOFT_OVERFLOW == uartCtx->rx.status, "RX buffer overflow");
    rtdm_lock_put(&uartCtx->lock);

    return (retval);
}

enum moduleState {
    MOD_STATE_PORT,
    MOD_STATE_CTX
};

static void moduleCleanup(
    enum moduleState    state) {

    switch (state) {
        case MOD_STATE_CTX : {
            portTerm(
                &gUartCtx);
            /* fall */
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

    gUartCtx.id = CFG_UART;                                                     /* TODO: This must go, almost all functions are already     */
                                                                                /* parameterized                                            */
    gUartCtx.rtdev = &gUartDev;                                                 /* TODO: This must be parameterized                         */
    gUartDev.device_id = gUartCtx.id;

    state = MOD_STATE_PORT;
    LOG_INFO(DEF_DRV_DESCRIPTION);
    LOG_INFO("version: %d.%d.%d", DEF_DRV_VERSION_MAJOR, DEF_DRV_VERSION_MINOR, DEF_DRV_VERSION_PATCH);
    LOG_INFO("UART: %d", gUartCtx.id);
    LOG_INFO("Device driver name: %s", CFG_DRV_NAME);
    retval = portInit(
        &gUartCtx);                                                             /* Initialize Linux device driver                           */

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("failed to initialize kernel platform device driver");
        moduleCleanup(
            state);

        return (retval);
    }

    state = MOD_STATE_CTX;
    retval = xUartCtxCreate(
        &gUartCtx);

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("failed to create device context");
        moduleCleanup(
            state);

        return (retval);
    }

    return (retval);
}

void __exit moduleTerm(
    void) {
    int             retval;

    LOG_INFO("removing driver for UART: %d", gUartCtx.id);
    retval = xUartCtxDestroy(
        &gUartCtx);
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "failed to destroy device context");
    retval = portTerm(
        &gUartCtx);
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "failed terminate platform device driver");
}

module_init(moduleInit);
module_exit(moduleTerm);

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of x-16c750.c
 ******************************************************************************/
