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
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/mman.h>

#include <native/task.h>
#include <native/timer.h>
#include <native/sem.h>
#include <native/heap.h>
#include <rtdm/rtdm.h>

/*=========================================================  LOCAL MACRO's  ==*/

#define APP_VER_MAJOR                   1U
#define APP_VER_MINOR                   0U
#define APP_VER_PATCH                   1U
#define APP_NAME                        "DUMMY"
#define APP_DESC                        "Xenomai discoverer"
#define APP_MAINTAINER                  "Nenad Radulovic <nenad.b.radulovic@gmail.com>"

#define TASK_A_NAME                     "taskA"
#define TASK_A_STKSZ                    0
#define TASK_A_MODE                     0
#define TASK_A_PRIO                     T_HIPRIO-2

#define TASK_B_NAME                     "taskB"
#define TASK_B_STKSZ                    0
#define TASK_B_MODE                     0
#define TASK_B_PRIO                     T_HIPRIO-1

#define TASK_MAN_STKSZ                  0
#define TASK_MAN_MODE                   0
#define TASK_MAN_PRIO                   T_HIPRIO

#define NS_PER_US                       1000ULL
#define US_PER_MS                       1000ULL
#define MS_PER_S                        1000ULL
#define NS_PER_MS                       (US_PER_MS * NS_PER_US)
#define NS_PER_S                        (MS_PER_S * NS_PER_MS)


#define US_TO_NS(us)                    (NS_PER_US * (us))
#define NS_TO_US(ns)                    ((ns) / NS_PER_US)
#define MS_TO_NS(ms)                    (NS_PER_MS * (ms))
#define NS_TO_MS(ns)                    ((ns) / NS_PER_MS)
#define SEC_TO_NS(sec)                  (NS_PER_S * (sec))

