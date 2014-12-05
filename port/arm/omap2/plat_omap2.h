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
 * @brief       Port for OMAP2 platform
 *********************************************************************//** @{ */

#if !defined(PLAT_OMAP2_H_)
#define PLAT_OMAP2_H_

/*=========================================================  INCLUDE FILES  ==*/

#include <linux/platform_data/edma.h>

#include "drv/x-16c750_lld.h"

/*===============================================================  MACRO's  ==*/

#if defined(CONFIG_SOC_AM33XX)

/**@brief       Available UARTs on AM335x
 */
 /*
  *       | UART #    | IOMEM                     | IRQ
  */
# define UART_DATA_TABLE(entry)                                                 \
    entry(  UARTO,      0x44e09000ul,               72)                         \
    entry(  UART1,      0x48022000ul,               73)                         \
    entry(  UART2,      0x48024000ul,               74)                         \
    entry(  UART3,      0x481a6000ul,               44)                         \
    entry(  UART4,      0x481a8000ul,               45)                         \
    entry(  UART5,      0x481aa000ul,               46)

/*
 *        | Baud-rate | UART mode                 | Divisor
 */
# define BAUD_RATE_CFG_TABLE(entry)                                             \
    entry(  9600,       LLD_MODE_UART16,            313)                        \
    entry(  19200,      LLD_MODE_UART16,            156)                        \
    entry(  38400,      LLD_MODE_UART16,            78)                         \
    entry(  115200,     LLD_MODE_UART16,            26)                         \
    entry(  921600,     LLD_MODE_UART13,            4)

# define EDMA_TPCC_BASE                 0x49000000u
# define EDMA_TPCC_SIZE                 0x000fffffu
# define EDMA_TPTC0_BASE                0x49800000u
# define EDMA_TPTC1_BASE                0x49900000u
# define EDMA_TPTC2_BASE                0x49a00000u
# define EDMA_COMP_IRQ                  12u
# define EDMA_MPERR_IRQ                 13u
# define EDMA_ERR_IRQ                   14u

# if (1 == CFG_DMA_MODE)
#  define EDMA_CHN_TX                   EDMA_CHANNEL_ANY
# elif (2 == CFG_DMA_MODE)
#  define EDMA_CHN_TX                   (7u + 63u)
# endif

# if (1 == CFG_DMA_MODE)
#  define EDMA_CHN_RX                   EDMA_CHANNEL_ANY
# elif (2 == CFG_DMA_MODE)
#  define EDMA_CHN_RX                   (8u + 63u)
# endif

# define EDMA_SH_BASE                   0x2000u
# define EDMA_SH_INCR                   0x200u                                  /* Increment to next shadow region                          */
#endif

/*------------------------------------------------------  C++ extern begin  --*/
#ifdef __cplusplus
extern "C" {
#endif

/*============================================================  DATA TYPES  ==*/
/*======================================================  GLOBAL VARIABLES  ==*/
/*===================================================  FUNCTION PROTOTYPES  ==*/
/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of plat_omap2.h
 ******************************************************************************/
#endif /* PLAT_OMAP2_H_ */
