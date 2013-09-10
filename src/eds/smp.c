/******************************************************************************
 * This file is part of eSolid
 *
 * Copyright (C) 2011, 2012 - Nenad Radulovic
 *
 * eSolid is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * eSolid is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with eSolid; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * web site:    http://blueskynet.dyndns-server.com
 * e-mail  :    blueskyniss@gmail.com
 *//***********************************************************************//**
 * @file
 * @author      Nenad Radulovic
 * @brief       Implementacija State Machine Processor objekta.
 * @addtogroup  smp_impl
 *********************************************************************//** @{ */

/*=========================================================  INCLUDE FILES  ==*/

#define SMP_PKG_H_VAR
#include "eds/smp.h"
#include "eds/evt.h"

/*=========================================================  LOCAL DEFINES  ==*/

/**
 * @brief       Konstanta za potpis SM objekta
 * @details     Konstanta se koristi prilikom debag procesa kako bi funkcije
 *              koje prihvate pokazivac na SM objekat bile sigurne da je SM
 *              objekat validan. SM objekti koji su obrisani nemaju ovaj potpis.
 * @pre         Opcija @ref OPT_KERNEL_DBG mora da bude aktivna kako bi bila
 *              omogucena provera pokazivaca.
 */
#define SM_SIGNATURE                   ((portReg_T)0xDEADDAAFU)

/*=========================================================  LOCAL MACRO's  ==*/

/**
 * @brief       Posalji predefinisan dogadjaj @c evt automatu @c hsm.
 * @param       sm                      Pokazivac na strukturu EPA objekta,
 * @param       state                   pokazivac na funkciju stanja,
 * @param       evt                     redni broj (enumerator) rezervisanog
 *                                      dogadjaj.
 */
#define SM_SIGNAL_SEND(sm, state, evt)                                         \
    (*state)((sm), (esEvt_T *)&esEvtSignal[evt])

/**
 * @brief       Posalji dogadjaj @c evt automatu @c hsm.
 * @param       sm                      Pokazivac na strukturu EPA objekta,
 * @param       state                   pokazivac na funkciju stanja,
 * @param       evt                     redni broj (enumerator) rezervisanog
 *                                      dogadjaj.
 */
#define SM_EVT_SEND(sm, state, evt)                                             \
    (*state)((sm), (evt))

/*======================================================  LOCAL DATA TYPES  ==*/
/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/
/*=======================================================  LOCAL VARIABLES  ==*/

DECL_MODULE_INFO("SMP", "State machine processor", "Nenad Radulovic");

/*======================================================  GLOBAL VARIABLES  ==*/

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
/**
 * @brief       Tabela signalnih dogadjaja
 */
const PORT_C_ROM esEvt_T esEvtSignal[] = {
    [SIG_EMPTY].id = SIG_EMPTY,
    [SIG_EMPTY].dynamic = {
        .s = {
            .counter = 0U,
            .attrib = EVT_CONST_MASK
        }
    },
#if (1U == CFG_DBG_API_VALIDATION)
    [SIG_EMPTY].signature = EVT_SIGNATURE,
#endif

    [SIG_ENTRY].id = SIG_ENTRY,
    [SIG_ENTRY].dynamic = {
        .s = {
            .counter = 0U,
            .attrib = EVT_CONST_MASK
        }
    },
#if (1U == CFG_DBG_API_VALIDATION)
    [SIG_ENTRY].signature = EVT_SIGNATURE,
#endif

    [SIG_EXIT].id = SIG_EXIT,
    [SIG_EXIT].dynamic = {
        .s = {
            .counter = 0U,
            .attrib = EVT_CONST_MASK
        }
    },
#if (1U == CFG_DBG_API_VALIDATION)
    [SIG_EXIT].signature = EVT_SIGNATURE,
#endif

    [SIG_INIT].id = SIG_INIT,
    [SIG_INIT].dynamic = {
        .s = {
            .counter = 0U,
            .attrib = EVT_CONST_MASK
        }
    },
#if (1U == CFG_DBG_API_VALIDATION)
    [SIG_INIT].signature = EVT_SIGNATURE,
#endif

    [SIG_SUPER].id = SIG_SUPER,
    [SIG_SUPER].dynamic = {
        .s = {
            .counter = 0U,
            .attrib = EVT_CONST_MASK
        }
    },
#if (1U == CFG_DBG_API_VALIDATION)
    [SIG_SUPER].signature = EVT_SIGNATURE,
#endif
};
#pragma GCC diagnostic pop

/*============================================  LOCAL FUNCTION DEFINITIONS  ==*/
/*===================================  GLOBAL PRIVATE FUNCTION DEFINITIONS  ==*/
/*====================================  GLOBAL PUBLIC FUNCTION DEFINITIONS  ==*/

/*----------------------------------------------------------------------------*/
void esFsmInit (
    esFsm_T *           sm,
    esState_T           initState) {

    ES_DBG_API_REQUIRE(ES_DBG_POINTER_NULL, NULL != sm);
    ES_DBG_API_REQUIRE(ES_DBG_OBJECT_NOT_VALID, SM_SIGNATURE != sm->signature);
    sm->state = initState;
    ES_DBG_API_OBLIGATION(sm->signature = SM_SIGNATURE);
}

/*----------------------------------------------------------------------------*/
void esFsmTerm(
    esFsm_T *           sm) {

    ES_DBG_API_REQUIRE(ES_DBG_POINTER_NULL, NULL != sm);
    ES_DBG_API_REQUIRE(ES_DBG_OBJECT_NOT_VALID, SM_SIGNATURE == sm->signature);
    sm->state = (esState_T)0U;
    ES_DBG_API_OBLIGATION(sm->signature = ~SM_SIGNATURE);
}

/*----------------------------------------------------------------------------*/
esStatus_T esFsmDispatch(
    esFsm_T *           fsm,
    const esEvt_T *     evt) {

    esStatus_T status;
    esState_T oldState;
    esState_T newState;

    ES_DBG_API_REQUIRE(ES_DBG_POINTER_NULL, NULL != fsm);
    ES_DBG_API_REQUIRE(ES_DBG_OBJECT_NOT_VALID, SM_SIGNATURE == fsm->signature);

    oldState = fsm->state;
    status = SM_EVT_SEND(fsm, fsm->state, (esEvt_T *)evt);
    newState = fsm->state;

    while (RETN_TRAN == status) {
        (void)SM_SIGNAL_SEND(fsm, oldState, SIG_EXIT);
        (void)SM_SIGNAL_SEND(fsm, newState, SIG_ENTRY);
        status = SM_SIGNAL_SEND(fsm, newState, SIG_INIT);
        oldState = newState;
        newState = fsm->state;
    }
    fsm->state = oldState;

    return (status);
}

/*----------------------------------------------------------------------------*/
esStatus_T esFsmTopState(
    esFsm_T *           fsm,
    esEvt_T *           evt) {

    (void)fsm;
    (void)evt;

    return ES_STATE_HANDLED(fsm);
}

/** @} *//*-------------------------------------------------------------------*/
/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/

/** @endcond *//** @} *//******************************************************
 * END of smp.c
 ******************************************************************************/
