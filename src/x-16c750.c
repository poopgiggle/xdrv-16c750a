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
 * @brief       Driver for 16C750 compatible UARTs
 *********************************************************************//** @{ */

/*=========================================================  INCLUDE FILES  ==*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/ioport.h>

/*TODO: Pogledati verzioniranje*/
#include <linux/version.h>

#include "x-16c750.h"
#include "x-16c750_regs.h"
#include "x-16c750_cfg.h"
#include "plat.h"
#include "log.h"

/*=========================================================  LOCAL MACRO's  ==*/

#define DEF_DRV_VERSION_MAJOR           1
#define DEF_DRV_VERSION_MINOR           0
#define DEF_DRV_VERSION_PATCH           0

/*======================================================  LOCAL DATA TYPES  ==*/

/**@brief       States of the context process
 */
enum ctxState {
    CTX_STATE_INIT,                                                             /**<@brief STATE_INIT                                       */
    CTX_STATE_TX_ALLOC,                                                         /**<@brief STATE_TX_ALLOC                                   */
    CTX_STATE_RX_ALLOC,                                                         /**<@brief STATE_RX_ALLOC                                   */
    CTX_STATE_DEV_REG                                                           /**<@brief STATE_DEV_REG                                    */
};

/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/

static void hwUartRegWr(
    struct uartCtx *    uartCtx,
    enum hwUartRegsWr   reg,
    unsigned int        val);

static int hwUartRegRd(
    struct uartCtx *    uartCtx,
    enum hwUartRegsRd   reg);

static void xUartCtxCleanup(
    struct uartCtx *    uartCtx,
    enum ctxState       state);

static int xUartCtxCreate(
    struct uartCtx *    uartCtx);

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
    void *              buff,
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
        .write_rt       = NULL,
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
    .peripheral_name    = "UART 16C750",
    .provider_name      = "Nenad Radulovic",
    .proc_name          = "xuart",
    .device_id          = 0,
    .device_data        = NULL
};

static struct uartCtx gUartCtx;

/*======================================================  GLOBAL VARIABLES  ==*/

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nenad Radulovic <nenad.radulovic@netico-group.com>");
MODULE_DESCRIPTION("Real-time 16C750 device driver");
MODULE_SUPPORTED_DEVICE("16C750");

/*============================================  LOCAL FUNCTION DEFINITIONS  ==*/

static void hwUartRegWr(
    struct uartCtx *    uartCtx,
    enum hwUartRegsWr   reg,
    unsigned int        val) {

    LOG_INFO("write base: %p, off: %d, addr: %p  val: %d", (void *)uartCtx->ioremap, reg, (unsigned char *)uartCtx->ioremap + (unsigned long)reg, val);
    __raw_writew(val, (unsigned char *)uartCtx->ioremap + (unsigned long)reg);
}

static int hwUartRegRd(
    struct uartCtx *    uartCtx,
    enum hwUartRegsRd   reg) {

    int                 retval;

    retval = __raw_readw((unsigned char *)uartCtx->ioremap + (unsigned long)reg);
    LOG_INFO("read  base: %p, off: %d, addr: %p,  val: %d", (void *)uartCtx->ioremap, reg, (unsigned char *)uartCtx->ioremap + (unsigned long)reg, retval);

    return (retval);
}

/**@brief       In case CTX create was not successful we have to undo some stuff
 */
