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
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/mman.h>

#include <native/task.h>
#include <native/timer.h>
#include <native/event.h>
#include <native/heap.h>
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

#define HEAP_SIZE                       1024UL*1

#define DATA_SIZE                       HEAP_SIZE

#define RECV_RETRY_CNT                  10U

#define BAUD_RATE                       921600ULL
#define STOP_BITS                       1ULL
#define DATA_BITS                       8ULL
#define PARITY_BITS                     0ULL

#define TRANSMISION_TIME(size)                                                  \
    ((1000000000ULL * (uint64_t)(STOP_BITS + DATA_BITS + PARITY_BITS + 1ULL) * (uint64_t)(size)) / BAUD_RATE)


#define TABLE_HEADER                                                                                \
    "----------------------------------------------------------------------------------------\n"    \
    " Nr.   |   min   |  TURN   |   max   |   avg   |   min   |    TX   |   max   |   avg   |\n"    \
    "----------------------------------------------------------------------------------------\n"

#define NS_PER_US                       1000
#define US_PER_MS                       1000
#define MS_PER_S                        1000

#define NS_PER_MS                       (US_PER_MS * NS_PER_US)
#define NS_PER_S                        (MS_PER_S * NS_PER_MS)
#define US_TO_NS(us)                    (NS_PER_US * (us))
#define MS_TO_NS(ms)                    (NS_PER_MS * (ms))
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
    printf(APP_NAME " <ERR> : line %d:\t" msg "\n", __LINE__, ##__VA_ARGS__)

#define LOG_ERR_IF(expr, msg, ...)                                              \
    do {                                                                        \
        if ((expr)) {                                                           \
            LOG_ERR(msg, ##__VA_ARGS__);                                        \
        }                                                                       \
    } while (0)

#define LOG_VAR(var)                                                            \
    printf(APP_NAME " _VAR_ " #var " : %d\n", var )

#define LOG_PVAR(var)                                                           \
    printf(APP_NAME " _PTR_ " #var " : %p\n", var )

/*======================================================  LOCAL DATA TYPES  ==*/

enum dataType {
    DATA_LINEAR
};

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

static void dataGen(
    enum dataType       type,
    uint8_t *           src,
    size_t size);

static uint32_t dataIsValid(
    void *              src,
    void *              dst,
    size_t              size);

/*=======================================================  LOCAL VARIABLES  ==*/

static RT_TASK          taskSendDesc;
static RT_TASK          taskRecvDesc;
static RT_TASK          taskPrintDesc;

static int              gDev;

static RT_EVENT         gSync;

static RT_HEAP          gTxStorage;
static void *           gTxBuff;
static RT_HEAP          gRxStorage;
static void *           gRxBuff;

static uint32_t         gInvalidChars;
static uint32_t         gMissingChars;

static volatile struct timeMeas gTime;

#define EVT_ID_PRINT    0x01U
#define EVT_ID_SEND     0x02U

/*======================================================  GLOBAL VARIABLES  ==*/
/*============================================  LOCAL FUNCTION DEFINITIONS  ==*/

static void taskSend(
    void *              arg) {

    int                 retval;
    unsigned long       mask;

    (void)arg;

    retval = rt_event_create(
        &gSync,
        "evtTransmit",
        0,
        EV_PRIO);

    if (0 != retval) {
        LOG_ERR("event wait (err: %d)", retval);

        return;
    }
    dataGen(
        DATA_LINEAR,
        (void *)&gTxBuff,
        DATA_SIZE);

    while (true) {

        retval = rt_event_wait(
            &gSync,
            EVT_ID_SEND,
            &mask,
            EV_ANY,
            TM_INFINITE);

        if (0 != retval) {

            if (-EIDRM == retval) {

                return;
            }
            LOG_ERR("event wait (err: %d)", retval);

            return;
        }
        rt_event_clear(
            &gSync,
            EVT_ID_SEND,
            NULL);
        gTime.txBegin = rt_timer_read();
        rt_dev_write(
            gDev,
            &gTxBuff,
            DATA_SIZE);
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
        ssize_t         len;

        retval = rt_event_signal(
            &gSync,
            EVT_ID_SEND);

        if (0 != retval) {
            LOG_ERR("event signal (err: %d)", retval);

            break;
        }
        len = rt_dev_read(
            gDev,
            (void *)gRxBuff,
            DATA_SIZE);
        gTime.rxEnd = rt_timer_read();

        if (-EBADF == len) {

            break;
        }
        if (0 > len) {
            LOG_ERR("failed reception (err: %d)", len);

            break;
        }
        gMissingChars = (uint32_t)((uint64_t)DATA_SIZE - (uint64_t)len);

        if (0U != gMissingChars) {
            LOG_WARN("received string has missing chars: %d, retry: %d", gMissingChars, retry);
        }
        gInvalidChars = dataIsValid(&gTxBuff, gRxBuff, DATA_SIZE);
        memset(gRxBuff, 0, DATA_SIZE);

        if (0U != gInvalidChars) {
            LOG_WARN("received string has invalid chars: %d, retry: %d", gInvalidChars, retry);
        }

        if ((0U == gMissingChars) && (0U == gInvalidChars)) {
            retry = RECV_RETRY_CNT;
        } else {
            retry--;

            if (0U == retry) {

                break;
            } else {

                continue;
            }
        }
        retval = rt_event_signal(
            &gSync,
            EVT_ID_PRINT);

        if (0 != retval) {
            LOG_ERR("event signal (err: %d)", retval);

            break;
        }
    }
}

static void taskPrint(
    void *              arg) {

    unsigned long       mask;
    int                 retval;
    uint32_t            cntr;
    struct meas         turn;
    struct meas         tx;

    (void)arg;

    cntr = 0UL;
    initMeas(
        &turn);
    initMeas(
        &tx);

    while (true) {
        uint64_t        turnCurr;
        uint64_t        txCurr;

        retval = rt_event_wait(
            &gSync,
            EVT_ID_PRINT,
            &mask,
            EV_ANY,
            TM_INFINITE);

        if (0 != retval) {
            if (-EIDRM == retval) {

                break;
            }
            LOG_ERR("event wait (err: %d)", retval);

            break;
        }
        rt_event_clear(
            &gSync,
            EVT_ID_PRINT,
            NULL);
        turnCurr = gTime.rxEnd - gTime.txBegin;
        turnCurr = turnCurr / 1000ULL;
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

static void dataGen(
    enum dataType       type,
    uint8_t *           src,
    size_t              size) {

    if (size >= UINT32_MAX) {
        LOG_ERR("size value too big");
        size = UINT32_MAX;
    }

    switch (type) {
        case DATA_LINEAR : {
            uint32_t i;

            for (i = 0U; i < size; i++) {
                src[i] = (uint8_t)(0xFFU & i);
            }
            break;
        }

        default : {
            memset(src, 0xFFU, size);

            break;
        }
    }
}

static uint32_t dataIsValid(
    void *              src,
    void *              dst,
    size_t              size) {

    uint32_t            i;
    uint32_t            cnt;
    uint8_t *           pSrc;
    uint8_t *           pDst;

    cnt = 0U;
    pSrc = (uint8_t *)src;
    pDst = (uint8_t *)dst;

    for (i = 0U; i < size; i++) {

        if (*pSrc != *pDst) {
            cnt++;
        }
        pSrc++;
        pDst++;
    }

    return (cnt);
}

enum appBootState {
    BOOT_DEV_OPEN,
    BOOT_CREATE_TX,
    BOOT_ALLOC_TX,
    BOOT_CREATE_RX,
    BOOT_ALLOC_RX
};

enum appBootState gAppBootState;

uint32_t appInit(
    void) {

    uint32_t            retval;

    /*-- BOOT_DEV_OPEN -------------------------------------------------------*/
    gAppBootState = BOOT_DEV_OPEN;
    LOG_INFO("open device: %s", DEVICE_DRIVER_NAME);
    gDev = rt_dev_open(
        DEVICE_DRIVER_NAME,
        0);

    if (0 > gDev) {
        LOG_ERR("failed to open device, err: %s)", strerror(gDev));

        return (gDev);
    }

    /*-- BOOT_CREATE_TX ------------------------------------------------------*/
    gAppBootState = BOOT_CREATE_TX;
    LOG_INFO("reserving memory");
    retval = rt_heap_create(
        &gTxStorage,
        "TXbuff_",
        HEAP_SIZE,
        H_SINGLE);

    if (0 != retval) {
        LOG_ERR("failed to create TX buffer, err: %s)", strerror(retval));

        return (retval);
    }

    /*-- BOOT_ALLOC_TX -------------------------------------------------------*/
    gAppBootState = BOOT_ALLOC_TX;
    retval = rt_heap_alloc(
        &gTxStorage,
        HEAP_SIZE,
        TM_NONBLOCK,
        &gTxBuff);

    if (0 != retval) {
        LOG_ERR("unable to allocate TX buffer, err: %s", strerror(retval));

        return (retval);
    }

    /*-- BOOT_CREATE_RX ------------------------------------------------------*/
    gAppBootState = BOOT_CREATE_RX;
    retval = rt_heap_create(
        &gRxStorage,
        "RXbuff_",
        HEAP_SIZE,
        H_SINGLE);

    if (0 != retval) {
        LOG_ERR("failed to create RX buffer, err: %s", strerror(retval));

        return (retval);
    }

    gAppBootState = BOOT_ALLOC_RX;
    retval = rt_heap_alloc(
        &gRxStorage,
        HEAP_SIZE,
        TM_NONBLOCK,
        &gRxBuff);

    if (0 != retval) {
        LOG_ERR("unable to allocate buffer, err: %s)", strerror(retval));

        return (retval);
    }

    LOG_INFO("create task: %s", TASK_SEND_NAME);
    retval = rt_task_create(
        &taskSendDesc,
        TASK_SEND_NAME,
        TASK_SEND_STKSZ,
        TASK_SEND_PRIO,
        TASK_SEND_MODE);

    if (0 != retval) {
        LOG_ERR("failed to create: %s, err: %s)", TASK_SEND_NAME, strerror(retval));
        rt_dev_close(
            gDev);

        return (retval);
    }
    LOG_INFO("start task : %s", TASK_SEND_NAME);
    retval = rt_task_start(
        &taskSendDesc,
        taskSend,
        NULL);
    LOG_ERR_IF(0 != retval, "failed to start: %s, err: %s", TASK_SEND_NAME, strerror(retval));

    LOG_INFO("create task: %s", TASK_PRINT_NAME);
    retval = rt_task_create(
        &taskPrintDesc,
        TASK_PRINT_NAME,
        TASK_PRINT_STKSZ,
        TASK_PRINT_PRIO,
        TASK_PRINT_MODE);

    if (0 != retval) {
        LOG_ERR("failed to create: %s, err: %s", TASK_PRINT_NAME, strerror(retval));
        rt_task_delete(
            &taskSendDesc);
        rt_task_delete(
            &taskRecvDesc);
        rt_dev_close(
            gDev);

        return (retval);
    }
    LOG_INFO("start task : %s", TASK_PRINT_NAME);
    retval = rt_task_start(
        &taskPrintDesc,
        taskPrint,
        NULL);
    LOG_ERR_IF(0 != retval, "failed to start: %s, err: %s", TASK_PRINT_NAME, strerror(retval));

    LOG_INFO("create task: %s", TASK_RECV_NAME);
    retval = rt_task_create(
        &taskRecvDesc,
        TASK_RECV_NAME,
        TASK_RECV_STKSZ,
        TASK_RECV_PRIO,
        TASK_RECV_MODE);

    if (0 != retval) {
        LOG_ERR("failed to create: %s, err: %s", TASK_RECV_NAME, strerror(retval));
        rt_task_delete(
            &taskSendDesc);
        rt_dev_close(
            gDev);

        return (retval);
    }
    LOG_INFO("start task : %s", TASK_RECV_NAME);
    retval = rt_task_start(
        &taskRecvDesc,
        taskRecv,
        NULL);
    LOG_ERR_IF(0 != retval, "failed to start: %s, err: %s", TASK_RECV_NAME, strerror(retval));
}

/*===================================  GLOBAL PRIVATE FUNCTION DEFINITIONS  ==*/
/*====================================  GLOBAL PUBLIC FUNCTION DEFINITIONS  ==*/

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



    printf("\n-----------------------------------------------------------------\n");
    printf("  Message length: %lu\n", DATA_SIZE);
    printf("-----------------------------------------------------------------\n\n");
    pause();

    return (retval);
}

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of main.c
 ******************************************************************************/
