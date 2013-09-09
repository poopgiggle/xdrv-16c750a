/*
 * This file is part of eSolid-Kernel
 *
 * Copyright (C) 2011, 2012 - Nenad Radulovic
 *
 * eSolid-Kernel is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * eSolid-Kernel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with eSolid-Kernel; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * web site:    http://blueskynet.dyndns-server.com
 * e-mail  :    blueskyniss@gmail.com
 *//***********************************************************************//**
 * @file
 * @author      Nenad Radulovic
 * @brief       Debug basic functionality.
 * @addtogroup  dbg_intf
 *********************************************************************//** @{ */

#if !defined(DBG_H_)
#define DBG_H_

/*=========================================================  INCLUDE FILES  ==*/

#include "port/compiler.h"
#include "eds/dbg_cfg.h"

/*===============================================================  MACRO's  ==*/

#define DECL_MODULE_INFO(modName, modDesc, modAuthor)                           \
    const PORT_C_ROM struct modInfo gModInfo_ = {                               \
        .name = modName,                                                        \
        .desc = modDesc,                                                        \
        .auth = modAuthor,                                                      \
        .file = PORT_C_FILE                                                     \
    }

/*------------------------------------------------------------------------*//**
 * @name        Error checking
 * @brief       These macros are enabled/disabled using the option
 *              @ref CFG_DBG_ENABLE.
 * @{ *//*--------------------------------------------------------------------*/

#if (1U == CFG_DBG_ENABLE)
/**@brief       Generic assert macro.
 * @param       msg
 *              Enumerator enum esDbgMsg: enumerated debug message.
 * @param       expr
 *              Condition expression which must be TRUE.
 */
