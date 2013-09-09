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

#include "drv/x-16c750.h"
#include "drv/x-16c750_lld.h"
#include "drv/x-16c750_cfg.h"
#include "drv/x-16c750_ctrl.h"
#include "port/port.h"
#include "port/compiler.h"
#include "eds/smp.h"
#include "log.h"

/*=========================================================  LOCAL MACRO's  ==*/

#define DEF_DRV_VERSION_MAJOR           1
#define DEF_DRV_VERSION_MINOR           0
#define DEF_DRV_VERSION_PATCH           0
#define DEF_DRV_AUTHOR                  "Nenad Radulovic <nenad.radulovic@netico-group.com>"
#define DEF_DRV_DESCRIPTION             "Real-time 16C750 device driver"
#define DEF_DRV_SUPP_DEVICE             "UART 16C750A"

/**@brief       Maximum number of supported channels
 */
#define DEF_MAX_CHANNELS                10

#define CTRL_RD_QUEUE_NAME              CFG_DRV_NAME "_ctrlIn"
#define CTRL_RD_QUEUE_MSG_SIZE          20U
#define CTRL_RD_QUEUE_SIZE              10U * CTRL_RD_QUEUE_MSG_SIZE
#define CTRL_WR_QUEUE_NAME              CFG_DRV_NAME "_ctrlOut"
#define CTRL_WR_QUEUE_MSG_SIZE          20U
#define CTRL_WR_QUEUE_SIZE              10U * CTRL_WR_QUEUE_MSG_SIZE

#define TASK_MANAGE_NAME                CFG_DRV_NAME "_manage"
#define TASK_MANAGE_PRIO                99

/*======================================================  LOCAL DATA TYPES  ==*/

enum cIntNum {
    C_INT_TX            = IER_THRIT,//!< C_INT_TX
    C_INT_RX            = IER_RHRIT,//!< C_INT_RX
    C_INT_RX_TIMEOUT    = IER_RHRIT //!< C_INT_RX_TIMEOUT
};

enum taskDrvManState {
    DRV_MAN_CREATE,
    DRV_MAN_Q_RD,
    DRV_MAN_Q_WR,
    DRV_MAN_RUN
};

/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/

static void cIntEnable(
    struct uartCtx *    uartCtx,
    enum cIntNum        cIntNum);

static void cIntDisable(
    struct uartCtx *    uartCtx,
    enum cIntNum        cIntNum);

static enum cIntNum cIntEnabledGet(
    struct uartCtx *    uartCtx);

static bool_T xProtoIsValid(
    const struct protocol * proto);

static void xProtoSet(
    struct uartCtx *    uartCtx,
    const struct protocol * proto);

static bool_T ctrlMsgIsValid(
    xUartCmd_T *        cmd,
    size_t              len);

static int drvManStart(
    void);

static void drvManStop (
    void);

static void taskDrvMan (
    void *              arg);

/*=======================================================  LOCAL VARIABLES  ==*/

rtdm_task_t gTaskDrvMan;
enum taskDrvManState gDrvManState;

RT_QUEUE gQctrlRd;
RT_QUEUE gQctrlWr;

static struct uartCtx * gUartChn[DEF_MAX_CHANNELS];

/*======================================================  GLOBAL VARIABLES  ==*/

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DEF_DRV_AUTHOR);
MODULE_DESCRIPTION(DEF_DRV_DESCRIPTION);
MODULE_SUPPORTED_DEVICE(DEF_DRV_SUPP_DEVICE);

/*============================================  LOCAL FUNCTION DEFINITIONS  ==*/

static void cIntEnable(
    struct uartCtx *    uartCtx,
    enum cIntNum        cIntNum) {

    uartCtx->cache.IER |= cIntNum;
    lldRegWr(
        uartCtx->cache.io,
        wIER,
        uartCtx->cache.IER);
}

static void cIntDisable(
    struct uartCtx *    uartCtx,
    enum cIntNum        cIntNum) {

    uartCtx->cache.IER &= ~cIntNum;
    lldRegWr(
        uartCtx->cache.io,
        wIER,
        uartCtx->cache.IER);
}

static enum cIntNum cIntEnabledGet(
    struct uartCtx *    uartCtx) {

    return (uartCtx->cache.IER);
}

static bool_T xProtoIsValid(
    const struct protocol * proto) {

    /*
     * TODO: Check proto values here
     */
    return (TRUE);
}

static void xProtoSet(
    struct uartCtx *    uartCtx,
    const struct protocol *  proto) {

    (void)lldProtocolSet(
        uartCtx->cache.io,
        proto);
    memcpy(
        &uartCtx->proto,
        proto,
        sizeof(struct protocol));
}

static bool_T ctrlMsgIsValid(
    xUartCmd_T *        cmd,
    size_t              len) {

    if ((-EINVAL == len) || (-EIDRM == len) || (-EINTR == len) || (-EPERM == len) ) {
        LOG_ERR("queue error: %d", len);

        return (FALSE);
    }

    if (sizeof(xUartCmd_T) != len) {
        LOG_ERR("wrong size of control message");

        return (FALSE);
    }

    if (XUART_CMD_SIGNATURE != cmd->signature) {
        LOG_ERR("wrong signature of control message");

        return (FALSE);
    }

    return (TRUE);
}

