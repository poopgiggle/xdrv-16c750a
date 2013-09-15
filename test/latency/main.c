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
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/mman.h>

#include <native/task.h>
#include <native/timer.h>
#include <native/event.h>
#include <rtdm/rtdm.h>

/*=========================================================  LOCAL MACRO's  ==*/

#define DEVICE_DRIVER_NAME              "xuart"

#define APP_NAME                        "UART_latency"

#define TASK_SEND_NAME                  "UART_tester_send"
#define TASK_SEND_STKSZ                 0
#define TASK_SEND_MODE                  0
#define TASK_SEND_PRIO                  97

#define TASK_RECV_NAME                  "UART_tester_recv"
#define TASK_RECV_STKSZ                 0
#define TASK_RECV_MODE                  0
#define TASK_RECV_PRIO                  98

#define TASK_PRINT_NAME                 "UART_tester_print"
#define TASK_PRINT_STKSZ                0
#define TASK_PRINT_MODE                 0
#define TASK_PRINT_PRIO                 96

#define TEST_DATA                       {'0','1','2','3','4','5','6','7','8','9'}

#define RECV_RETRY_CNT                  10U

#define TABLE_HEADER                                                                                \
    "---------------------------------------------------------------------------------------\n"     \
    " Nr.  |   min   |  TURN   |   max   |   avg   |   min   |    TX   |   max   |   avg   |\n"     \
    "---------------------------------------------------------------------------------------\n"

#define NS_PER_US                       1000
#define US_PER_MS                       1000
#define MS_PER_S                        1000

#define NS_PER_MS                       (US_PER_MS * NS_PER_US)
#define NS_PER_S                        (MS_PER_S * NS_PER_MS)
#define US_TO_NS(us)                    (NS_PER_US * (us))
#define MS_TO_NS(ms)                    (NS_PER_MS * (ms))
#define SEC_TO_NS(sec)                  (NS_PER_S * (sec))

