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

#include "drv/x-16c750.h"
#include "drv/x-16c750_fsm.h"
#include "drv/x-16c750_cfg.h"
#include "drv/x-16c750_ctrl.h"
#include "port/port.h"
#include "port/compiler.h"
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
#define DEF_MAX_CHANNELS                10U

#define CTRL_QUEUE_NAME                 CFG_DRV_NAME "_ctrl"
#define CTRL_QUEUE_MSG_SIZE             sizeof(struct xUartCmd)
#define CTRL_QUEUE_SIZE                 50U * CTRL_QUEUE_MSG_SIZE

#define TASK_MANAGE_NAME                CFG_DRV_NAME "_manager"
#define TASK_MANAGE_PRIO                99

/*======================================================  LOCAL DATA TYPES  ==*/

enum drvManState {
    DRV_MAN_Q_CTRL,
    DRV_MAN_FSM,
    DRV_MAN_CREATE,
    DRV_MAN_RUN
};

struct drvManCtx {
    rtdm_task_t         task;
    RT_QUEUE            ctrl;
    u32                 registeredFsm;
    enum drvManState    state;
};

/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/

static bool_T ctrlCmdIsValid(
    xUartCmd_T *        cmd,
    size_t              len);

static bool_T ctrlCmdUartIdIsValid(
    xUartCmd_T *        cmd);

static int drvManStart(
    void);

static void drvManStop (
    void);

static void taskDrvMan (
    void *              arg);

/*=======================================================  LOCAL VARIABLES  ==*/

static struct uartCtx * gUartChn[DEF_MAX_CHANNELS];
static struct drvManCtx gDrvManCtx;

/*======================================================  GLOBAL VARIABLES  ==*/

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DEF_DRV_AUTHOR);
MODULE_DESCRIPTION(DEF_DRV_DESCRIPTION);
MODULE_SUPPORTED_DEVICE(DEF_DRV_SUPP_DEVICE);

/*============================================  LOCAL FUNCTION DEFINITIONS  ==*/

static bool_T ctrlCmdIsValid(
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

static bool_T ctrlCmdUartIdIsValid(
    xUartCmd_T *        cmd) {

    bool_T              ans;

    ans = portIsOnline(cmd->uartId);

    return (ans);
}

static void uartCtxInit(
    struct uartCtx *    uartCtx,
    u32                 id) {

    rtdm_lock_init(&uartCtx->lock);
    scnprintf(
        uartCtx->name,
        UARTCTX_NAME_SIZE,
        CFG_DRV_NAME ".%d",
        id);
    uartCtx->hw.initialized = FALSE;
    uartCtx->id = id;
    uartCtx->signature = UARTCTX_SIGNATURE;
    fsmInit(
        uartCtx);
}

static int drvManStart(
    void) {

    int                 retval;
    u8                  i;

    gDrvManCtx.state = DRV_MAN_Q_CTRL;
    LOG_INFO("control queue %s, size %d", CTRL_QUEUE_NAME, CTRL_QUEUE_SIZE);
    retval = rt_queue_create(
        &gDrvManCtx.ctrl,
        CTRL_QUEUE_NAME,
        CTRL_QUEUE_SIZE,
        Q_UNLIMITED,
        Q_PRIO | Q_SHARED);

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("failed to create %s control queue", CTRL_QUEUE_NAME);

        return (retval);
    }

    gDrvManCtx.state = DRV_MAN_FSM;

    for (i = 0U; i < DEF_MAX_CHANNELS; i++) {
        gUartChn[i] = NULL;
    }

    for (i = 0U; i < DEF_MAX_CHANNELS; i++) {

        if (TRUE == portIsOnline(i)) {
            LOG_INFO("channel init %d", i);
            gUartChn[i] = kmalloc(
                sizeof(struct uartCtx),
                GFP_KERNEL);

            if (NULL == gUartChn[i]) {
                LOG_ERR("failed to create UART %d channel", i);

                return (-ENOMEM);
            }
            uartCtxInit(
                gUartChn[i],
                i);
        }
    }

    gDrvManCtx.state = DRV_MAN_CREATE;
    LOG_INFO("creating driver manager");
    retval = rtdm_task_init(
        &gDrvManCtx.task,
        TASK_MANAGE_NAME,
        taskDrvMan,
        NULL,
        TASK_MANAGE_PRIO,
        0);

    if (RETVAL_SUCCESS != retval) {
        LOG_ERR("failed to create %s driver manager", TASK_MANAGE_NAME);

        return (retval);
    }
    gDrvManCtx.state = DRV_MAN_RUN;

    return (retval);
}

