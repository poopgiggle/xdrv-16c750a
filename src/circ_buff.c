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
 * @brief       Circular buffer implementation
 *********************************************************************//** @{ */

/*=========================================================  INCLUDE FILES  ==*/

#include <asm/system.h>

#include "circ_buff.h"
#include "dbg/dbg.h"

#if (1U == CFG_DBG_ENABLE)
# include "log.h"
#endif

/*=========================================================  LOCAL MACRO's  ==*/
/*======================================================  LOCAL DATA TYPES  ==*/
/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/
/*=======================================================  LOCAL VARIABLES  ==*/
/*======================================================  GLOBAL VARIABLES  ==*/
/*============================================  LOCAL FUNCTION DEFINITIONS  ==*/
/*===================================  GLOBAL PRIVATE FUNCTION DEFINITIONS  ==*/
/*====================================  GLOBAL PUBLIC FUNCTION DEFINITIONS  ==*/

#if (1U == CFG_DBG_ENABLE)
# define DBG_VALIDATE(buff, var)                                                \
    do {                                                                        \
        if (var > buff->size) {                                                 \
            uint64_t i;\
            LOG_ERR("INVALID QUEUE POINTER: %s in %s", #var, PORT_C_FUNC);      \
            for (i = -1; i != 0; i--);\
        }                                                                       \
    } while (0)
#else
# define DBG_VALIDATE(buff, var)
#endif

void circInit(
    CIRC_BUFF *         buff,
    void *              mem,
    size_t              size) {

    buff->mem  = (u8 *)mem;
    buff->head = 0U;
    buff->tail = 0U;
    buff->size = (u32)size;
    buff->free = buff->size;
}

size_t circRemainingFreeGet(
    const CIRC_BUFF *   buff) {

    u32                 tmp;

    if (buff->tail > buff->head) {
        tmp = buff->tail - buff->head;
    } else if (buff->tail < buff->head) {
        tmp = buff->size - buff->head;
    } else if (0U != buff->free) {
        tmp = buff->size - buff->head;
    } else {
        tmp = buff->free;
    }
    DBG_VALIDATE(buff, tmp);

    return ((size_t)tmp);
}

size_t circRemainingOccGet(
    const CIRC_BUFF *   buff) {

    u32                 tmp;

    if (buff->tail < buff->head) {
        tmp = buff->head - buff->tail;
    } else if (buff->tail > buff->head) {
        tmp = buff->size - buff->tail;
    } else if (0U == buff->free) {
        tmp = buff->size - buff->tail;
    } else {
        tmp = 0U;
    }
    DBG_VALIDATE(buff, tmp);

    return ((size_t)tmp);
}

u8 * circMemBaseGet(
    const CIRC_BUFF *   buff) {

    return ((u8 *)buff->mem);
}

u8 * circMemHeadGet(
    const CIRC_BUFF *   buff) {

    DBG_VALIDATE(buff, buff->head);

    return ((u8 *)&buff->mem[buff->head]);
}

u8 * circMemTailGet(
    const CIRC_BUFF *   buff) {

    DBG_VALIDATE(buff, buff->tail);

    return ((u8 *)&buff->mem[buff->tail]);
}

void circPosHeadSet(
    CIRC_BUFF *         buff,
    s32                 position) {

    buff->free -= position;
    buff->head += position;

    if (buff->size == buff->head) {
        buff->head = 0U;
    }
    DBG_VALIDATE(buff, buff->head);
    DBG_VALIDATE(buff, buff->free);
}

void circPosTailSet(
    CIRC_BUFF *         buff,
    s32                 position) {

    buff->free += position;
    buff->tail += position;

    if (buff->size == buff->tail) {
        buff->tail = 0U;
    }
    DBG_VALIDATE(buff, buff->head);
    DBG_VALIDATE(buff, buff->free);
}

bool_T circIsEmpty(
    const CIRC_BUFF *   buff) {

    bool_T             ans;

    DBG_VALIDATE(buff, buff->free);

    if (buff->size == buff->free) {
        ans = TRUE;
    } else {
        ans = FALSE;
    }

    return (ans);
}

bool_T circIsFull(
    const CIRC_BUFF *   buff) {

    bool_T             ans;

    DBG_VALIDATE(buff, buff->free);

    if (0U == buff->free) {
        ans = TRUE;
    } else {
        ans = FALSE;
    }

    return (ans);
}

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of circ_buff.c
 ******************************************************************************/
