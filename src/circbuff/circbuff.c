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

#include "circbuff/circbuff.h"

/*=========================================================  LOCAL MACRO's  ==*/
/*======================================================  LOCAL DATA TYPES  ==*/
/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/
/*=======================================================  LOCAL VARIABLES  ==*/
/*======================================================  GLOBAL VARIABLES  ==*/
/*============================================  LOCAL FUNCTION DEFINITIONS  ==*/
/*===================================  GLOBAL PRIVATE FUNCTION DEFINITIONS  ==*/
/*====================================  GLOBAL PUBLIC FUNCTION DEFINITIONS  ==*/

void circInit(
    circBuff_T *         buff,
    void *              mem,
    size_t              size) {

    buff->mem  = (uint8_t *)mem;
    buff->head = 0U;
    buff->tail = 0U;
    buff->size = (uint32_t)size;
    buff->free = buff->size;
}

void circItemPut(
    circBuff_T *         buff,
    uint8_t                  item) {

    buff->mem[buff->head] = item;
    smp_wmb();
    buff->free--;
    buff->head++;

    if (buff->head == buff->size) {
        buff->head = 0;
    }
}

uint8_t circItemGet(
    circBuff_T *         buff) {

    uint8_t                  tmp;

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
    const circBuff_T *   buff) {

    uint32_t                 tmp;

    if (buff->tail > buff->head) {
        tmp = buff->tail - buff->head;
    } else if (buff->tail < buff->head) {
        tmp = buff->size - buff->head;
    } else {
        tmp = buff->free;
    }

    return ((size_t)tmp);
}

size_t circRemainingOccGet(
    const circBuff_T *   buff) {

    uint32_t                 tmp;

    if (buff->tail < buff->head) {
        tmp = buff->head - buff->tail;
    } else if (buff->tail > buff->head) {
        tmp = buff->size - buff->tail;
    } else {
        tmp = buff->size - buff->free;
    }

    return ((size_t)tmp);
}

uint8_t * circMemBaseGet(
    const circBuff_T *   buff) {

    return ((uint8_t *)buff->mem);
}

uint8_t * circMemHeadGet(
    const circBuff_T *   buff) {

    return ((uint8_t *)&buff->mem[buff->head]);
}

uint8_t * circMemTailGet(
    const circBuff_T *   buff) {

    return ((uint8_t *)&buff->mem[buff->tail]);
}

void circPosHeadSet(
    circBuff_T *         buff,
    s32                 position) {

    buff->free -= position;
    buff->head += position;

    if (buff->size == buff->head) {
        buff->head = 0U;
    }
}

void circPosTailSet(
    circBuff_T *         buff,
    s32                 position) {

    buff->free += position;
    buff->tail += position;

    if (buff->size == buff->tail) {
        buff->tail = 0U;
    }
}

bool_T circIsEmpty(
    const circBuff_T *   buff) {

    bool_T              ans;

    if (buff->size == buff->free) {
        ans = TRUE;
    } else {
        ans = FALSE;
    }

    return (ans);
}

bool_T circIsFull(
    const circBuff_T *   buff) {

    bool_T              ans;

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
