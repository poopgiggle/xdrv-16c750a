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
 * @brief       Interfejs Event (EVT) objekata
 * @details     This file is not meant to be included in application code
 *              independently but through the inclusion of "kernel.h" file.
 * @addtogroup  evt_intf
 *********************************************************************//** @{ */

#ifndef EVT_H_
#define EVT_H_

/*=========================================================  INCLUDE FILES  ==*/

#include "port/compiler.h"
#include "eds/dbg.h"
#include "../config/evt_config.h"

/*===============================================================  MACRO's  ==*/

/*------------------------------------------------------------------------*//**
 * @name        Dinamicki attributi dogadjaja
 * @{ *//*--------------------------------------------------------------------*/

/**
 * @brief       Bit maska za definisanje rezervisanog dogadjaja.
 * @details     Ukoliko je ovaj bit postavljen na jedinicu, dati dogadjaj je
 *              rezervisan i sistem ga nece razmatrati kao kandidata za brisanje.
 *              Brojac korisnika dogadjaja se i dalje azurira, tako da kada se
 *              dogadjaj oslobodii rezervacije on moze da bude obrisan ako nema
 *              korisnika.
 */
#define EVT_RESERVED_MASK               ((uint_fast8_t)(1U << 0))

/**
 * @brief       Bit maska za definisanje konstantnog dogadjaja.
 * @details     Ukoliko je ovaj bit postavljen na jedinicu, dati dogadjaj je
 *              konstantan (najčešće se nalazi u ROM memoriju) i ne moze biti
 *              izbrisan. Broj korisnika dogadjaja se ne azurira. Dati dogadjaj
 *              ne moze da postane dinamican.
 */
#define EVT_CONST_MASK                  ((uint_fast8_t)(1U << 1))

/** @} *//*-------------------------------------------------------------------*/

/**
 * @brief       Konstanta za potpis dogadjaja
 * @details     Konstanta se koristi prilikom debag procesa kako bi funkcije
 *              koje prime dogadjaj bile sigurne da je dogadjaj kreiran
 *              funkcijom esEvtCreate() i da je i dalje validan. Dogadjaji koji
 *              se obrisu nemaju ovaj potpis.
 * @pre         Opcija @ref OPT_KERNEL_DBG_EVT mora da bude aktivna kako bi bila
 *              omogucena provera pokazivaca.
 */
#define EVT_SIGNATURE                   ((portReg_T)0xDEADFEEDU)

/*------------------------------------------------------  C++ extern begin  --*/
#ifdef __cplusplus
extern "C" {
#endif

/*============================================================  DATA TYPES  ==*/

/**
 * @brief       Zaglavlje dogadjaja.
 * @details     Interni podaci dogadjaja su obavezni podaci u zaglavlju
 *              dogadjaja. Svi ostali podaci se mogu ukljuciti/iskljuciti po
 *              zelji.
 *
 *              Ova struktura nudi sledece opisivanje dogadjaja:
 *              - @c generator - ko je generisao dogadjaj,
 *              - @c timestamp - vremenska oznaka kada se dogadjaj desio,
 *              - @c size - velicina dogadjaja,
 *
 *              Svaka od navedenih clanova strukture se moze nezavisno
 *              ukljuciti/iskljuciti i tipovi clanova se mogu po zelji izabrati.
 *
 *              Dinamicki atributi pokazuju da li je dogadjaj konstantan (nalazi
 *              se u ROM memoriji), da li je rezervisan i koliko korisnika dati
 *              dogadjaj ima.
 * @note        Ukoliko se vrsi razmena dogadjaja izmedju sistema sa razlicitim
 *              procesorima/okruzenjem mora se pokloniti posebna paznja
 *              poravnanju (align) podataka ove strukture. Radi podesavanja
 *              nacina pokovanja strukture koristi se @ref OPT_EVT_STRUCT_ATTRIB.
 * @api
 */
typedef struct OPT_EVT_STRUCT_ATTRIB esEvt {

/**
 * @brief       Identifikator dogadjaja
 * @details     Podesavanje tipa se vrsi pomocu: @ref OPT_EVT_ID_T.
 */
    esEvtId_T       id;

/**
 * @brief       Unija dinamickih atributa dogadjaja
 * @details     Koristi se samo za brz pristup clanovima
 */
    union udynamic {
        uint16_t    u;                                                          /**< @brief     Unija atributa                              */

        /**
         * @brief       Struktura atributa
         */
        struct sdynamic {
            uint8_t counter;                                                    /**< @brief     Brojac korisnika dogadjaja                  */
            uint8_t attrib;                                                     /**< @brief     Kvalifikatori dogadjaja.
                                                                                     @see       EVT_RESERVED_MASK
                                                                                     @see       EVT_CONST_MASK                              */
        }           s;                                                          /**< @brief     Struktura atributa                          */
    }               dynamic;                                                    /**< @brief     Dinamicki atributi dogadjaja                */
#if (1U == CFG_DBG_API_VALIDATION)
    portReg_T       signature;
#endif
} esEvt_T;

/*======================================================  GLOBAL VARIABLES  ==*/
/*===================================================  FUNCTION PROTOTYPES  ==*/
/** @} *//*-----------------------------------------------  C++ extern end  --*/
#ifdef __cplusplus
}
#endif

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of evt.h
 ******************************************************************************/
#endif /* EVT_H_ */
