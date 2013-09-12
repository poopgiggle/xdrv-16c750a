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
 * @author  	Nenad Radulovic
 * @brief       Circular buffer interface
 *********************************************************************//** @{ */

#if !defined(CIRC_BUFF_H_)
#define CIRC_BUFF_H_

/*=========================================================  INCLUDE FILES  ==*/

#include "arch/compiler.h"

/*===============================================================  MACRO's  ==*/
/*------------------------------------------------------  C++ extern begin  --*/
#ifdef __cplusplus
extern "C" {
#endif

/*============================================================  DATA TYPES  ==*/

/*------------------------------------------------------------------------*//**
 * @name        Data types group
 * @brief       brief description
 * @{ *//*--------------------------------------------------------------------*/

struct circBuff {
    volatile u8 *       mem;
    volatile u32        head;
    volatile u32        tail;
    u32                 size;
    u32                 free;
};

typedef struct circBuff CIRC_BUFF;

/** @} *//*-------------------------------------------------------------------*/
/*======================================================  GLOBAL VARIABLES  ==*/
/*===================================================  FUNCTION PROTOTYPES  ==*/

/*------------------------------------------------------------------------*//**
 * @name        Function group
 * @brief       brief description
 * @{ *//*--------------------------------------------------------------------*/

void circInit(
    CIRC_BUFF *         buff,
    void *              mem,
    size_t              size);

static inline void circItemPut(
    CIRC_BUFF *         buff,
    u8                  item) {

    buff->mem[buff->head] = item;
    smp_wmb();
    buff->free--;
    buff->head++;

    if (buff->head == buff->size) {
        buff->head = 0;
    }
}

static inline u8 circItemGet(
    CIRC_BUFF *         buff) {

    u8                  tmp;

    smp_read_barrier_depends();
    tmp = buff->mem[buff->tail];
    smp_mb();
    buff->free++;
    buff->tail++;

    if (buff->tail == buff->size) {
        buff->tail = 0;
    }

    return (tmp);
}

size_t circRemainingFreeGet(
    const CIRC_BUFF *   buff);

size_t circRemainingOccGet(
    const CIRC_BUFF *   buff);

u8 * circMemBaseGet(
    const CIRC_BUFF *   buff);

u8 * circMemHeadGet(
    const CIRC_BUFF *   buff);

u8 * circMemTailGet(
    const CIRC_BUFF *   buff);

void circPosHeadSet(
    CIRC_BUFF *         buff,
    s32                 position);

void circPosTailSet(
    CIRC_BUFF *         buff,
    s32                 position);

bool_T circIsEmpty(
    const CIRC_BUFF *   buff);

bool_T circIsFull(
    const CIRC_BUFF *   buff);

/** @} *//*-------------------------------------------------------------------*/
/*--------------------------------------------------------  C++ extern end  --*/
#ifdef __cplusplus
}
#endif

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of circ_buff.h
 ******************************************************************************/
#endif /* CIRC_BUFF_H_ */
