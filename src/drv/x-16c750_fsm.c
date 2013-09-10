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
 * @brief       Driver FSM
 *********************************************************************//** @{ */

/*=========================================================  INCLUDE FILES  ==*/

#include "drv/x-16c750.h"
#include "drv/x-16c750_fsm.h"
#include "drv/x-16c750_lld.h"
#include "port/compiler.h"
#include "port/port.h"
#include "eds/smp.h"

/*=========================================================  LOCAL MACRO's  ==*/

#define TO_UART_CTX(fsm)                                                        \
    container_of(fsm, struct uartCtx, fsm)

/*======================================================  LOCAL DATA TYPES  ==*/

struct evtCmd {
    esEvt_T             header;
    xUartCmd_T *        cmd;
};

enum cIntNum {
    C_INT_TX            = IER_THRIT,
    C_INT_RX            = IER_RHRIT,
    C_INT_RX_TIMEOUT    = IER_RHRIT
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

static esStatus_T stateInit(
    esFsm_T *           fsm,
    esEvt_T *           evt);

static esStatus_T stateTerm(
    esFsm_T *           fsm,
    esEvt_T *           evt);

static esStatus_T stateIdle(
    esFsm_T *           fsm,
    esEvt_T *           evt);

static esStatus_T stateOpen(
    esFsm_T *           fsm,
    esEvt_T *           evt);

/*=======================================================  LOCAL VARIABLES  ==*/
/*======================================================  GLOBAL VARIABLES  ==*/
/*============================================  LOCAL FUNCTION DEFINITIONS  ==*/

static int handleIrq(
    rtdm_irq_t *        arg) {

    struct uartCtx *    uartCtx;
    volatile u8 *       io;
    int                 retval;
    u32                 evtRx;
    u32                 evtTx;

    uartCtx = rtdm_irq_get_arg(arg, struct uartCtx);
    io = uartCtx->cache.io;
    evtRx = 0U;
    evtTx = 0U;
    retval = RTDM_IRQ_NONE;
    rtdm_lock_get(&uartCtx->lock);
    uartCtx->rx.status = UART_STATUS_NORMAL;

    while (0 == lldIsIntPending(io)) {                                          /* Loop until there are interrupts to process               */
        enum lldIntNum      intNum;

        intNum = lldIntGet(                                                     /* Get new interrupt                                        */
            io);

        /*-- Receive interrupt -----------------------------------------------*/
        if ((LLD_INT_RX_TIMEOUT == intNum) || (LLD_INT_RX == intNum)) {
            retval = RTDM_IRQ_HANDLED;
            evtRx = 1U;

            if (FALSE == circIsFull(&uartCtx->rx.buffHandle)) {
                u16 item;

                item = lldRegRd(
                    io,
                    RHR);
                circItemPut(
                    &uartCtx->rx.buffHandle,
                    item);
            } else {
                cIntDisable(
                    uartCtx,
                    C_INT_RX | C_INT_RX_TIMEOUT);
                uartCtx->rx.status = UART_STATUS_SOFT_OVERFLOW;
                lldRegRd(
                    io,
                    RHR);
            }

        /*-- Transmit interrupt ----------------------------------------------*/
        } else if (LLD_INT_TX == intNum) {
            retval = RTDM_IRQ_HANDLED;

            while (0 == (lldRegRd(io,SSR) & SSR_TXFIFOFULL)) {

                if (FALSE == circIsEmpty(&uartCtx->tx.buffHandle)) {
                    u16 item;

                    item = circItemGet(
                        &uartCtx->tx.buffHandle);
                    lldRegWr(
                        io,
                        wTHR,
                        item);
                } else {
                    cIntDisable(
                        uartCtx,
                        C_INT_TX);
                    evtTx = 1U;
                    break;
                }
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
            lldIntDisable(
                uartCtx->cache.io,
                LLD_INT_ALL);
            break;
        }
    }

    if (0U != evtRx) {                                                          /* Notify read listeners                                    */
        rtdm_event_signal(
            &uartCtx->rx.opr);
    }

    if (0U != evtTx) {                                                          /* Notify write listeners                                   */
        rtdm_event_signal(
            &uartCtx->tx.opr);
    }
    rtdm_lock_put(&uartCtx->lock);

    return (retval);
}

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

/*-- FSM ---------------------------------------------------------------------*/
static esStatus_T stateInit(
    esFsm_T *           fsm,
    esEvt_T *           evt) {

    struct uartCtx *    uartCtx;

    uartCtx = TO_UART_CTX(fsm);

    switch (evt->id) {
        case SIG_INIT : {
            drvManRegisterFSM(
                uartCtx->id);

            return ES_STATE_TRAN(fsm, stateIdle);
        }
        default : {

            return ES_STATE_SUPER(fsm, esFsmTopState);
        }
    }
}

static esStatus_T stateTerm(
    esFsm_T *           fsm,
    esEvt_T *           evt) {

    struct uartCtx *    uartCtx;

    uartCtx = TO_UART_CTX(fsm);

    switch (evt->id) {
        case SIG_ENTRY : {
            /*
             * Something like close function
             */
            uartCtx->hw.initialized = FALSE;
            drvManUnregisterFSM(
                uartCtx->id);

            return ES_STATE_HANDLED(fsm);
        }
        default : {

            return ES_STATE_SUPER(fsm, esFsmTopState);
        }
    }
}

static esStatus_T stateIdle(
    esFsm_T *           fsm,
    esEvt_T *           evt) {

    struct uartCtx *    uartCtx;

    uartCtx = TO_UART_CTX(fsm);

    switch (evt->id) {
        case XUART_CMD_CHN_OPEN : {

            if (FALSE == uartCtx->hw.initialized) {
                rtdm_lockctx_t      lockCtx;
                volatile u8 *       io;
                int                 retval;

                uartCtx->hw.resource = portInit(
                    uartCtx->id);

                if (NULL == uartCtx->hw.resource) {
                    LOG_ERR("failed to initialize port driver");

                    return ES_STATE_TRAN(fsm, stateTerm);
                }
                io = portIORemapGet(
                    uartCtx->hw.resource);
                lldSoftReset(
                    io);
                lldFIFOInit(
                    io);
                lldEnhanced(
                    io,
                    LLD_ENABLE);
                uartCtx->cache.io = io;
                uartCtx->cache.IER = lldRegRd(
                    uartCtx->cache.io,
                    IER);
                uartCtx->hw.initialized = TRUE;
                rtdm_lock_get_irqsave(&uartCtx->lock, lockCtx);
                retval = rtdm_irq_request(
                    &uartCtx->irqHandle,
                    gPortIRQ[uartCtx->id],
                    handleIrq,
                    RTDM_IRQTYPE_EDGE,
                    uartCtx->name,
                    uartCtx);
                cIntEnable(
                    uartCtx,
                    C_INT_RX | C_INT_RX_TIMEOUT);
                rtdm_lock_put_irqrestore(&uartCtx->lock, lockCtx);

                if (RETVAL_SUCCESS != retval) {
                    LOG_ERR("failed to register interrupt handler (err: %d)", retval);

                    return ES_STATE_TRAN(fsm, stateTerm);
                }
            }

            return ES_STATE_TRAN(fsm, stateOpen);
        }
        case SIG_INTR_TERM : {

            return ES_STATE_TRAN(fsm, stateTerm);
        }
        default : {

            return ES_STATE_SUPER(fsm, esFsmTopState);
        }
    }
}

static esStatus_T stateOpen(
    esFsm_T *           fsm,
    esEvt_T *           evt) {

    struct uartCtx *    uartCtx;

    uartCtx = TO_UART_CTX(fsm);

    switch (evt->id) {
        default : {

            return ES_STATE_SUPER(fsm, esFsmTopState);
        }
    }
}

/*===================================  GLOBAL PRIVATE FUNCTION DEFINITIONS  ==*/
/*====================================  GLOBAL PUBLIC FUNCTION DEFINITIONS  ==*/

void fsmInit(
    struct uartCtx *    uartCtx) {

    esFsmInit(
        &uartCtx->fsm,
        stateInit);
}

void fsmStart(
    struct uartCtx *    uartCtx) {

    esFsmDispatch(
        &uartCtx->fsm,
        ES_SIGNAL(SIG_INIT));
}

void fsmDispatch(
    struct uartCtx *    uartCtx,
    struct xUartCmd *   cmd) {

    struct evtCmd       evtCmd;

    evtCmd.header.id = cmd->cmdId;
    evtCmd.header.dynamic.u = 0U;
    ES_DBG_API_OBLIGATION(evtCmd.header.signature = EVT_SIGNATURE;);
    evtCmd.cmd = cmd;
    esFsmDispatch(
        &uartCtx->fsm,
        (esEvt_T *)&evtCmd);
}

void fsmTerm(
    struct uartCtx *    uartCtx) {

    esFsmTerm(
        &uartCtx->fsm);
}

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of x-16c750_fsm.c
 ******************************************************************************/
