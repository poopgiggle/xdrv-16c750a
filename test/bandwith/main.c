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

#include <time.h>
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
#include <native/event.h>
#include <native/sem.h>
#include <native/heap.h>
#include <rtdm/rtdm.h>

/*=========================================================  LOCAL MACRO's  ==*/

#define CFG_DEVICE_DRIVER_NAME          "xuart"
#define CFG_TEST_DATA_SIZE              1024U
#define CFG_TX_PERIOD_NS                MS_TO_NS(100)
#define CFG_NUM_OF_TESTS                0UL
#define CFG_LINES_PER_HEADER            20UL
#define CFG_ALGORITHM                   0

#define APP_VER_MAJOR                   1U
#define APP_VER_MINOR                   0U
#define APP_VER_PATCH                   2U
#define APP_NAME                        "RTDEV_bandwith"
#define APP_DESC                        "Real-Time RTDEV driver latency tester"
#define APP_MAINTAINER                  "Nenad Radulovic <nenad.b.radulovic@gmail.com>"

#define TASK_SEND_NAME                  "RTDEV_test_send"
#define TASK_SEND_STKSZ                 0
#define TASK_SEND_MODE                  0
#define TASK_SEND_PRIO                  T_HIPRIO-2

#define TASK_RECV_NAME                  "RTDEV_test_recv"
#define TASK_RECV_STKSZ                 0
#define TASK_RECV_MODE                  0
#define TASK_RECV_PRIO                  T_HIPRIO-1

#define TASK_PRINT_NAME                 "RTDEV_test_print"
#define TASK_PRINT_STKSZ                0
#define TASK_PRINT_MODE                 0
#define TASK_PRINT_PRIO                 T_LOPRIO

#define TASK_MAN_STKSZ                  0
#define TASK_MAN_MODE                   0
#define TASK_MAN_PRIO                   T_HIPRIO

#define HEAP_SIZE                       (1024ULL*1024ULL*10ULL)
#define RECV_PERIODIC                   1U
#define DEF_RECV_RETRY_CNT              10U
#define DEF_MAX_TEST_DATA_SIZE          HEAP_SIZE

/*-- FIXME: UART protocol defaults should go into driver configuration -------*/
#define BAUD_RATE                       921600ULL
#define STOP_BITS                       1ULL
#define DATA_BITS                       8ULL
#define PARITY_BITS                     0ULL

#define TABLE_HEADER                                                                                \
    "------------------------------------------------------------------------------------------------------\n"   \
    " Nr.    |   RX avg   |   min   |  TURN   |   max   |   avg   |   min   |    TX   |   max   |   avg   |\n"   \
    "------------------------------------------------------------------------------------------------------\n"

#define NS_PER_US                       1000ULL
#define US_PER_MS                       1000ULL
#define MS_PER_S                        1000ULL
#define NS_PER_MS                       (US_PER_MS * NS_PER_US)
#define NS_PER_S                        (MS_PER_S * NS_PER_MS)


#define TRANSMISION_TIME(size)                                                  \
    ((1000000000ULL * (uint64_t)(STOP_BITS + DATA_BITS + PARITY_BITS + 1ULL) * (uint64_t)(size)) / BAUD_RATE)

#define BEFORE_DECIMAL(val)                                                     \
    ((val) / 1000ULL)

#define AFTER_DECIMAL(val)                                                      \
    ((val % 1000ULL) / 100ULL)

#define ABS64(x) ({                                                             \
        int64_t __x = (x);                                                      \
        (__x < 0) ? -__x : __x;                                                 \
    })

#define US_TO_NS(us)                    (NS_PER_US * (us))
#define NS_TO_US(ns)                    ((ns) / NS_PER_US)
#define MS_TO_NS(ms)                    (NS_PER_MS * (ms))
#define NS_TO_MS(ns)                    ((ns) / NS_PER_MS)
#define SEC_TO_NS(sec)                  (NS_PER_S * (sec))

