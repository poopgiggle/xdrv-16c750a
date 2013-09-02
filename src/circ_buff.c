/*
 * This file is part of x-16c750
 *
 * Template version: 1.1.15 (03.07.2013)
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
 * @author      nenad
 * @brief       Short desciption of file
 * @addtogroup  module_impl
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

    buff->mem = (u32 *)mem;
    buff->head = 0U;
    buff->tail = 0U;
    buff->size = (u32)size;
}

void circItemPut(
    CIRC_BUFF *         buff,
    u32                 item) {

    buff->mem[buff->head] = item;
    smp_wmb();
    buff->head++;

    if (buff->head == buff->size) {
        buff->head = 0;
    }
}

u32 circItemGet(
    CIRC_BUFF *         buff) {

    u32                 tmp;

    smp_mb();
    buff->tail++;

    if (buff->tail == buff->size) {
        buff->tail = 0;
    }
    smp_read_barrier_depends();
    tmp = buff->mem[buff->tail];

    return (tmp);
}

size_t circFreeSizeGet(
    const CIRC_BUFF *   buff) {

    u32                 ans;

    if (buff->head >= buff->tail) {
        ans = buff->size - buff->head + buff->tail;
    } else {
        ans = buff->tail - buff->head;
    }
    return ((size_t)ans);
}

size_t circSizeGet(
    const CIRC_BUFF *   buff) {

    return ((size_t)buff->size);
}

void * circBuffGet(
    const CIRC_BUFF *   buff) {

    return ((void *)buff->mem);
}

BOOLEAN circIsEmpty(
    CIRC_BUFF *         buff) {

    BOOLEAN             ans;

    if (buff->head == buff->tail) {
        ans = TRUE;
    } else {
        ans = FALSE;
    }

    return (ans);
}


BOOLEAN circIsFull(
    CIRC_BUFF *         buff) {

    BOOLEAN             ans;

    if ((buff->head + 1U) == buff->tail) {
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
