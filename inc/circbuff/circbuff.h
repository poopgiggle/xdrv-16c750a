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

#if !defined(CIRCBUFF_H_)
#define CIRCBUFF_H_

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
    volatile uint8_t *  mem;
    uint32_t   			head;
    uint32_t   			tail;
    uint32_t            size;
    uint32_t            free;
};

typedef struct circBuff circBuff_T;

/** @} *//*-------------------------------------------------------------------*/
/*======================================================  GLOBAL VARIABLES  ==*/
/*===================================================  FUNCTION PROTOTYPES  ==*/

/*------------------------------------------------------------------------*//**
 * @name        Function group
 * @brief       brief description
 * @{ *//*--------------------------------------------------------------------*/

void circInit(
    circBuff_T *        buff,
    void *              mem,
    size_t              size);

static inline void circItemPut(
    circBuff_T *        buff,
    uint8_t             item) {

    buff->mem[buff->head] = item;
    smp_wmb();
    buff->free--;
    buff->head++;

    if (buff->head == buff->size) {
        buff->head = 0;
    }
}

static inline u8 circItemGet(
    circBuff_T *        buff) {

    uint8_t             tmp;

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
    const circBuff_T *  buff);

size_t circRemainingOccGet(
    const circBuff_T *  buff);

size_t circFreeGet(
    const circBuff_T *  buff);

size_t circOccGet(
    const circBuff_T *  buff);

size_t circSizeGet(
    const circBuff_T *  buff);

uint8_t * circMemBaseGet(
    const circBuff_T *  buff);

uint8_t * circMemHeadGet(
    const circBuff_T *  buff);

uint8_t * circMemTailGet(
    const circBuff_T *  buff);

void circPosHeadSet(
    circBuff_T *        buff,
    int32_t             position);

void circPosTailSet(
    circBuff_T *        buff,
    int32_t             position);

bool_T circIsEmpty(
    const circBuff_T *  buff);

bool_T circIsFull(
    const circBuff_T *  buff);

void circFlush(
    circBuff_T *        buff);

/** @} *//*-----------------------------------------------  C++ extern end  --*/
#ifdef __cplusplus
}
#endif

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of circbuff.h
 ******************************************************************************/
#endif /* CIRCBUFF_H_ */