static void xUartCtxCleanup(
    struct uartCtx *    uartCtx,
    enum ctxState       state) {

    switch (state) {

        case CTX_STATE_DEV_REG : {
            (void)rt_queue_flush(
                &uartCtx->buffRxHandle);
            (void)rt_queue_delete(
                &uartCtx->buffTxHandle);
            /* fall through */
        }

        case CTX_STATE_RX_ALLOC: {
            (void)rt_queue_flush(
                &uartCtx->buffTxHandle);
            (void)rt_queue_delete(
                &uartCtx->buffTxHandle);
            /* fall through */
        }

        case CTX_STATE_TX_ALLOC: {
            /* nothing */
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

    /*-- STATE: init ---------------------------------------------------------*/
    state = CTX_STATE_INIT;
    LOG_INFO("Creating device context");

    /*-- STATE: TX allocate --------------------------------------------------*/
    state = CTX_STATE_TX_ALLOC;
    LOG_INFO("TX buffer: %s, size: %ld", CFG_BUFF_TX_NAME, CFG_BUFF_TX_SIZE);
    retval = rt_queue_create(
        &uartCtx->buffTxHandle,
        CFG_BUFF_TX_NAME,
        CFG_BUFF_TX_SIZE,
        Q_UNLIMITED,
        Q_PRIO | Q_SHARED);

    if (RETVAL_SUCCESS != retval) {
        LOG_WARN("failed to create buffer");
        xUartCtxCleanup(
            uartCtx,
            state);

        return (retval);
    }

    /*-- STATE: RX allocate --------------------------------------------------*/
    state = CTX_STATE_RX_ALLOC;
    LOG_INFO("RX buffer: %s, size: %ld", CFG_BUFF_RX_NAME, CFG_BUFF_RX_SIZE);
    retval = rt_queue_create(
        &uartCtx->buffRxHandle,
        CFG_BUFF_RX_NAME,
        CFG_BUFF_RX_SIZE,
        Q_UNLIMITED,
        Q_PRIO | Q_SHARED);

    if (RETVAL_SUCCESS != retval) {
        LOG_WARN("failed to create buffer");
        xUartCtxCleanup(
            uartCtx,
            state);

        return (retval);
    }

    /*-- STATE: Xenomai device registration ----------------------------------*/
    state  = CTX_STATE_DEV_REG;
    retval = rtdm_dev_register(
            uartCtx->rtdev);

    if (RETVAL_SUCCESS != retval) {
        LOG_WARN("could not register XENO device driver");
        xUartCtxCleanup(
            uartCtx,
            state);

        return (retval);
    }
    hwUartRegWr(
        uartCtx,
        wLCR,
        0U);
    hwUartRegWr(
        uartCtx,
        wIER,
        0U);
    retval = hwUartRegRd(
        uartCtx,
        rIER);

    return (retval);
}

static int xUartCtxDestroy(
    struct uartCtx *    uartCtx) {

    int                 retval;

    retval = rtdm_dev_unregister(
        &gUartDev,
        CFG_WAIT_EXIT_DELAY);
    LOG_WARN_IF(-EAGAIN == retval, "the device is busy with open instances");
    retval = rt_queue_flush(
        &uartCtx->buffRxHandle);
    LOG_WARN_IF(0 != retval, "failed to flush RX buffer");
    retval = rt_queue_delete(
        &uartCtx->buffRxHandle);
    LOG_WARN_IF(0 != retval, "failed to delete RX buffer");
    retval = rt_queue_flush(
        &uartCtx->buffTxHandle);
    LOG_WARN_IF(0 != retval, "failed to flush TX buffer");
    retval = rt_queue_delete(
        &uartCtx->buffTxHandle);
    LOG_WARN_IF(0 != retval, "failed to delete TX buffer");

    return (retval);
}

/**@brief       Named device open handler
 * @details     This function will setup LATE real-time aspects of driver
 */
static int xUartOpen(
    struct rtdm_dev_context * ctx,
    rtdm_user_info_t *  usrInfo,
    int                 oflag) {

    int                 retval;
    struct uartCtx *    uartCtx;
    rtdm_lockctx_t      lockCtx;

    uartCtx = (struct uartCtx *)ctx->dev_private;
    rtdm_lock_init(&uartCtx->lock);
    retval = rtdm_irq_request(
        &uartCtx->irqHandle,
        gHwIrqNum[uartCtx->id],
        xUartIrqHandle,
        RTDM_IRQTYPE_EDGE,
        ctx->device->proc_name,
        uartCtx);

    if (RETVAL_SUCCESS != retval) {
        LOG_WARN("could not register interrupt");

        return (retval);
    }
    rtdm_lock_get_irqsave(
        &uartCtx->lock,
        lockCtx);
    rtdm_lock_put_irqrestore(
        &uartCtx->lock,
        lockCtx);

    return (retval);
}

/**@brief       Device close handler
 */
static int xUartClose(
    struct rtdm_dev_context * ctx,
    rtdm_user_info_t *  usrInfo) {

    return (RETVAL_SUCCESS);
}

/**@brief       IOCTL handler
 */
static int xUartIOctl(
    struct rtdm_dev_context * ctx,
    rtdm_user_info_t *  usrInfo,
    unsigned int        req,
    void __user *       args) {

    return (RETVAL_SUCCESS);
}

/**@brief       Read device handler
 */
static int xUartRd(
    struct rtdm_dev_context * ctx,
    rtdm_user_info_t *  usrInfo,
    void *              buff,
    size_t              bytes) {

    return (RETVAL_SUCCESS);
}

/**@brief       Write device handler
 */
static int xUartWr(
    struct rtdm_dev_context * ctx,
    rtdm_user_info_t *  usrInfo,
    void *              buff,
    size_t              bytes) {

    return (RETVAL_SUCCESS);
}

static int xUartIrqHandle(
    rtdm_irq_t *        irqHandle) {

    return (RETVAL_SUCCESS);
}

/*===================================  GLOBAL PRIVATE FUNCTION DEFINITIONS  ==*/
/*====================================  GLOBAL PUBLIC FUNCTION DEFINITIONS  ==*/

int __init moduleInit(
    void) {

    int                 retval;

    gUartCtx.id = CFG_UART;                                                     /* TODO: This must go, almost all functions are parameterized   */
    gUartCtx.rtdev = &gUartDev;                                                 /* TODO: This must be parameterized                         */
    LOG_INFO("Real-Time driver for UART: %d", gUartCtx.id);
    retval = platInit(
        &gUartCtx);                                                             /* Initialize Linux device driver                           */

    if (RETVAL_SUCCESS != retval) {
        LOG_WARN("could not initialize kernel platform device driver");

        return (retval);
    }
    retval = xUartCtxCreate(
        &gUartCtx);

    if (RETVAL_SUCCESS != retval) {
        LOG_WARN("device context creation failed");

        return (retval);
    }

    return (retval);
}

void __exit moduleTerm(
    void) {
    int             retval;

    LOG_INFO("removing driver for UART: %d", CFG_UART);
    retval = xUartCtxDestroy(
        &gUartCtx);
    LOG_WARN_IF(RETVAL_SUCCESS != retval, "context destroy failed");
    retval = platTerm(
        &gUartCtx);
}

module_init(moduleInit);
module_exit(moduleTerm);

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of x-16c750.c
 ******************************************************************************/