#define LOG_INFO(msg, ...)                                                      \
    printf(APP_NAME " " msg "\n", ##__VA_ARGS__)

#define LOG_WARN(msg, ...)                                                      \
    printf(APP_NAME " <WARN>: line %d:\t" msg "\n", __LINE__, ##__VA_ARGS__)

#define LOG_WARN_IF(expr, msg, ...)                                             \
    do {                                                                        \
        if ((expr)) {                                                           \
            LOG_WARN(msg, ##__VA_ARGS__);                                       \
        }                                                                       \
    } while (0)

#define LOG_ERR(msg, ...)                                                       \
    fprintf(stderr, APP_NAME " <ERR> : line %d:\t" msg "\n", __LINE__, ##__VA_ARGS__)

#define LOG_ERR_IF(expr, msg, ...)                                              \
    do {                                                                        \
        if ((expr)) {                                                           \
            LOG_ERR(msg, ##__VA_ARGS__);                                        \
        }                                                                       \
    } while (0)

#define LOG_DBG(msg, ...)                                                       \
    do {                                                                        \
        RTIME now = rt_timer_read();                                            \
        cnt += sprintf(&buff[cnt], "[ %llu ] " msg "\n", now, ##__VA_ARGS__);   \
    } while (0)

#define LOG_VAR(var)                                                            \
    printf(APP_NAME " _VAR_ " #var " : %d\n", var )

#define LOG_PVAR(var)                                                           \
    printf(APP_NAME " _PTR_ " #var " : %p\n", var )

/*======================================================  LOCAL DATA TYPES  ==*/

enum appBootState {
    BOOT_CREATE_A,
    BOOT_CREATE_B,
    BOOT_START_A,
    BOOT_START_B,
    BOOT_CREATE_SEM,
    BOOT_DONE
};

/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/

static void taskManager(
    void *              arg);

static void taskA(
    void *              arg);

static void taskB(
    void *              arg);

static int appInit(
    void);

static void appTerm(
    void);

/*=======================================================  LOCAL VARIABLES  ==*/

static RT_SEM           Sem;

static RT_TASK          Taskman;
static RT_TASK          TaskA;
static RT_TASK          TaskB;

static enum appBootState AppBootState;

static volatile uint32_t         A;
static volatile uint32_t         B;

static char             buff[10240];
static int32_t          cnt = 0;

/*======================================================  GLOBAL VARIABLES  ==*/
/*============================================  LOCAL FUNCTION DEFINITIONS  ==*/

static void taskValidatePrio(
    void) {

    RT_TASK_INFO thisTask;

    rt_task_inquire(
        rt_task_self(),
        &thisTask);

    if (thisTask.cprio > thisTask.bprio) {
        LOG_ERR("task %s, has raised prio from %d to %d",
            thisTask.name,
            thisTask.bprio,
            thisTask.cprio);
    }
}

static void taskPrintInfo(
    void) {

    RT_TASK_INFO thisTask;

    rt_task_inquire(
        rt_task_self(),
        &thisTask);

    LOG_INFO("task %s, has bprio %d, cprio %d",
        thisTask.name,
        thisTask.bprio,
        thisTask.cprio);
}

static void taskManager(
    void *              arg) {

    int                 retval;

    (void)arg;

    retval = appInit();

    if (0 != retval) {
        LOG_ERR("could not start the application, err: %s. Exiting", strerror(retval));
        appTerm();
        kill(
            getpid(),
            SIGTERM);

        return;
    }
    rt_task_sleep(
        MS_TO_NS(2000));
    appTerm();
    printf("a: %u, b: %u\n", A, B);
    buff[sizeof(buff) - 1] = 0;
    printf("buff: \n%s\n", buff);
    kill(getpid(), SIGTERM);
}

static void taskA(
    void *              arg) {

    (void)arg;

    taskPrintInfo();
    A = 0ULL;

    while (1) {
        LOG_DBG("A wait");
        rt_sem_p(
            &Sem,
            TM_INFINITE);
        LOG_DBG("A cont");
        A++;
        printf("A: %d\n", A);
    }
}

static void taskB(
    void *              arg) {

    (void)arg;

    taskPrintInfo();
    B = 0ULL;

    while (1) {
        LOG_DBG("B signal");
        rt_sem_v(
            &Sem);
        LOG_DBG("B signaled");
        B++;
        printf("B: %d\n", B);
        rt_task_sleep(MS_TO_NS(10));

        if (B == 10) {

            return;
        }
    }
}

static int appInit(
    void) {

    int             retval;

    AppBootState = BOOT_CREATE_SEM;
    retval = rt_sem_create(
        &Sem,
        "semaphore",
        0,
        S_PRIO);

    if (0 != retval) {
        LOG_ERR("sem create, err: %s", strerror(retval));

        return (retval);
    }

    AppBootState = BOOT_CREATE_A;
    LOG_INFO("create task: %s", TASK_A_NAME);
    retval = rt_task_create(
        &TaskA,
        TASK_A_NAME,
        TASK_A_STKSZ,
        TASK_A_PRIO,
        TASK_A_MODE);

    if (0 != retval) {
        LOG_ERR("failed to create: %s, err: %s)", TASK_A_NAME, strerror(retval));

        return (retval);
    }

    AppBootState = BOOT_START_A;
    LOG_INFO("start task : %s", TASK_A_NAME);
    retval = rt_task_start(
        &TaskA,
        taskA,
        NULL);

    if (0 != retval) {
        LOG_ERR("failed to start: %s, err: %s", TASK_A_NAME, strerror(retval));

        return (retval);
    }

    AppBootState = BOOT_CREATE_B;
    LOG_INFO("create task: %s", TASK_B_NAME);
    retval = rt_task_create(
        &TaskB,
        TASK_B_NAME,
        TASK_B_STKSZ,
        TASK_B_PRIO,
        TASK_B_MODE);

    if (0 != retval) {
        LOG_ERR("failed to create: %s, err: %s", TASK_B_NAME, strerror(retval));

        return (retval);
    }

    AppBootState = BOOT_START_B;
    LOG_INFO("start task : %s", TASK_B_NAME);
    retval = rt_task_start(
        &TaskB,
        taskB,
        NULL);

    if (0 != retval) {
        LOG_ERR("failed to start: %s, err: %s", TASK_B_NAME, strerror(retval));

        return (retval);
    }

    AppBootState = BOOT_DONE;

    return (0);
}

static void appTerm(
    void) {

    LOG_INFO("exiting application");
    switch (AppBootState) {
        case BOOT_DONE :
        case BOOT_START_B :
            rt_task_delete(
                &TaskB);
        case BOOT_CREATE_B :
        case BOOT_START_A :
            rt_task_delete(
                &TaskA);
        case BOOT_CREATE_A :
            rt_sem_delete(
                &Sem);
        case BOOT_CREATE_SEM:
            break;

        default : {

        }
    }
}

/*===================================  GLOBAL PRIVATE FUNCTION DEFINITIONS  ==*/
/*====================================  GLOBAL PUBLIC FUNCTION DEFINITIONS  ==*/

int main(
    int                 argc,
    char **             argv) {

    int                 retval;
    int                 sig;
    sigset_t            sigset;

    (void)argc;
    (void)argv;

    printf("\n" APP_DESC "\n");
    sigemptyset(
        &sigset);
    sigaddset(
        &sigset,
        SIGINT);
    sigaddset(
        &sigset,
        SIGTERM);
    sigaddset(
        &sigset,
        SIGHUP);
    sigaddset(
        &sigset,
        SIGALRM);
    pthread_sigmask(
        SIG_BLOCK,
        &sigset,
        NULL);
    mlockall(
        MCL_CURRENT | MCL_FUTURE);

    retval = rt_task_create(
        &Taskman,
        "RTDEV_test_man",
        TASK_MAN_STKSZ,
        TASK_MAN_PRIO,
        TASK_MAN_MODE);

    if (0 != retval) {
        LOG_ERR("failed to create task manager, err: %s", strerror(retval));

        return (retval);
    }
    retval = rt_task_start(
        &Taskman,
        taskManager,
        NULL);

    if (0 != retval) {
        LOG_ERR("failed to start task manager, err: %s", strerror(retval));

        return (retval);
    }
    sigwait(
        &sigset,
        &sig);
    appTerm();

    return (retval);
}

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of main.c
 ******************************************************************************/
