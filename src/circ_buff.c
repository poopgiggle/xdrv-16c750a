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

/*=========================================================  LOCAL MACRO's  ==*/
/*======================================================  LOCAL DATA TYPES  ==*/
/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/
/*=======================================================  LOCAL VARIABLES  ==*/
/*======================================================  GLOBAL VARIABLES  ==*/
/*============================================  LOCAL FUNCTION DEFINITIONS  ==*/
/*===================================  GLOBAL PRIVATE FUNCTION DEFINITIONS  ==*/
/*====================================  GLOBAL PUBLIC FUNCTION DEFINITIONS  ==*/

void circInit(
    CIRC_BUFF *         buff,
    void *              mem,
    size_t              size) {

    buff->mem  = (u8 *)mem;
    buff->head = 0U;
    buff->tail = 0U;
    buff->size = (u32)size;
    buff->free = (u32)size;
}

void circItemPut(
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

u8 circItemGet(
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

size_t circRemainingGet(
    const CIRC_BUFF *   buff) {

    u32                 tmp;

    if (buff->tail > buff->head) {
        tmp = buff->tail - buff->head;
    } else {
        tmp = buff->size - buff->head;
    }

    return ((size_t)tmp);
}

u8 * circMemBaseGet(
    const CIRC_BUFF *   buff) {

    return ((u8 *)buff->mem);
}

u8 * circMemHeadGet(
    const CIRC_BUFF *   buff) {

    return ((u8 *)&buff->mem[buff->head]);
}

void circHeadPosSet(
    CIRC_BUFF *         buff,
    s32                 position) {

    buff->free -= position;
    buff->head += position;

    if (buff->size == buff->head) {
        buff->head = 0U;
    }
}

BOOLEAN circIsEmpty(
    const CIRC_BUFF *   buff) {

    BOOLEAN             ans;

    if (buff->size == buff->free) {
        ans = TRUE;
    } else {
        ans = FALSE;
    }

    return (ans);
}

BOOLEAN circIsFull(
    const CIRC_BUFF *   buff) {

    BOOLEAN             ans;

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
