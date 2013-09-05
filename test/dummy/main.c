/*
 * This file is part of x-16c750-app
 *
 * Copyright (C) 2011, 2012 - Nenad Radulovic
 *
 * x-16c750-app is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * x-16c750-app is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with x-16c750-app; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * web site:    http://blueskynet.dyndns-server.com
 * e-mail  :    blueskyniss@gmail.com
 *//***********************************************************************//**
 * @file
 * @author      Nenad Radulovic
 *********************************************************************//** @{ */

/*=========================================================  INCLUDE FILES  ==*/

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <native/task.h>
#include <native/heap.h>
#include <rtdm/rtdm.h>
#include <stdint.h>
#include <unistd.h>

/*=========================================================  LOCAL MACRO's  ==*/

#define DEVICE_DRIVER_NAME              "xuart"
#define TASK_NAME                       "UART_tester"
#define TASK_STKSZ                      0
#define TASK_MODE                       0
#define TASK_PRIO                       99
#define HEAP_SIZE                       100000

/*======================================================  LOCAL DATA TYPES  ==*/
/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/
/*=======================================================  LOCAL VARIABLES  ==*/

static RT_TASK taskDesc;
static RT_HEAP textHeap;
static char * text;

/*======================================================  GLOBAL VARIABLES  ==*/
/*============================================  LOCAL FUNCTION DEFINITIONS  ==*/

static void task(
    void *              arg);

/*===================================  GLOBAL PRIVATE FUNCTION DEFINITIONS  ==*/
/*====================================  GLOBAL PUBLIC FUNCTION DEFINITIONS  ==*/

static void task(
    void *              arg) {

    int                 retval;
    int                 device;

    device = (int)arg;

    uint32_t i;
    retval = rt_heap_create(&textHeap, "nesto", HEAP_SIZE, H_PRIO);

    if (0 != retval) {
        printf("ERROR: heap create\n");
        fflush(stdout);

        return;
    }
    retval = rt_heap_alloc(&textHeap, HEAP_SIZE, TM_INFINITE, (void **)&text);

    if (0 != retval) {
        printf("ERROR: heap alloc\n");
        fflush(stdout);

        return;
    }

    for (i = 0U; i < HEAP_SIZE; i++) {
        text[i] = 'a';
    }
    retval = rt_dev_write(
        device,
        text,
        HEAP_SIZE);

    rt_heap_free(
        &textHeap,
        text);
    rt_heap_delete(
        &textHeap);

    printf("sent %d bytes\n", retval);
}

int main(
    int                 argc,
    char **             argv) {

    int                 retval;
    int                 device;

    printf("Real-Time UART tester\n");
    mlockall(MCL_CURRENT | MCL_FUTURE);

    retval = rt_task_create(
        &taskDesc,
        TASK_NAME,
        TASK_STKSZ,
        TASK_PRIO,
        TASK_MODE);

    if (0 == retval) {
        uint32_t i;

        device = rt_dev_open(DEVICE_DRIVER_NAME, 0);

        if (0 > device) {
            printf("ERROR: failed to open device: %s (%d)\n", DEVICE_DRIVER_NAME, -device);
            fflush(stdout);
            (void)rt_task_delete(
                &taskDesc);
        }
        retval = rt_task_start(
            &taskDesc,
            task,
            (void *)device);

        printf("wait.\n");
        sleep(4);
        retval = rt_dev_close(
            device);

        if (0 != retval) {
            printf("ERROR: failed to close device: %s (%d)\n", DEVICE_DRIVER_NAME, -retval);
        }
        retval = rt_task_delete(
            &taskDesc);
    }

    return (retval);
}

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of main.c
 ******************************************************************************/
