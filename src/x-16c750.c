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
    .device_sub_class   = RTDM_SUBCLASS_GENERIC,
    .profile_version    = RTSER_PROFILE_VER,
    .driver_name        = CFG_DRV_NAME,
    .driver_version     = RTDM_DRIVER_VER(DEF_DRV_VERSION_MAJOR, DEF_DRV_VERSION_MINOR, DEF_DRV_VERSION_PATCH),
    .peripheral_name    = DEF_DRV_SUPP_DEVICE,
    .provider_name      = DEF_DRV_AUTHOR,
    .proc_name          = "xuart",
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
                &uartCtx->rxHeapHandle,
                circBuffGet(&uartCtx->buffRxHandle));
        }

        case CTX_STATE_RX_BUFF_ALLOC: {
            LOG_INFO("reversing action: create internal RX buffer");
            (void)rt_heap_delete(
                &uartCtx->rxHeapHandle);
        }

        case CTX_STATE_RX_BUFF: {
            LOG_INFO("reversing action: allocate internal TX buffer");
            (void)rt_heap_free(
                &uartCtx->txHeapHandle,
                circBuffGet(&uartCtx->buffTxHandle));
        }

        case CTX_STATE_TX_BUFF_ALLOC: {
            LOG_INFO("reversing action: create internal TX buffer");
            (void)rt_heap_delete(
                &uartCtx->txHeapHandle);
        }

        case CTX_STATE_TX_BUFF : {
            LOG_INFO("reversing action: RX allocate");
            (void)rt_queue_flush(
                &uartCtx->qRxHandle);
            (void)rt_queue_delete(
                &uartCtx->qRxHandle);
            /* fall through */
        }

        case CTX_STATE_RX_ALLOC: {
            LOG_INFO("reversing action: TX allocate");
            (void)rt_queue_flush(
                &uartCtx->qTxHandle);
            (void)rt_queue_delete(
                &uartCtx->qTxHandle);
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
        &uartCtx->qTxHandle,
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
        &uartCtx->qRxHandle,
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
        &uartCtx->txHeapHandle,
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
        &uartCtx->txHeapHandle,
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
        &uartCtx->buffTxHandle,
        tmpPtr,
        CFG_DRV_BUFF_SIZE);

    /*-- STATE: Create RX buffer ---------------------------------------------*/
    state  = CTX_STATE_RX_BUFF;
    retval = rt_heap_create(
        &uartCtx->rxHeapHandle,
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
        &uartCtx->rxHeapHandle,
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
        &uartCtx->buffRxHandle,
        tmpPtr,
        CFG_DRV_BUFF_SIZE);

    /*-- STATE: Xenomai device registration ----------------------------------*/
    state  = CTX_STATE_DEV_REG;
    retval = rtdm_dev_register(
            uartCtx->rtdev);

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("failed to register to Real-Time DM");
        xUartCtxCleanup(
            uartCtx,
            state);

        return (retval);
    }

    /*-- Prepare UART --------------------------------------------------------*/
    (void)lldSoftReset(
        uartCtx->io);
    (void)lldFIFOSetup(
        uartCtx->io);
    (void)lldProtocolSet(
        uartCtx->io,
        &gDefProtocol);

    return (retval);
}

static int xUartCtxDestroy(
    struct uartCtx *    uartCtx) {

    int                 retval;

    LOG_DBG("destroying device context");
    retval = rtdm_dev_unregister(
        &gUartDev,
        CFG_WAIT_EXIT_DELAY);
    LOG_WARN_IF(-EAGAIN == retval, "the device is busy with open instances");
    retval = rt_heap_free(
        &uartCtx->rxHeapHandle,
        circBuffGet(&uartCtx->buffRxHandle));
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "failed to free internal RX buffer");
    retval = rt_heap_delete(
        &uartCtx->rxHeapHandle);
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "failed to delete internal RX buffer");
    retval = rt_heap_free(
        &uartCtx->txHeapHandle,
        circBuffGet(&uartCtx->buffTxHandle));
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "failed to free internal TX buffer");
    retval = rt_heap_delete(
        &uartCtx->txHeapHandle);
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "failed to delete internal TX buffer");
    retval = rt_queue_flush(
        &uartCtx->qRxHandle);
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "failed to flush RX queue");
    retval = rt_queue_delete(
        &uartCtx->qRxHandle);
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "failed to delete RX queue");
    retval = rt_queue_flush(
        &uartCtx->qTxHandle);
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "failed to flush TX queue");
    retval = rt_queue_delete(
        &uartCtx->qTxHandle);
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
    uartCtx = RTDMDEVCTX_TO_UARTCTX(ctx);


    return (RETVAL_SUCCESS);
}

static int xUartIrqHandle(
    rtdm_irq_t *        irqHandle) {

    struct uartCtx *    uartCtx;
    int                 retval;
    enum lldINT         intNum;

    retval = RTDM_IRQ_NONE;

    uartCtx = rtdm_irq_get_arg(irqHandle, struct uartCtx);

    rtdm_lock_get(&uartCtx->lock);

    intNum = lldIntGet(
        uartCtx->io);

    while (LLD_INT_NONE != intNum) {

        if (LLD_INT_RX_TIMEOUT == intNum) {

        } else if (LLD_INT_RX == intNum) {


        } else if (LLD_INT_TX == intNum) {

        }
        intNum = lldIntGet(
            uartCtx->io);
    }
    rtdm_lock_put(&uartCtx->lock);

    return (retval);
}

/*===================================  GLOBAL PRIVATE FUNCTION DEFINITIONS  ==*/
/*====================================  GLOBAL PUBLIC FUNCTION DEFINITIONS  ==*/

int __init moduleInit(
    void) {

    int                 retval;

    gUartCtx.id = CFG_UART;                                                     /* TODO: This must go, almost all functions are already     */
                                                                                /* parameterized                                            */
    gUartCtx.rtdev = &gUartDev;                                                 /* TODO: This must be parameterized                         */
    gUartDev.device_id = gUartCtx.id;

    LOG_INFO(DEF_DRV_DESCRIPTION);
    LOG_INFO("version: %d.%d.%d", DEF_DRV_VERSION_MAJOR, DEF_DRV_VERSION_MINOR, DEF_DRV_VERSION_PATCH);
    LOG_INFO("UART: %d", gUartCtx.id);
    retval = portInit(
        &gUartCtx);                                                             /* Initialize Linux device driver                           */

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("failed to initialize kernel platform device driver");

        return (retval);
    }
    retval = xUartCtxCreate(
        &gUartCtx);

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("failed to create device context");

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
