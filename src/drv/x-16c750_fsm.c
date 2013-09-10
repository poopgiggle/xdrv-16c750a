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
#include "eds/smp.h"

/*=========================================================  LOCAL MACRO's  ==*/

#define TO_UART_CTX(fsm)                                                        \
    container_of(fsm, struct uartCtx, fsm)

/*======================================================  LOCAL DATA TYPES  ==*/

struct evtCmd {
    esEvt_T             header;
    xUartCmd_T *        cmd;
};

/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/

static esStatus_T stateInit(
    esFsm_T *           fsm,
    esEvt_T *           evt);

static esStatus_T stateTerm(
    esFsm_T *           fsm,
    esEvt_T *           evt);

static esStatus_T stateIdle(
    esFsm_T *           fsm,
    esEvt_T *           evt);

/*=======================================================  LOCAL VARIABLES  ==*/
/*======================================================  GLOBAL VARIABLES  ==*/
/*============================================  LOCAL FUNCTION DEFINITIONS  ==*/

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
        case SIG_INTR_TERM : {

            return ES_STATE_TRAN(fsm, stateTerm);
        }

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
