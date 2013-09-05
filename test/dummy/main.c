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
#define TASK_SEND_NAME                  "UART_tester_send"
#define TASK_RECV_NAME                  "UART_tester_recv"
#define TASK_STKSZ                      0
#define TASK_MODE                       0
#define TASK_PRIO                       99

#if !defined(TEST1) && !defined(TEST2)
#define TEST1
#endif

#if defined(TEST1)
#define HEAP_SIZE                       100000
#elif defined(TEST2)
#define HEAP_SIZE                       30
#endif

/*======================================================  LOCAL DATA TYPES  ==*/
/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/
/*=======================================================  LOCAL VARIABLES  ==*/

#if defined(TEST1) || defined(TEST2)
static RT_TASK taskSendDesc;
static RT_HEAP textSendHeap;
#endif
#if defined(TEST2)
static RT_TASK taskRecvDesc;
static RT_HEAP textRecvHeap;
#endif
static char * text;

/*======================================================  GLOBAL VARIABLES  ==*/
/*============================================  LOCAL FUNCTION DEFINITIONS  ==*/

static void taskSend(
    void *              arg);

/*===================================  GLOBAL PRIVATE FUNCTION DEFINITIONS  ==*/
/*====================================  GLOBAL PUBLIC FUNCTION DEFINITIONS  ==*/

static void taskSend(
    void *              arg) {

    int                 retval;
    int                 device;

    device = (int)arg;

    uint32_t i;
    retval = rt_heap_create(&textSendHeap, "nesto", HEAP_SIZE, H_PRIO);

    if (0 != retval) {
        printf("ERROR: heap create\n");
        fflush(stdout);

        return;
    }
    retval = rt_heap_alloc(&textSendHeap, HEAP_SIZE, TM_INFINITE, (void **)&text);

    if (0 != retval) {
        printf("ERROR: heap alloc\n");
        fflush(stdout);

        return;
    }

    for (i = 0U; i < HEAP_SIZE; i++) {
        text[i] = (char)('a' + i);
    }
    retval = rt_dev_write(
        device,
        text,
        HEAP_SIZE);

    printf("sent %d bytes\n", retval);
    rt_heap_free(
        &textSendHeap,
        text);
    rt_heap_delete(
        &textSendHeap);
}

#if defined(TEST2)
static void taskRecv(
    void *              arg) {

    int                 retval;
    int                 device;

    device = (int)arg;

    uint32_t i;
    retval = rt_heap_create(&textSendHeap, "nesto", HEAP_SIZE, H_PRIO);

    if (0 != retval) {
        printf("ERROR: heap create\n");
        fflush(stdout);

        return;
    }
    retval = rt_heap_alloc(&textSendHeap, HEAP_SIZE, TM_INFINITE, (void **)&text);

    if (0 != retval) {
        printf("ERROR: heap alloc\n");
        fflush(stdout);

        return;
    }
    printf("receive:\n");
    memset(text, 0, HEAP_SIZE);
    retval = rt_dev_read(
        device,
        text,
        HEAP_SIZE);

    printf("recv %d bytes\n", retval);
    text[HEAP_SIZE - 1U] = 0;
    printf("\n Recvd text: %s", text);
    rt_heap_free(
        &textSendHeap,
        text);
    rt_heap_delete(
        &textSendHeap);
}
#endif

int main(
    int                 argc,
    char **             argv) {

    int                 retval;
    int                 device;

    printf("Real-Time UART tester\n");
    mlockall(MCL_CURRENT | MCL_FUTURE);

    printf("Create: SEND task\n");
    retval = rt_task_create(
        &taskSendDesc,
        TASK_SEND_NAME,
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
                &taskSendDesc);
        }
        retval = rt_task_start(
            &taskSendDesc,
            taskSend,
            (void *)device);

        printf("wait.\n");
        sleep(4);
        retval = rt_dev_close(
            device);

        if (0 != retval) {
            printf("ERROR: failed to close device: %s (%d)\n", DEVICE_DRIVER_NAME, -retval);
        }
        retval = rt_task_delete(
            &taskSendDesc);
    }
#if defined(TEST2)
    sleep(1);
    printf("Create: RECV task\n");
    retval = rt_task_create(
        &taskRecvDesc,
        TASK_RECV_NAME,
        TASK_STKSZ,
        TASK_PRIO,
        TASK_MODE);

    if (0 == retval) {
        device = rt_dev_open(DEVICE_DRIVER_NAME, 0);

        if (0 > device) {
            printf("ERROR: failed to open device: %s (%d)\n", DEVICE_DRIVER_NAME, -device);
            fflush(stdout);
            (void)rt_task_delete(
                &taskRecvDesc);
        }
        retval = rt_task_start(
            &taskRecvDesc,
            taskRecv,
            (void *)device);

        printf("wait.\n");
        sleep(4);
        retval = rt_dev_close(
            device);

        if (0 != retval) {
            printf("ERROR: failed to close device: %s (%d)\n", DEVICE_DRIVER_NAME, -retval);
        }
        retval = rt_task_delete(
            &taskRecvDesc);
    }
#endif

    return (retval);
}

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of main.c
 ******************************************************************************/
