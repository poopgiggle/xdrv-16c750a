/*
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
 * @brief   	Interfejs State Machine Processor (SMP) objekata
 * @addtogroup  smp_intf
 *********************************************************************//** @{ */

#ifndef SMP_H_
#define SMP_H_

/*=========================================================  INCLUDE FILES  ==*/

#include "port/compiler.h"
#include "eds/evt.h"
#include "eds/dbg.h"
#include "../config/smp_config.h"

/*===============================================================  DEFINES  ==*/
/*===============================================================  MACRO's  ==*/

#define ES_STATE_TRAN(fsm, nextState)                                            \
    (((esFsm_T *)fsm)->state = nextState, RETN_TRAN)

#define ES_RETN_DEFFERED(fsm)                                                   \
    (RETN_DEFFERED)

#define ES_STATE_HANDLED(fsm)                                                    \
    (RETN_HANDLED)

#define ES_RETN_IGNORED(fsm)                                                    \
    (RETN_IGNORED)

#define ES_STATE_SUPER(fsm, topState)                                            \
    (((esFsm_T *)fsm)->state = topState, RETN_SUPER)

#define ES_SIGNAL(signal)                                                       \
    &esEvtSignal[signal]

/*------------------------------------------------------  C++ extern begin  --*/
#ifdef __cplusplus
extern "C" {
#endif

/*============================================================  DATA TYPES  ==*/

/**
 * @brief       Nabrajanje odgovora state handler funkcije.
 * @details     State handler funkcija preko ovih nabrajanja govori SMP
 *              dispeceru da li treba da se preuzme neka akcija kao odgovor na
 *              dogadjaj.
 */
enum smStatus {
/**
 * @brief       Treba izvrsiti tranziciju ka drugom stanju.
 * @details     Akcija koja je potrebna za odgovor na dogadjaj je tranzicija ka
 *              drugom stanju.
 */
    RETN_TRAN,

/**
 * @brief       Treba odloziti pristigli dogadjaj.
 * @details     Sistem ce predati dogadjaj vratiti ponovo u red za cekanje za
 *              dogadjaje i poslati ga prilikom sledeceg ciklusa.
 */
    RETN_DEFERRED,

/**
 * @brief       Ne treba izvrsiti nikakve dalje akcije.
 * @details     Ovo je odgovor state handler funkcije da je potpuno opsluzila
 *              dogadjaj i nikakve dodatne akcije ne treba da se preduzmu.
 */
    RETN_HANDLED,

/**
 * @brief       Pristigli dogadjaj nije obradjen i ignorisan je.
 * @details     Obicno se ovakav odgovor u top state-u automata i koristi se u
 *              svrhe debagiranja sistema. Dogadjaj se brise iz sistema ako nema
 *              jos korisnika.
 */
    RETN_IGNORED,

/**
 * @brief       Vraca se koje je super stanje date state handler funkcije.
 * @details     Ova vrednost se vraca kada state handler funkcija ne zna da
 *              obradi neki dogadjaj ili je od nje zahtevano da vrati koje je
 *              njeno super stanje.
 */
    RETN_SUPER
};

/**
 * @brief       Identifikatori predefinisanih dogadjaja
 * @details     Zadnji predefinisan identifikator je @ref SIG_ID_USR. Nakon ovog
 *              identifikatora korisnik definise svoje, aplikacione
 *              identifikatore dogadjaja.
 */
enum esEvtSignalId {
/**
 * @brief       Signalni dogadjaj - prazan signal.
 * @note        Ne koristi se.
 */
    SIG_EMPTY = 1000U,

/**
 * @brief       Signalni dogadjaj - zahteva se entry obrada u datom stanju.
 */
    SIG_ENTRY,

/**
 * @brief       Signalni dogadjaj - zahteva se exit obrada u datom stanju.
 */
    SIG_EXIT,

/**
 * @brief       Signalni dogadjaj - zahteva se inicijalizaciona (init) putanja.
 */
    SIG_INIT,

/**
 * @brief       Signalni dogadjaj - zahteva se super stanje.
 * @details     Od funkcije stanja (u aplikaciji) se zahteva koje je njeno
 *              super stanje. Funkcija stanja mora da vrati pokazivac na njeno
 *              super stanje.
 */
    SIG_SUPER,

/**
 * @brief       Domen korisnickih identifikatora dogadjaja.
 */
    SIG_ID_USR = 15
};

/**
 * @brief       Status koji state handler funkcije vracaju dispeceru.
 * @details     Ovo je apstraktni tip koji se koristi za podatke koji vracaju
 *              odgovor state handler funkcija dispeceru automata. Preko ovog
 *              podatka state handler funkcije obavestavaju dispecer koje akcije
 *              automat zeli da preduzme, kao na primer, tranzicija ka drugom
 *              stanju.
 * @api
 */
typedef uint_fast8_t esStatus_T;

/**
 * @brief       Objekat konacnog automata
 * @details     Ovo je apstraktni tip koji se koristi za referenciranje SMP
 *              objekata.
 * @api
 */
typedef struct esFsm esFsm_T;

/**
 * @brief       Tip state handler funkcija.
 * @details     State handler funkcije vracaju esStatus_T , a kao parametar
 *              prihvataju pokazivac (void *) na strukturu podataka i pokazivac
 *              na dogadjaj.
 * @api
 */
typedef esStatus_T (* esState_T) (esFsm_T *, esEvt_T *);

struct esFsm {
    esState_T       state;

#if (1U == CFG_DBG_API_VALIDATION)
    portReg_T       signature;
#endif
};

/*======================================================  GLOBAL VARIABLES  ==*/

/**
 * @brief       Signalni događaji koji se koriste prilikom izvršavanja automata
 * @details     Identifikatori događaja su navedeni u @ref esEvtSignalId
 */
extern const PORT_C_ROM esEvt_T esEvtSignal[];

/*===================================================  FUNCTION PROTOTYPES  ==*/

/*------------------------------------------------------------------------*//**
 * @name Funkcije za rad sa konacnim automatom (State Machine)
 * @{ *//*--------------------------------------------------------------------*/

/**
 * @brief       Dispecer HSM automata
 * @param       [in] sm                 Pokazivac na strukturu FSM automata
 * @param       [in] evt                Dogadjaj koji treba da se obradi
 * @return      Status obrade dogadjaja.
 */
esStatus_T esFsmDispatch(
    esFsm_T *           fsm,
    const esEvt_T *     evt);

/**
 * @brief       Konstruise automat
 * @param       [out] sm                Pokazivac na tek kreiranu strukturu
 *                                      automata,
 * @param       [in] initState          inicijalno stanje automata,
 *                                      automata.
 */
void esFsmInit (
    esFsm_T *           fsm,
    esState_T           initState);

/**
 * @brief       Dekonstruise automat
 * @param       [out] sm                Pokazivac na kreiran automat.
 */
void esFsmTerm(
    esFsm_T *           fsm);

esStatus_T esFsmTopState(
    esFsm_T *           fsm,
    esEvt_T *           evt);

/** @} *//*-----------------------------------------------  C++ extern end  --*/
#ifdef __cplusplus
}
#endif

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of smp.h
 ******************************************************************************/
#endif /* SMP_H_ */