#define LOG_INFO(msg, ...)                                                      \
    printf(APP_NAME " <INFO>: " msg "\n", ##__VA_ARGS__)

#define LOG_WARN(msg, ...)                                                      \
    printf(APP_NAME " <WARN>: line %d: " msg "\n", __LINE__, ##__VA_ARGS__)

#define LOG_WARN_IF(expr, msg, ...)                                             \
    do {                                                                        \
        if ((expr)) {                                                           \
            LOG_WARN(msg, ##__VA_ARGS__);                                       \
        }                                                                       \
    } while (0)

#define LOG_ERR(msg, ...)                                                       \
    printf(APP_NAME " <ERR> : line %d: " msg "\n", __LINE__, ##__VA_ARGS__)

#define LOG_ERR_IF(expr, msg, ...)                                              \
    do {                                                                        \
        if ((expr)) {                                                           \
            LOG_ERR(msg, ##__VA_ARGS__);                                        \
        }                                                                       \
    } while (0)

#define LOG_VAR(var)                                                            \
    printf(APP_NAME " _VAR_" #var " : %d\n", var )

#define LOG_PVAR(var)                                                           \
    printf(APP_NAME " _PTR_" #var " : %p\n", var )

/*======================================================  LOCAL DATA TYPES  ==*/

struct timeMeas {
    volatile RTIME      txBegin;
    volatile RTIME      txEnd;
    volatile RTIME      rxEnd;
};

struct meas {
    uint64_t            cnt;
    RTIME               curr;
    RTIME               sum;
    RTIME               avg;
    RTIME               min;
    RTIME               max;
};

/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/

static void taskSend(
    void *              arg);

static void taskRecv(
    void *              arg);

static void taskPrint(
    void *              arg);

static void initMeas(
    struct meas *       meas);

static void calcMeas(
    struct meas *       meas,
    RTIME               new);

static void catch(
    int                 sig);

/*=======================================================  LOCAL VARIABLES  ==*/

static RT_TASK          taskSendDesc;
static RT_TASK          taskRecvDesc;
static RT_TASK          taskPrintDesc;

static int              gDev;
static RT_EVENT         gPrint;
static const char       gTestData[] = TEST_DATA;

static volatile struct timeMeas gTime;

/*======================================================  GLOBAL VARIABLES  ==*/
/*============================================  LOCAL FUNCTION DEFINITIONS  ==*/


/*===================================  GLOBAL PRIVATE FUNCTION DEFINITIONS  ==*/
/*====================================  GLOBAL PUBLIC FUNCTION DEFINITIONS  ==*/

static void taskSend(
    void *              arg) {

    int                 retval;

    (void)arg;

    retval = rt_task_set_periodic(
        NULL,
        TM_NOW,
        MS_TO_NS(10));
    LOG_ERR_IF(0 != retval, "failed to set period (err: %d)", retval);

    while (true) {
        rt_task_wait_period(NULL);
        gTime.txBegin = rt_timer_read();
        rt_dev_write(
            gDev,
            &gTestData,
            sizeof(gTestData));
        gTime.txEnd = rt_timer_read();
    }
}

static void taskRecv(
    void *              arg) {

    uint32_t            retry;

    (void)arg;

    retry = RECV_RETRY_CNT;

    while (true) {
        int             retval;
        char            recv[sizeof(gTestData)];
        ssize_t         len;

        len = rt_dev_read(
            gDev,
            &recv,
            sizeof(recv));
        gTime.rxEnd = rt_timer_read();

        if (-EBADF == len) {

            break;
        }
        if (len != sizeof(gTestData)) {
            LOG_WARN("received string has invalid length: %d, retry: %d", len, retry);
            retry--;

            if (0U == retry) {
                LOG_ERR("failed reception");

                break;
            } else {

                continue;
            }
        } else {
            retry = RECV_RETRY_CNT;
        }

        retval = rt_event_signal(
            &gPrint,
            0x01UL);

        if (0 != retval) {
            LOG_ERR("event signal (err: %d)", retval);

            break;
        }
    }
    rt_event_delete(
        &gPrint);
}

static void taskPrint(
    void *              arg) {

    unsigned long       mask;
    int                 retval;
    uint32_t            cntr;
    struct meas         turn;
    struct meas         tx;

    (void)arg;

    retval = rt_event_create(
        &gPrint,
        NULL,
        0,
        EV_PRIO);

    if (0 != retval) {
        LOG_ERR("event wait (err: %d)", retval);

        return;
    }
    cntr = 0UL;
    initMeas(
        &turn);
    initMeas(
        &tx);

    while (true) {
        uint64_t        turnCurr;
        uint64_t        txCurr;

        retval = rt_event_wait(
            &gPrint,
            0x01UL,
            &mask,
            EV_ANY,
            TM_INFINITE);

        if (-EIDRM == retval) {

            return;
        }

        if (0 != retval) {
            LOG_ERR("event wait (err: %d)", retval);

            return;
        }
        rt_event_clear(
            &gPrint,
            0x01UL,
            NULL);
        turnCurr = gTime.rxEnd - gTime.txBegin;
        txCurr   = gTime.txEnd - gTime.txBegin;
        calcMeas(
            &turn,
            turnCurr);
        calcMeas(
            &tx,
            txCurr);

        if (0U == (cntr % 40U)) {
            printf(TABLE_HEADER);
        }
        cntr++;
        printf(" %4d | %5d.%1d | %5d.%1d | %5d.%1d | %5d.%1d | %5d.%1d | %5d.%1d | %5d.%1d | %5d.%1d |\n",
            cntr,
            (int32_t)turn.min / 1000,
            ((int32_t)turn.min % 1000) / 100,
            (int32_t)turn.curr / 1000,
            ((int32_t)turn.curr % 1000) / 100,
            (int32_t)turn.max / 1000,
            ((int32_t)turn.max % 1000) / 100,
            (int32_t)turn.avg / 1000,
            ((int32_t)turn.avg % 1000) / 100,
            (int32_t)tx.min / 1000,
            ((int32_t)tx.min % 1000) / 100,
            (int32_t)tx.curr / 1000,
            ((int32_t)tx.curr % 1000) / 100,
            (int32_t)tx.max / 1000,
            ((int32_t)tx.max % 1000) / 100,
            (int32_t)tx.avg / 1000,
            ((int32_t)tx.avg % 1000) / 100);
    }
}


static void initMeas(
    struct meas *       meas) {
    meas->cnt  = 0UL;
    meas->sum  = 0U;
    meas->curr = 0U;
    meas->avg  = 0U;
    meas->min  = (RTIME)-1;
    meas->max  = 0U;
}

static void calcMeas(
    struct meas *       meas,
    RTIME               new) {
    meas->curr = new;
    meas->cnt++;

    if (1UL == meas->cnt) {
        meas->avg = meas->curr;
    } else {
        meas->avg = (meas->sum + meas->curr) / meas->cnt;
    }
    meas->sum += meas->curr;

    if (meas->min > meas->curr) {
        meas->min = meas->curr;
    }

    if (meas->max < meas->curr) {
        meas->max = meas->curr;
    }
}

static void catch(
    int                 sig) {

    (void)sig;
}

int main(
    int                 argc,
    char **             argv) {

    int                 retval;

    (void)argc;
    (void)argv;

    printf("\nReal-Time UART driver latency tester\n");
    signal(
        SIGTERM,
        catch);
    signal(
        SIGINT,
        catch);
    mlockall(
        MCL_CURRENT | MCL_FUTURE);

    LOG_INFO("open device: %s", DEVICE_DRIVER_NAME);
    gDev = rt_dev_open(
        DEVICE_DRIVER_NAME,
        0);
    if (0 > gDev) {
        LOG_ERR("failed to open device (err: %d)", gDev);

        return (gDev);
    }

    LOG_INFO("create task: %s", TASK_SEND_NAME);
    retval = rt_task_create(
        &taskSendDesc,
        TASK_SEND_NAME,
        TASK_SEND_STKSZ,
        TASK_SEND_PRIO,
        TASK_SEND_MODE);

    if (0 != retval) {
        LOG_ERR("failed to create: %s (err: %d)", TASK_SEND_NAME, retval);
        rt_dev_close(
            gDev);

        return (retval);
    }
    LOG_INFO("create task: %s", TASK_RECV_NAME);
    retval = rt_task_create(
        &taskRecvDesc,
        TASK_RECV_NAME,
        TASK_RECV_STKSZ,
        TASK_RECV_PRIO,
        TASK_RECV_MODE);

    if (0 != retval) {
        LOG_ERR("failed to create: %s (err: %d)", TASK_RECV_NAME, retval);
        rt_task_delete(
            &taskSendDesc);
        rt_dev_close(
            gDev);

        return (retval);
    }
    LOG_INFO("create task: %s", TASK_PRINT_NAME);
    retval = rt_task_create(
        &taskPrintDesc,
        TASK_PRINT_NAME,
        TASK_PRINT_STKSZ,
        TASK_PRINT_PRIO,
        TASK_PRINT_MODE);

    if (0 != retval) {
        LOG_ERR("failed to create: %s (err: %d)", TASK_PRINT_NAME, retval);
        rt_task_delete(
            &taskSendDesc);
        rt_task_delete(
            &taskRecvDesc);
        rt_dev_close(
            gDev);

        return (retval);
    }

    LOG_INFO("start task : %s", TASK_RECV_NAME);
    retval = rt_task_start(
        &taskRecvDesc,
        taskRecv,
        NULL);
    LOG_ERR_IF(0 != retval, "failed to start: %s (err: %d)", TASK_RECV_NAME, retval);
    LOG_INFO("start task : %s", TASK_SEND_NAME);
    rt_task_start(
        &taskSendDesc,
        taskSend,
        NULL);
    LOG_ERR_IF(0 != retval, "failed to start: %s (err: %d)", TASK_SEND_NAME, retval);
    LOG_INFO("start task : %s", TASK_PRINT_NAME);
        rt_task_start(
            &taskPrintDesc,
            taskPrint,
            NULL);
        LOG_ERR_IF(0 != retval, "failed to start: %s (err: %d)", TASK_PRINT_NAME, retval);
    printf("\n-----------------------------------------------------------------\n");
    printf("  Message length: %d\n", sizeof(gTestData));
    printf("-----------------------------------------------------------------\n\n");
    pause();
    LOG_INFO("terminating");
    rt_task_delete(
        &taskPrintDesc);
    rt_task_delete(
        &taskSendDesc);
    rt_task_delete(
        &taskRecvDesc);
    rt_dev_close(
        gDev);

    return (retval);
}

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of main.c
 ******************************************************************************/