static void drvManStop (
    void) {

    int                 retval;

    LOG_INFO("removing driver");

    switch (gDrvManCtx.state) {
        case DRV_MAN_RUN : {
            u32             i;
            volatile u32    cnt;
            struct xUartCmd cmd;

            memcpy(
                &cmd.sender,
                TASK_MANAGE_NAME,
                sizeof(TASK_MANAGE_NAME));
            cmd.cmdId = SIG_INTR_TERM;
            cmd.signature = XUART_CMD_SIGNATURE;

            for (i = 0; i < DEF_MAX_CHANNELS; i++) {

                if (TRUE == portIsOnline(i)) {
                    cmd.uartId = i;
                    rt_queue_write(
                        &gDrvManCtx.ctrl,
                        &cmd,
                        sizeof(struct xUartCmd),
                        Q_URGENT);
                }
            }
            cnt = -1;
            while ((0U != gDrvManCtx.registeredFsm) && (0U != cnt--));
            LOG_WARN_IF(0U == cnt, "failed to exit cleanly");

            rtdm_task_destroy(
                &gDrvManCtx.task);
            /* fall down */
        }
        case DRV_MAN_CREATE : {
            u32         i;

            for (i = 0U; i < DEF_MAX_CHANNELS; i++) {

                if (NULL != gUartChn[i]) {
                    fsmTerm(
                        gUartChn[i]);
                    kfree(
                        gUartChn[i]);
                }
            }
            /* fall down */
        }
        case DRV_MAN_FSM : {
            retval = rt_queue_delete(
                &gDrvManCtx.ctrl);
            LOG_ERR_IF(RETVAL_SUCCESS != retval, "failed to delete %s queue", CTRL_QUEUE_NAME);
            /* fall down */
        }
        case DRV_MAN_Q_CTRL : {
            break;
        }
        default : {
            LOG_ERR("internal error");
            break;
        }
    }
}

static void taskDrvMan (
    void *              arg) {

    u32                 i;

    gDrvManCtx.registeredFsm = 0;

    for (i = 0U; i < DEF_MAX_CHANNELS; i++) {

        if (NULL != gUartChn[i]) {
            fsmStart(
                gUartChn[i]);
        }
    }

    while (0U != gDrvManCtx.registeredFsm) {
        ssize_t         len;
        xUartCmd_T *    cmd;

        len = rt_queue_receive(
            &gDrvManCtx.ctrl,
            (void **)&cmd,
            TM_INFINITE);
        LOG_INFO("recv: cmd id: %d, UART: %d, from: %s", cmd->cmdId, cmd->uartId, cmd->sender);

        if (FALSE == ctrlCmdIsValid(cmd, len)) {
            LOG_ERR("sender: %s invalid control message", cmd->sender);

            continue;
        }
        if (FALSE == ctrlCmdUartIdIsValid(cmd)) {
            LOG_ERR("sender: %s invalid UART ID: %d", cmd->sender, cmd->uartId);

            continue;
        }
        fsmDispatch(
            gUartChn[cmd->uartId],
            cmd);
        rt_queue_free(
            &gDrvManCtx.ctrl,
            cmd);
    }
}

/*===================================  GLOBAL PRIVATE FUNCTION DEFINITIONS  ==*/
/*====================================  GLOBAL PUBLIC FUNCTION DEFINITIONS  ==*/

void drvManRegisterFSM(
    u32                 id) {

    gDrvManCtx.registeredFsm |= (0x01U << id);
}

void drvManUnregisterFSM(
    u32                 id) {

    gDrvManCtx.registeredFsm &= ~(0x01U << id);
}

int __init moduleInit(
    void) {

    int                 retval;

    LOG_INFO(DEF_DRV_DESCRIPTION);
    LOG_INFO("version: %d.%d.%d", DEF_DRV_VERSION_MAJOR, DEF_DRV_VERSION_MINOR, DEF_DRV_VERSION_PATCH);

    retval = drvManStart();

    return (retval);
}

void __exit moduleTerm(
    void) {

    drvManStop();
    LOG_INFO("driver removed");
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