static int drvManStart(
    void) {

    int             retval;

    gDrvManState = DRV_MAN_Q_RD;
    retval = rt_queue_create(
        &gQctrlRd,
        CTRL_RD_QUEUE_NAME,
        CTRL_RD_QUEUE_SIZE,
        Q_UNLIMITED,
        Q_PRIO | Q_SHARED);

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("failed to create %s control queue", CTRL_RD_QUEUE_NAME);

        return (retval);
    }

    gDrvManState = DRV_MAN_Q_WR;
    retval = rt_queue_create(
        &gQctrlWr,
        CTRL_WR_QUEUE_NAME,
        CTRL_WR_QUEUE_SIZE,
        Q_UNLIMITED,
        Q_PRIO | Q_SHARED);

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("failed to create %s control queue", CTRL_WR_QUEUE_NAME);

        return (retval);
    }

    gDrvManState = DRV_MAN_CREATE;
    retval = rtdm_task_init(
        &gTaskDrvMan,
        TASK_MANAGE_NAME,
        taskDrvMan,
        NULL,
        TASK_MANAGE_PRIO,
        0);

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("failed to create %s task", TASK_MANAGE_NAME);

        return (retval);
    }

    gDrvManState = DRV_MAN_RUN;

    return (retval);
}

static void drvManStop (
    void) {

    int                 retval;

    switch (gDrvManState) {
        case DRV_MAN_RUN : {
            rtdm_task_destroy(
                &gTaskDrvMan);
            /* fall down */
        }
        case DRV_MAN_CREATE : {
            retval = rt_queue_delete(
                &gQctrlWr);
            LOG_ERR_IF(RETVAL_SUCCESS != retval, "failed to delete %s queue", CTRL_WR_QUEUE_NAME);
            /* fall down */
        }
        case DRV_MAN_Q_WR : {
            retval = rt_queue_delete(
                &gQctrlRd);
            LOG_ERR_IF(RETVAL_SUCCESS != retval, "failed to delete %s queue", CTRL_RD_QUEUE_NAME);
            /* fall down */
        }
        case DRV_MAN_Q_RD : {
            break;
        }
        default : {
            LOG_ERR("internal error");
            break;
        }
    }
}



struct uartCtx * xCtxCreate(
    u8                  id) {


}

struct evtCmd {
    esEvt_T             header;
    xUartCmd_T *        cmd;
};

static void fillEvtFromCmd(
    struct evtCmd *     evtCmd,
    xUartCmd_T *        cmd) {

    evtCmd->header.id = cmd->cmdId;
    evtCmd->header.dynamic = 0U;
    ES_DBG_API_OBLIGATION(evtCmd->header.signature = EVT_SIGNATURE;);
    evtCmd->cmd = cmd;
}

static esFsm_T * getFsmFromId(u32 id) {
    esFsm_T * fsm;

    fsm = (esFsm_T *)&gUartChn[id]->fsm;

    return (fsm);
}

static void taskDrvMan (
    void *              arg) {

    char                buffRd[CTRL_RD_QUEUE_MSG_SIZE];
    u8                  i;

    for (i = 0U; i < DEF_MAX_CHANNELS; i++) {
        gUartChn[i] = NULL;
    }

    while (TRUE) {
        struct evtCmd   evt;
        ssize_t         len;
        xUartCmd_T *    cmd;
        esFsm_T *       fsm;

        len = rt_queue_read(
            &gQctrlRd,
            &buffRd,
            CTRL_RD_QUEUE_MSG_SIZE,
            TM_INFINITE);
        cmd = (xUartCmd_T *)buffRd;

        if (FALSE == ctrlMsgIsValid(cmd, len)) {
            continue;
        }
        fillEvtFromCmd(&evt, cmd);
        fsm = &gUartChn[cmd->uartId]->fsm;
        esFsmDispatch(
            fsm,
            &evt);
        rt_queue_free(
            &gQctrlRd,
            cmd);
    }
}

/*===================================  GLOBAL PRIVATE FUNCTION DEFINITIONS  ==*/
/*====================================  GLOBAL PUBLIC FUNCTION DEFINITIONS  ==*/

int __init moduleInit(
    void) {

    int                 retval;

    retval = drvManStart();

    return (retval);
}

void __exit moduleTerm(
    void) {

    drvManStop();
}

void userAssert(
    const char *    modName,
    const char *    modDesc,
    const char *    modAuthor,
    const char *    modFile,
    const char *    fn,
    const char *    expr,
    const char *    msgTxt,
    uint32_t        msgNum) {

    printk(KERN_ERR "** ASSERTION FAILED in " CFG_DRV_NAME " Real-Time driver\n");
    printk(KERN_ERR "Driver information :\n");
    printk(KERN_ERR "  Version     : %d.%d.%d", DEF_DRV_VERSION_MAJOR, DEF_DRV_VERSION_MINOR, DEF_DRV_VERSION_PATCH);
    printk(KERN_ERR "  Description : %s\n", DEF_DRV_DESCRIPTION);
    printk(KERN_ERR "  Author      : %s\n", DEF_DRV_AUTHOR);
    printk(KERN_ERR "Module information :\n");
    printk(KERN_ERR "  Module      : %s\n", modName);
    printk(KERN_ERR "  Description : %s\n", modDesc);
    printk(KERN_ERR "  Author      : %s\n", modAuthor);
    printk(KERN_ERR "  File        : %s\n", modFile);
    printk(KERN_ERR "Object information :\n");
    printk(KERN_ERR "  Function    : %s\n", fn);
    printk(KERN_ERR "  Expression  : %s\n", expr);
    printk(KERN_ERR "  Message text: %s\n", msgTxt);
    printk(KERN_ERR "  Message num : %d\n", msgNum);
    printk(KERN_ERR "--------------------\n");
}

module_init(moduleInit);
module_exit(moduleTerm);

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of x-16c750.c
 ******************************************************************************/
