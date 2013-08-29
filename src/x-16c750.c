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

/*TODO: Pogledati verzioniranje*/
#include <linux/version.h>

#include "x-16c750.h"
#include "x-16c750_lld.h"
#include "x-16c750_cfg.h"
#include "port.h"
#include "log.h"

/*=========================================================  LOCAL MACRO's  ==*/

#define DEF_DRV_VERSION_MAJOR           1
#define DEF_DRV_VERSION_MINOR           0
#define DEF_DRV_VERSION_PATCH           0
#define DEF_DRV_AUTHOR                  "Nenad Radulovic <nenad.radulovic@netico-group.com>"
#define DEF_DRV_DESCRIPTION             "Real-time 16C750 device driver"
#define DEF_DRV_SUPP_DEVICE             "UART 16C750A"
#define DEF_Q_NAME_MAX_SIZE           10

#define TO_UARTCTX(rtdm_dev_context)                                            \
    (struct uartCtx *)rtdm_dev_context->dev_private

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

/**@brief       Setup UART module to use FIFO and DMA
 */
static int xUartDMAFIFOSetup(
    struct uartCtx *    uartCtx);

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

static int xUartDMAFIFOSetup(
    struct uartCtx *    uartCtx) {

    int                 retval;
    u16                 tmp;
    u16                 regLCR;
    u16                 regEFR;
    u16                 regMCR;

    regLCR = lldRegRd(                                                          /* Switch to register configuration mode B to access the    */
        uartCtx->ioRemap,                                                       /* EFR register                                             */
        LCR);
    UART_CFG_MODE_SET(uartCtx->ioRemap, UART_CFG_MODE_B);
    regEFR = lldRegSetBits(                                                     /* Enable register submode TCR_TLR to access the TLR        */
        uartCtx->ioRemap,                                                       /* register (1/2)                                           */
        bEFR,
        EFR_ENHANCEDEN);
    UART_CFG_MODE_SET(uartCtx->ioRemap, UART_CFG_MODE_A);                       /* Switch to register configuration mode A to access the MCR*/
                                                                                /* register                                                 */
    regMCR = lldRegSetBits(                                                     /* Enable register submode TCR_TLR to access the TLR        */
        uartCtx->ioRemap,                                                       /* register (2/2)                                           */
        aMCR,
        MCR_TCRTLR);
    lldRegWr(                                                                   /* Load the new FIFO triggers (1/3) and the new DMA mode    */
        uartCtx->ioRemap,                                                       /* (1/2)                                                    */
        waFCR,
        FCR_RX_FIFO_TRIG_56 | FCR_TX_FIFO_TRIG_56 | FCR_DMA_MODE |
            FCR_TX_FIFO_CLEAR | FCR_RX_FIFO_CLEAR | FCR_FIFO_EN);
    UART_CFG_MODE_SET(uartCtx->ioRemap, UART_CFG_MODE_B);                       /* Switch to register configuration mode B to access the EFR*/
                                                                                /* register   */
    lldRegWr(                                                                   /* Load the new FIFO triggers (2/3)                         */
        uartCtx->ioRemap,
        wbTLR,
        TLR_RX_FIFO_TRIG_DMA_0 | TLR_TX_FIFO_TRIG_DMA_0);
    (void)lldRegResetBits(                                                      /* Load the new FIFO triggers (3/3) and the new DMA mode    */
        uartCtx->ioRemap,                                                       /* (2/2)                                                    */
        SCR,
        SCR_RXTRIGGRANU1 | SCR_TXTRIGGRANU1 | SCR_DMAMODE2_Mask |
            SCR_DMAMODECTL);
    tmp = regEFR & EFR_ENHANCEDEN;                                              /* Restore EFR<4> ENHANCED_EN bit                           */
    tmp |= lldRegRd(uartCtx->ioRemap, bEFR) & ~EFR_ENHANCEDEN;
    lldRegWr(
        uartCtx->ioRemap,
        bEFR,
        tmp);
    UART_CFG_MODE_SET(uartCtx->ioRemap, UART_CFG_MODE_A);                       /* Switch to register configuration mode A to access the MCR*/
                                                                                /* register                                                 */
    tmp = regMCR & MCR_TCRTLR;                                                  /* Restore MCR<6> TCRTLR bit                                */
    tmp |= lldRegRd(uartCtx->ioRemap, aMCR) & ~MCR_TCRTLR;
    lldRegWr(
        uartCtx->ioRemap,
        aMCR,
        tmp);
    lldRegWr(                                                                   /* Restore LCR                                              */
        uartCtx->ioRemap,
        LCR,
        regLCR);

    retval = portDMAInit(
        uartCtx);

    return (retval);
}

static void xUartCtxCleanup(
    struct uartCtx *    uartCtx,
    enum ctxState       state) {

    switch (state) {

        case CTX_STATE_DEV_REG : {
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
    retval = rt_queue_flush(
        &uartCtx->qRxHandle);
    LOG_WARN_IF(0 != retval, "failed to flush RX queue");
    retval = rt_queue_delete(
        &uartCtx->qRxHandle);
    LOG_WARN_IF(0 != retval, "failed to delete RX queue");
    retval = rt_queue_flush(
        &uartCtx->qTxHandle);
    LOG_WARN_IF(0 != retval, "failed to flush TX queue");
    retval = rt_queue_delete(
        &uartCtx->qTxHandle);
    LOG_WARN_IF(0 != retval, "failed to delete TX queue");

    return (retval);
}

static int xUartOpen(
    struct rtdm_dev_context * ctx,
    rtdm_user_info_t *  usrInfo,
    int                 oflag) {

    int                 retval;
    struct uartCtx *    uartCtx;
    rtdm_lockctx_t      lockCtx;

    uartCtx = TO_UARTCTX(ctx);
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
    rtdm_lock_put_irqrestore(
        &uartCtx->lock,
        lockCtx);

    return (retval);
}

static int xUartClose(
    struct rtdm_dev_context * ctx,
    rtdm_user_info_t *  usrInfo) {

    return (RETVAL_SUCCESS);
}

static int xUartIOctl(
    struct rtdm_dev_context * ctx,
    rtdm_user_info_t *  usrInfo,
    unsigned int        req,
    void __user *       args) {

    return (RETVAL_SUCCESS);
}

static int xUartRd(
    struct rtdm_dev_context * ctx,
    rtdm_user_info_t *  usrInfo,
    void *              buff,
    size_t              bytes) {

    return (RETVAL_SUCCESS);
}

static int xUartWr(
    struct rtdm_dev_context * ctx,
    rtdm_user_info_t *  usrInfo,
    const void *        buff,
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