#define COLOR_BLUE                      "\e[1;34m"
#define COLOR_WHITE                     "\e[0m"
#define COLOR_RED                       "\e[1;31m"
#define COLOR_GREEN                     "\e[1;32m"

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
        printf("[ %llu ] " msg "\n", now, ##__VA_ARGS__);                       \
    } while (0)

#define LOG_VAR(var)                                                            \
    printf(APP_NAME " _VAR_ " #var " : %d\n", var )

#define LOG_PVAR(var)                                                           \
    printf(APP_NAME " _PTR_ " #var " : %p\n", var )

/*======================================================  LOCAL DATA TYPES  ==*/

enum dataType {
    DATA_LINEAR         = 0,
    DATA_ZERO           = 1,
    DATA_ONE            = 2,
    DATA_RANDOM         = 3,
    DATA_LAST_ALGO
};

struct timeMeas {
    volatile RTIME      txBegin;
    volatile RTIME      txEnd;
    volatile RTIME      rxBegin;
    volatile RTIME      rxEnd;
};

struct meas {
    uint64_t            cnt;
    RTIME               curr;
    RTIME               avg;
    RTIME               min;
    RTIME               max;
};

struct appConfig {
    size_t              testDataSize;
    RTIME               txPeriod;
    uint32_t            numOfTests;
    uint32_t            linesPerHeader;
    uint32_t            taskStats;
    uint32_t            algo;
    bool                dumpBuff;
};

enum appBootState {
    BOOT_DEV_OPEN,
    BOOT_CREATE_TX,
    BOOT_ALLOC_TX,
    BOOT_CREATE_RX,
    BOOT_ALLOC_RX,
    BOOT_CREATE_RECV,
    BOOT_CREATE_SEND,
    BOOT_CREATE_PRINT,
    BOOT_START_RECV,
    BOOT_START_SEND,
    BOOT_START_PRINT,
    BOOT_CREATE_SEM_PRINT,
    BOOT_CREATE_SEM_SEND,
    BOOT_CREATE_SEM_RECV
};

/*=============================================  LOCAL FUNCTION PROTOTYPES  ==*/

static void taskManager(
    void *              arg);

static void taskSend(
    void *              arg);

static void taskRecv(
    void *              arg);

static void taskPrint(
    void *              arg);

static void measInit(
    struct meas *       meas);

static void measCalc(
    struct meas *       meas,
    RTIME               new);

static void dataGen(
    enum dataType       type,
    uint8_t *           src,
    size_t size);

static uint32_t dataIsValid(
    const void *        src,
    const void *        dst,
    size_t              size);

static int appInit(
    void);

static void appTerm(
    void);

static void appPrintConfig(
    void);

/*=======================================================  LOCAL VARIABLES  ==*/

static RT_SEM           SemPrint;
static RT_SEM           SemSend;
static RT_SEM           SemRecv;
static RT_SEM           SemExit;

static RT_TASK          Taskman;
static RT_TASK          TaskSend;
static RT_TASK          TaskRecv;
static RT_TASK          TaskPrint;

static int              UARTDevice;

static RT_HEAP          HeapTx;
static void *           TxBuff;
static RT_HEAP          HeapRx;
static void *           RxBuff;

static volatile struct timeMeas Time;

static enum appBootState AppBootState;

static struct appConfig AppConfig = {
    .testDataSize       = CFG_TEST_DATA_SIZE,
    .txPeriod           = CFG_TX_PERIOD_NS,
    .numOfTests         = CFG_NUM_OF_TESTS,
    .linesPerHeader     = CFG_LINES_PER_HEADER,
    .taskStats          = 0,
    .algo               = CFG_ALGORITHM,
    .dumpBuff           = false
};

/*======================================================  GLOBAL VARIABLES  ==*/
/*============================================  LOCAL FUNCTION DEFINITIONS  ==*/

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
    rt_sem_p(
        &SemExit,
        TM_INFINITE);
    appPrintConfig();
    appTerm();
}

static void taskSend(
    void *              arg) {

    int                 retval;

    (void)arg;

    dataGen(
        AppConfig.algo,
        TxBuff,
        AppConfig.testDataSize);

    while (true) {
        ssize_t len;
        retval = rt_sem_p(
            &SemSend,
            TM_INFINITE);

        if (0 != retval) {

            if (-EIDRM != retval) {
                LOG_ERR("evt pend, err: %s", strerror(retval));
            }

            return;
        }
        Time.txBegin = rt_timer_read();
        len = rt_dev_write(
            UARTDevice,
            TxBuff,
            AppConfig.testDataSize);
        Time.txEnd = rt_timer_read();

        if ((0 > len) || (len != (ssize_t)AppConfig.testDataSize)) {
            LOG_ERR("failed transmission, err: %s", strerror(len));
        }
    }
}

static void taskRecv(
    void *              arg) {

    int                 retval;
    uint32_t            retry;

    (void)arg;

#if (1u == RECV_PERIODIC)
    retval = rt_task_set_periodic(
        NULL,
        TM_NOW,
        AppConfig.txPeriod);

    if (0 != retval) {
        LOG_ERR("set periodic, err %s", strerror(retval));

        return;
    }
#endif
    retry = DEF_RECV_RETRY_CNT;

    while (true) {
        ssize_t         len;
        uint32_t        differentChars;
        int32_t         invalidNum;

#if (1U == RECV_PERIODIC)
        rt_task_wait_period(
            NULL);
#endif
        retval = rt_sem_v(
            &SemSend);

        if (0 != retval) {

            if (-EIDRM != retval) {
                LOG_ERR("evt signal, err: %s", strerror(retval));
            }

            return;
        }
        Time.rxBegin = rt_timer_read();
        len = rt_dev_read(
            UARTDevice,
            RxBuff,
            AppConfig.testDataSize);
        Time.rxEnd = rt_timer_read();

        if (0 > len) {

            if (-EIDRM == len) {

                return;
            }
            LOG_ERR("failed reception, err: %s", strerror(len));

            continue;
        }
        invalidNum = ((ssize_t)AppConfig.testDataSize - len);

        if (0U != invalidNum) {
            LOG_WARN("received string has invalid number of chars: %d, retry: %u", len, retry);
        }
        differentChars = dataIsValid(
            TxBuff,
            RxBuff,
            AppConfig.testDataSize);

        if (0U != differentChars) {
            LOG_WARN("received string has different chars: %u, retry: %u", differentChars, retry);

            if (true == AppConfig.dumpBuff) {
                size_t cnt;
                uint8_t * src;
                uint8_t * dst;

                src = (uint8_t *)RxBuff;
                dst = (uint8_t *)TxBuff;

                printf("\n\n Dumping invalid buffer content: \n");

                for (cnt = 0; cnt < AppConfig.testDataSize; cnt++) {

                    if (0 == (cnt % 16)) {
                        printf("\n 0x%x\t", cnt);
                    }

                    if (src[cnt] != dst[cnt]) {
                        printf(COLOR_RED);
                        printf(" %4x", src[cnt]);
                        printf(COLOR_WHITE);
                    } else {
                        printf(" %4x", src[cnt]);
                    }
                }
                printf("\n\n done\n");
            }
        }
        memset(
            RxBuff,
            0,
            AppConfig.testDataSize);

        if ((0U == invalidNum) && (0U == differentChars)) {
            retry = DEF_RECV_RETRY_CNT;
        } else {
            retry--;

            if (0U == retry) {

                return;
            } else {

                continue;
            }
        }
        retval = rt_sem_v(
            &SemPrint);

        if (0 != retval) {
            LOG_ERR("evt signal, err: %s", strerror(retval));

            return;
        }
    }
}

static void taskPrint(
    void *              arg) {

    int                 retval;
    uint32_t            cntr;
    struct meas         turn;
    struct meas         tx;
    struct meas         rx;

    (void)arg;
    cntr = 0UL;
    measInit(
        &turn);
    measInit(
        &tx);
    measInit(
        &rx);

    while (true) {
        RTIME           turnCurr;
        RTIME           txCurr;
        RTIME           rxCurr;

        retval = rt_sem_p(
            &SemPrint,
            TM_INFINITE);

        if (0 != retval) {
            if (-EIDRM != retval) {
                LOG_ERR("sem pend, err: %s", strerror(retval));
            }

            return;
        }
        turnCurr = (uint64_t)ABS64((int64_t)Time.rxEnd - (int64_t)Time.txBegin);
        txCurr   = (uint64_t)ABS64((int64_t)Time.txEnd - (int64_t)Time.txBegin);
        rxCurr   = (uint64_t)ABS64((int64_t)Time.rxEnd - (int64_t)Time.rxBegin);
        measCalc(
            &turn,
            turnCurr);
        measCalc(
            &tx,
            txCurr);
        measCalc(
            &rx,
            rxCurr);

        if (0U == cntr) {
            printf("\n-----------------------------------------------------------------\n");
            printf("  Test data size: %u\n", AppConfig.testDataSize);
            printf("-----------------------------------------------------------------\n\n");
        }

        if ((0U != AppConfig.linesPerHeader) && (0U == (cntr % AppConfig.linesPerHeader))) {
            printf(TABLE_HEADER);
        }
        cntr++;
        printf(" %6d | %8llu.%llu | %5llu.%llu | %5llu.%llu | %5llu.%llu | %5llu.%llu | %5llu.%1llu | %5llu.%1llu | %5llu.%1llu | %5llu.%1llu |\n",
            cntr,
            BEFORE_DECIMAL(rx.avg),
            AFTER_DECIMAL(rx.avg),
            BEFORE_DECIMAL(turn.min),
            AFTER_DECIMAL(turn.min),
            BEFORE_DECIMAL(turn.curr),
            AFTER_DECIMAL(turn.curr),
            BEFORE_DECIMAL(turn.max),
            AFTER_DECIMAL(turn.max),
            BEFORE_DECIMAL(turn.avg),
            AFTER_DECIMAL(turn.avg),
            BEFORE_DECIMAL(tx.min),
            AFTER_DECIMAL(tx.min),
            BEFORE_DECIMAL(tx.curr),
            AFTER_DECIMAL(tx.curr),
            BEFORE_DECIMAL(tx.max),
            AFTER_DECIMAL(tx.max),
            BEFORE_DECIMAL(tx.avg),
            AFTER_DECIMAL(tx.avg));

        if (0 != AppConfig.taskStats) {

            if (0 == (cntr % AppConfig.taskStats)) {
                RT_TASK_INFO recv;
                RT_TASK_INFO send;

                rt_task_inquire(
                    &TaskRecv,
                    &recv);
                rt_task_inquire(
                    &TaskSend,
                    &send);
                printf(" task %s: modesw: %d, ctxsw: %d\n",
                    recv.name,
                    recv.modeswitches,
                    recv.ctxswitches);
                printf(" task %s: modesw: %d, ctxsw: %d\n",
                    send.name,
                    send.modeswitches,
                    send.ctxswitches);
            }
        }

        if (0U != AppConfig.numOfTests) {

            if (AppConfig.numOfTests == cntr) {
                kill(
                    getpid(),
                    SIGTERM);
            }
        }
    }
}

static void measInit(
    struct meas *       meas) {
    meas->cnt  = 0UL;
    meas->curr = 0U;
    meas->avg  = 0U;
    meas->min  = (RTIME)-1;
    meas->max  = 0U;
}

static void measCalc(
    struct meas *       meas,
    RTIME               new) {

    int64_t             tmp;

    meas->curr = new;
    meas->cnt++;

    tmp = (int64_t)meas->avg;
    tmp += ((int64_t)meas->curr - (int64_t)meas->avg) / (int64_t)meas->cnt;

    if (0 < tmp) {
        meas->avg = (uint64_t)tmp;
    }

    if (meas->min > meas->curr) {
        meas->min = meas->curr;
    }

    if (meas->max < meas->curr) {
        meas->max = meas->curr;
    }
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
            uint32_t    i;

            for (i = 0U; i < size; i++) {
                src[i] = (uint8_t)(0xFFU & i);
            }
            break;
        }
        case DATA_ZERO :
            memset(
                src,
                0x00U,
                size);
            break;
        case DATA_ONE :
            memset(
                src,
                0xFFU,
                size);
            break;
        case DATA_RANDOM : {
            uint32_t    i;

            srand((unsigned int)time(NULL));

            for (i = 0U; i < size; i++) {
                src[i] = (uint8_t)(rand() % 255);
            }
            break;
        }
        default : {

        }
    }
}