# define ES_DBG_ASSERT(msg, expr)                                               \
    do {                                                                        \
        if (!(expr)) {                                                          \
            const struct cObj thisObj = {                                       \
                .mod = &gModInfo_,                                              \
                .fn = PORT_C_FUNC,                                              \
                .line = PORT_C_LINE                                             \
            };                                                                  \
            dbgAssert(&thisObj, #expr, msg);                                    \
        }                                                                       \
    } while (0U)

/**@brief       Assert macro that will always execute (no conditional).
 * @param       msg
 *              Enumerator enum esDbgMsg: enumerated kernel message.
 * @param       text
 *              Pointer to string: a text which will be printed when this assert
 *              macro is executed.
 */
# define ES_DBG_ASSERT_ALWAYS(msg, text)                                        \
    do {                                                                        \
        dbgAssert(PORT_C_FUNC, text, msg);                                      \
    } while (0U)

#else
# define ES_DBG_ASSERT(msg, expr)                                               \
    (void)0

# define ES_DBG_ASSERT_ALWAYS(msg, text)                                        \
    (void)0
#endif

/**@} *//*----------------------------------------------------------------*//**
 * @name        Internal checking
 * @brief       These macros are enabled/disabled using the option
 *              @ref CFG_DBG_INTERNAL_CHECK.
 * @{ *//*--------------------------------------------------------------------*/

#if (1U == CFG_DBG_INTERNAL_CHECK)

/**@brief       Assert macro used for internal execution checking
 * @param       msg
 *              Enumerator enum esDbgMsg: enumerated debug message.
 * @param       expr
 *              Expression which must be satisfied
 */
# define ES_DBG_INTERNAL(msg, expr)                                             \
    ES_DBG_ASSERT(msg, expr)
#else
# define ES_DBG_INTERNAL(msg, expr)                                             \
    (void)0
#endif

/**@} *//*----------------------------------------------------------------*//**
 * @name        API contract validation
 * @brief       These macros are enabled/disabled using the option
 *              @ref CFG_DBG_API_VALIDATION.
 * @{ *//*--------------------------------------------------------------------*/

#if (1U == CFG_DBG_API_VALIDATION) || defined(__DOXYGEN__)

/**@brief       Execute code to fulfill the contract
 * @param       expr
 *              Expression to be executed only if contracts need to be validated.
 */
# define ES_DBG_API_OBLIGATION(expr)                                            \
    expr

/**@brief       Make sure the caller has fulfilled all contract preconditions
 * @param       msg
 *              Enumerator enum esDbgMsg: enumerated debug message.
 * @param       expr
 *              Expression which must be satisfied
 */
# define ES_DBG_API_REQUIRE(msg, expr)                                          \
    ES_DBG_ASSERT(msg, expr)

/**@brief       Make sure the callee has fulfilled all contract postconditions
 * @param       msg
 *              Enumerator enum esDbgMsg: enumerated debug message.
 * @param       expr
 *              Expression which must be satisfied
 */
# define ES_DBG_API_ENSURE(msg, expr)                                           \
    ES_DBG_ASSERT(msg, expr)

#else
# define ES_DBG_API_OBLIGATION(expr)                                            \
    (void)0

# define ES_DBG_API_REQUIRE(msg, expr)                                          \
    (void)0

# define ES_DBG_API_ENSURE(msg, expr)                                           \
    (void)0
#endif

/**@} *//*----------------------------------------------  C++ extern begin  --*/
#ifdef __cplusplus
extern "C" {
#endif

/*============================================================  DATA TYPES  ==*/

/**@brief       Debug messages
 */
enum esDbgMsg {
    ES_DBG_OUT_OF_RANGE,                                                        /**< @brief Value is out of valid range.                    */
    ES_DBG_OBJECT_NOT_VALID,                                                    /**< @brief Object is not valid.                            */
    ES_DBG_POINTER_NULL,                                                        /**< @brief Pointer has NULL value.                         */
    ES_DBG_USAGE_FAILURE,                                                       /**< @brief Object usage failure.                           */
    ES_DBG_NOT_ENOUGH_MEM,                                                      /**< @brief Not enough memory available.                    */
    ES_DBG_UNKNOWN_ERROR = 0xFFU                                                /**< @brief Unknown error.                                  */
};

struct modInfo {
    const char *    name;
    const char *    desc;
    const char *    auth;
    const char *    file;
};

struct cObj {
    const struct modInfo * mod;
    const char *    fn;
    uint32_t        line;
};

/*======================================================  GLOBAL VARIABLES  ==*/
/*===================================================  FUNCTION PROTOTYPES  ==*/


/**@} *//*----------------------------------------------------------------*//**
 * @name        Error checking
 * @brief       Some basic infrastructure for error checking
 * @details     For more datails see @ref errors.
 * @{ *//*--------------------------------------------------------------------*/

/**@brief       An assertion has failed
 * @param       fnName
 *              Function name: is pointer to the function name string where the
 *              assertion has failed. Macro will automatically fill in the
 *              function name.
 * @param       expr
 *              Expression: is pointer to the string containing the expression
 *              that failed to evaluate to `TRUE`.
 * @param       msg
 *              Message: is enum esDbgMsg containing some information about the
 *              error.
 * @pre         1) `NULL != fnName`
 * @pre         2) `NULL != expr`
 * @note        1) This function is called only if @ref CFG_DBG_API_VALIDATION
 *              is active.
 * @details     Function will just print the information which was given by the
 *              macros.
 * @notapi
 */
PORT_C_NORETURN void dbgAssert(
    const struct cObj * cObj,
    const char *    expr,
    enum esDbgMsg   msg);

/**@} *//*----------------------------------------------------------------*//**
 * @name        Debug hook functions
 * @note        1) The definition of this functions must be written by the user.
 * @{ *//*--------------------------------------------------------------------*/

/**@brief       An assertion has failed. This function should inform the user
 *              about failed assertion.
 * @param       fnName
 *              Function name: is pointer to the function name string where the
 *              assertion has failed. Macro will automatically fill in the
 *              function name.
 * @param       expr
 *              Expression: is pointer to the string containing the expression
 *              that failed to evaluate to `TRUE`.
 * @param       msg
 *              Message: is a pointer to the string containing some information
 *              about the error.
 * @param       msgNum
 *              Message number: is enumerator esDbgMsg.
 * @pre         1) `NULL != fnName`
 * @pre         2) `NULL != expr`
 * @pre         3) `NULL != msg`
 * @note        1) This function is called only if @ref CFG_DBG_ENABLE is active.
 * @note        2) The function is called with interrupts disabled.
 * @details     Function will just print the information which was given by the
 *              macros.
 */
extern void userAssert(
    const char *    modName,
    const char *    modDesc,
    const char *    modAuthor,
    const char *    modFile,
    const char *    fn,
    const char *    expr,
    const char *    msgTxt,
    uint32_t        msgNum);

/** @} *//*-----------------------------------------------  C++ extern end  --*/
#ifdef __cplusplus
}
#endif

/*================================*//** @cond *//*==  CONFIGURATION ERRORS  ==*/
/** @endcond *//** @} *//******************************************************
 * END of dbg.h
 ******************************************************************************/
#endif /* DBG_H_ */