static uint32_t dataIsValid(
    const void *        src,
    const void *        dst,
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

static int appInit(
    void) {

    int             retval;

    /*-- BOOT_DEV_OPEN -------------------------------------------------------*/
    AppBootState = BOOT_DEV_OPEN;
    LOG_INFO("open device: %s", CFG_DEVICE_DRIVER_NAME);
    UARTDevice = rt_dev_open(
        CFG_DEVICE_DRIVER_NAME,
        0);

    if (0 > UARTDevice) {
        LOG_ERR("failed to open device, err: %s)", strerror(UARTDevice));

        return (UARTDevice);
    }

    /*-- BOOT_CREATE_TX ------------------------------------------------------*/
    AppBootState = BOOT_CREATE_TX;
    LOG_INFO("reserving memory");
    retval = rt_heap_create(
        &HeapTx,
        "TXbuff_",
        HEAP_SIZE,
        H_SINGLE);

    if (0 != retval) {
        LOG_ERR("failed to create TX buffer, err: %s)", strerror(retval));

        return (retval);
    }

    /*-- BOOT_ALLOC_TX -------------------------------------------------------*/
    AppBootState = BOOT_ALLOC_TX;
    retval = rt_heap_alloc(
        &HeapTx,
        HEAP_SIZE,
        TM_INFINITE,
        &TxBuff);

    if (0 != retval) {
        LOG_ERR("unable to allocate TX buffer, err: %s", strerror(retval));

        return (retval);
    }

    /*-- BOOT_CREATE_RX ------------------------------------------------------*/
    AppBootState = BOOT_CREATE_RX;
    retval = rt_heap_create(
        &HeapRx,
        "RXbuff_",
        HEAP_SIZE,
        H_SINGLE);

    if (0 != retval) {
        LOG_ERR("failed to create RX buffer, err: %s", strerror(retval));

        return (retval);
    }

    AppBootState = BOOT_ALLOC_RX;
    retval = rt_heap_alloc(
        &HeapRx,
        HEAP_SIZE,
        TM_INFINITE,
        &RxBuff);

    if (0 != retval) {
        LOG_ERR("unable to allocate buffer, err: %s)", strerror(retval));

        return (retval);
    }

    AppBootState = BOOT_CREATE_SEM_PRINT;
    retval = rt_sem_create(
        &SemPrint,
        "print",
        0,
        S_PRIO);

    if (0 != retval) {
        LOG_ERR("sem create, err: %s", strerror(retval));

        return (retval);
    }

    AppBootState = BOOT_CREATE_SEM_SEND;
    retval = rt_sem_create(
        &SemSend,
        "send",
        0,
        S_PULSE);

    if (0 != retval) {
        LOG_ERR("sem create, err: %s", strerror(retval));

        return (retval);
    }
    retval = rt_sem_create(
        &SemRecv,
        "recv",
        0,
        S_PULSE);

    if (0 != retval) {
        LOG_ERR("sem create, err: %s", strerror(retval));

        return (retval);
    }

    AppBootState = BOOT_CREATE_PRINT;
    LOG_INFO("create task: %s", TASK_PRINT_NAME);
    retval = rt_task_create(
        &TaskPrint,
        TASK_PRINT_NAME,
        TASK_PRINT_STKSZ,
        TASK_PRINT_PRIO,
        TASK_PRINT_MODE);

    if (0 != retval) {
        LOG_ERR("failed to create: %s, err: %s", TASK_PRINT_NAME, strerror(retval));

        return (retval);
    }

    AppBootState = BOOT_START_PRINT;
    LOG_INFO("start task : %s", TASK_PRINT_NAME);
    retval = rt_task_start(
        &TaskPrint,
        taskPrint,
        NULL);

    if (0 != retval) {
        LOG_ERR("failed to start: %s, err: %s", TASK_PRINT_NAME, strerror(retval));

        return (retval);
    }

    AppBootState = BOOT_CREATE_RECV;
    LOG_INFO("create task: %s", TASK_RECV_NAME);
    retval = rt_task_create(
        &TaskRecv,
        TASK_RECV_NAME,
        TASK_RECV_STKSZ,
        TASK_RECV_PRIO,
        TASK_RECV_MODE);

    if (0 != retval) {
        LOG_ERR("failed to create: %s, err: %s", TASK_RECV_NAME, strerror(retval));

        return (retval);
    }

    AppBootState = BOOT_START_RECV;
    LOG_INFO("start task : %s", TASK_RECV_NAME);
    retval = rt_task_start(
        &TaskRecv,
        taskRecv,
        NULL);

    if (0 != retval) {
        LOG_ERR("failed to start: %s, err: %s", TASK_RECV_NAME, strerror(retval));

        return (retval);
    }

    AppBootState = BOOT_CREATE_SEND;
    LOG_INFO("create task: %s", TASK_SEND_NAME);
    retval = rt_task_create(
        &TaskSend,
        TASK_SEND_NAME,
        TASK_SEND_STKSZ,
        TASK_SEND_PRIO,
        TASK_SEND_MODE);

    if (0 != retval) {
        LOG_ERR("failed to create: %s, err: %s)", TASK_SEND_NAME, strerror(retval));

        return (retval);
    }

    AppBootState = BOOT_START_SEND;
    LOG_INFO("start task : %s", TASK_SEND_NAME);
    retval = rt_task_start(
        &TaskSend,
        taskSend,
        NULL);

    if (0 != retval) {
        LOG_ERR("failed to start: %s, err: %s", TASK_SEND_NAME, strerror(retval));

        return (retval);
    }

    return (0);
}

static void appTerm(
    void) {

    LOG_INFO("exiting application");
    /*
     * TODO: zavrsiti
     */
    rt_sem_delete(
        &SemPrint);
    rt_sem_delete(
        &SemSend);
    rt_sem_delete(
        &SemRecv);
    rt_dev_close(
        UARTDevice);
    switch (AppBootState) {
        case BOOT_START_RECV : {
            rt_task_delete(
                &TaskRecv);
        }
        case BOOT_CREATE_RECV :
        case BOOT_START_PRINT : {
            rt_task_delete(
                &TaskPrint);
        }
        case BOOT_CREATE_PRINT :
        case BOOT_START_SEND : {
            rt_task_delete(
                &TaskSend);
        }
        case BOOT_CREATE_SEND : {

        }
        case BOOT_CREATE_SEM_PRINT : {
            rt_heap_free(
                &HeapRx,
                RxBuff);
        }
        case BOOT_ALLOC_RX : {
            rt_heap_delete(
                &HeapRx);
        }
        case BOOT_CREATE_RX : {
            rt_heap_free(
                &HeapTx,
                TxBuff);
        }
        case BOOT_ALLOC_TX : {
            rt_heap_delete(
                &HeapTx);
        }
        case BOOT_CREATE_TX : {


        }
        case BOOT_DEV_OPEN : {

            break;
        }

        default : {

        }
    }
}

static void appPrintConfig(
    void) {

    printf("------------------------\n");
    printf("### Using configuration \n");
    printf(" - test data size      : %u bytes\n", AppConfig.testDataSize);
    printf(" - transmission period : %llu ms\n", NS_TO_MS(AppConfig.txPeriod));
    printf(" - lines per header    : %u\n", AppConfig.linesPerHeader);
    printf(" - num of tests        : %u\n", AppConfig.numOfTests);
    printf(" - tasks statistics    : %u\n", AppConfig.taskStats);
    printf(" - algorithm           : %u\n\n", AppConfig.algo);
}

/*===================================  GLOBAL PRIVATE FUNCTION DEFINITIONS  ==*/
/*====================================  GLOBAL PUBLIC FUNCTION DEFINITIONS  ==*/

int main(
    int                 argc,
    char **             argv) {

    int                 retval;
    int                 cmd;
    int                 sig;
    sigset_t            sigset;

    printf("\n" APP_DESC "\n");

    while (EOF != (cmd = getopt(argc, argv, "a:s:p:l:n:t:vd"))) {

        switch (cmd) {
            case 'a' :
                AppConfig.algo = (uint32_t)atoi(optarg);

                if (DATA_LAST_ALGO <= AppConfig.algo) {
                    printf(" Invalid algorithm number %d\n", AppConfig.algo);
                    printf(" Valid range: 0 - %d\n", DATA_LAST_ALGO - 1);
                    exit(2);
                }
                break;
            case 'd' :
                AppConfig.dumpBuff = true;
                break;
            case 's' :
                AppConfig.testDataSize = (size_t)atoi(optarg);

                if ((0U == AppConfig.testDataSize) || (DEF_MAX_TEST_DATA_SIZE < AppConfig.testDataSize)) {
                    printf(" Invalid -s option value: %u\n", AppConfig.testDataSize);
                    exit(2);
                }
                break;
            case 'p' :
                AppConfig.txPeriod = MS_TO_NS((RTIME)atoi(optarg));

                if ((0U == AppConfig.txPeriod) || (TRANSMISION_TIME(DEF_MAX_TEST_DATA_SIZE) < AppConfig.txPeriod)) {
                    printf(" Invalid -p option value: %llu\n", AppConfig.txPeriod);
                    exit(2);
                }
                break;
            case 'l' :
                AppConfig.linesPerHeader = (uint32_t)atoi(optarg);
                break;
            case 'n' :
                AppConfig.numOfTests = (uint32_t)atoi(optarg);

                if (0U == AppConfig.numOfTests) {
                    printf(" Invalid -n option value: %d\n", AppConfig.numOfTests);
                    exit(2);
                }
                break;
            case 't' :
                AppConfig.taskStats = (uint32_t)atoi(optarg);
                break;
            case 'v' :
                printf(" Version: %u.%u.%u\n", APP_VER_MAJOR, APP_VER_MINOR, APP_VER_PATCH);
                printf(" Maintainer: %s\n", APP_MAINTAINER);
                exit(0);
            default :
                fprintf(stderr,
                    "usage: latency [options]                                                   \n"
                    "                                                                           \n"
                    "  -s <size>                - default %u bytes, [1-%llu] test data size     \n"
                    "  -p <period_ms>           - default %llu ms, [1-10000] transmission period\n"
                    "  -l <lines_per_header>    - default %u, 0 to supress headers              \n"
                    "  -n <num_of_tests>        - default infinite                              \n"
                    "  -t <lines_per_info>      - default %u, 0 to supress task statistics      \n"
                    "  -a <algorithm>           - default lin, available const and lin          \n"
                    "  -d                       - dump received buffer in case it is not valid  \n"
                    "  -v                       - show version information                      \n"
                    "                                                                           \n",
                    AppConfig.testDataSize,
                    DEF_MAX_TEST_DATA_SIZE,
                    NS_TO_MS(AppConfig.txPeriod),
                    AppConfig.linesPerHeader,
                    AppConfig.taskStats);
                exit(2);
        }
    }
    appPrintConfig();
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
    retval = rt_sem_create(
        &SemExit,
        "exit",
        0,
        S_FIFO);

    if (0 != retval) {
        LOG_ERR("sem create, err: %s", strerror(retval));

        return (retval);
    }
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
    rt_sem_v(
        &SemExit);
    rt_sem_delete(
        &SemExit);

    return (retval);
}

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of main.c
 ******************************************************************************/
