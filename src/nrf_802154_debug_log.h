/* Copyright (c) 2019, Nordic Semiconductor ASA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this
 *      list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *
 *   3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef NRF_802154_DEBUG_LOG_H_
#define NRF_802154_DEBUG_LOG_H_

#include <stdint.h>

#include "nrf_802154_config.h"
#include "nrf_802154_debug_log_codes.h"

/**@def NRF_802154_DEBUG_LOG_BUFFER_LEN
 * @brief Capacity of debug log buffer
 * @note This value must be power of 2
 */
#ifndef NRF_802154_DEBUG_LOG_BUFFER_LEN
#define NRF_802154_DEBUG_LOG_BUFFER_LEN 1024U
#endif

/**@def NRF_802154_DEBUG_LOG_BLOCKS_IRQS
 * @brief Configures if writing to log buffer is performed with interrupts disabled.
 * Setting this macro to 1 has following consequences:
 * - Interrupts are automatically disabled during write to log buffer. This ensures
 *   thread-safety and always coherent log)
 * - Higher priority interrupts may be delayed, so logging has impact on timing
 * Setting this macto to 0 has following consequences:
 * - Interrupts are NOT disabled during write to log buffer. This may lead to missing
 *   logs if higher priority interrupt preempts current write log operation.
 * - Logging does not introduce delay to execution of higher priority interrupts.
 */
#ifndef NRF_802154_DEBUG_LOG_BLOCKS_INTERRUPTS
#define NRF_802154_DEBUG_LOG_BLOCKS_INTERRUPTS 0
#endif

#ifndef NRF_802154_DEBUG_LOG_VERBOSITY_DEFAULT
#define NRF_802154_DEBUG_LOG_VERBOSITY_DEFAULT 1
#endif

/**@def NRF_802154_DEBUG_LOG_VERBOSITY
 * @brief Current verbosity level of generated logs.
 * Define this macro in your C file before inclusion of nrf_802154_debug_log.h to
 * set verbosity level per-module basis.
 */
#ifndef NRF_802154_DEBUG_LOG_VERBOSITY
#define NRF_802154_DEBUG_LOG_VERBOSITY         1
#endif

#if (NRF_802154_DEBUG_LOG_BLOCKS_INTERRUPTS)
#define nrf_802154_debug_log_saved_interrupt_state_variable(var_name)   \
    uint32_t var_name

#define nrf_802154_debug_log_disable_interrupts(var_name)   \
do {                                                        \
    (var_name) = __get_PRIMASK();                           \
    __disable_irq();                                        \
}                                                           \
while(0)

#define nrf_802154_debug_log_restore_interrupts(var_name)   \
do                                                          \
{                                                           \
    __set_PRIMASK(var_name);                                \
}                                                           \
while(0)

#else

#define nrf_802154_debug_log_saved_interrupt_state_variable(var_name)   /* empty macro */
#define nrf_802154_debug_log_disable_interrupts(var_name)               do {} while(0)
#define nrf_802154_debug_log_restore_interrupts(var_name)               do {} while(0)

#endif

/**@brief Checks if provided @c verbosity has value allowing the module to record a log. */
#define nrf_802154_debug_log_verbosity_allows(verbosity) \
    (((verbosity) > 0) && ((verbosity) <= NRF_802154_DEBUG_LOG_VERBOSITY))


#if !defined(CU_TEST) && (ENABLE_DEBUG_LOG)

extern volatile uint32_t nrf_802154_debug_log_buffer[NRF_802154_DEBUG_LOG_BUFFER_LEN];
extern volatile uint32_t nrf_802154_debug_log_ptr;

/**@brief Writes one word into debug log buffer
 */
#define nrf_802154_debug_log_write_raw(value)                                               \
do                                                                                          \
{                                                                                           \
    uint32_t nrf_802154_debug_log_write_raw_value = (value);                                \
                                                                                            \
    nrf_802154_debug_log_saved_interrupt_state_variable(nrf_802154_debug_log_write_raw_sv); \
    nrf_802154_debug_log_disable_interrupts(nrf_802154_debug_log_write_raw_sv);             \
                                                                                            \
    uint32_t nrf_802154_debug_log_write_raw_ptr   = nrf_802154_debug_log_ptr;               \
                                                                                            \
    nrf_802154_debug_log_buffer[nrf_802154_debug_log_write_raw_ptr] = nrf_802154_debug_log_write_raw_value; \
    nrf_802154_debug_log_write_raw_ptr = (nrf_802154_debug_log_write_raw_ptr + 1U) & (NRF_802154_DEBUG_LOG_BUFFER_LEN - 1U); \
    nrf_802154_debug_log_ptr           = nrf_802154_debug_log_write_raw_ptr;                \
                                                                                            \
    nrf_802154_debug_log_restore_interrupts(nrf_802154_debug_log_write_raw_sv);             \
}                                                                                           \
while(0)

#else // !defined(CU_TEST) && (ENABLE_DEBUG_LOG)

#define nrf_802154_debug_log_write_raw(value) do { } while(0)

#endif // !defined(CU_TEST) && (ENABLE_DEBUG_LOG)

/**@brief Records log about entry to a function.
 * @param verbosity     Verbosity level of the module in which log is recorded required to emit log.
 */
#define nrf_802154_debug_log_function_entry(verbosity)                  \
do                                                                      \
{                                                                       \
    if (nrf_802154_debug_log_verbosity_allows(verbosity))               \
    {                                                                   \
        nrf_802154_debug_log_write_raw(                                 \
                ((NRF_802154_DEBUG_LOG_TYPE_FUNCTION_ENTRY) << 28) |    \
                ((NRF_802154_DEBUG_LOG_MODULE_ID) << 22) |              \
                ((uint32_t)((uintptr_t)(__func__)) << 0));              \
    }                                                                   \
}                                                                       \
while(0)

/**@brief Records log about exit from a function.
 * @param verbosity     Verbosity level of the module in which log is recorded required to emit log.
 */
#define nrf_802154_debug_log_function_exit(verbosity)                   \
do                                                                      \
{                                                                       \
    if (nrf_802154_debug_log_verbosity_allows(verbosity))               \
    {                                                                   \
        nrf_802154_debug_log_write_raw(                                 \
                ((NRF_802154_DEBUG_LOG_TYPE_FUNCTION_EXIT) << 28) |     \
                ((NRF_802154_DEBUG_LOG_MODULE_ID) << 22) |              \
                ((uint32_t)((uintptr_t)(__func__)) << 0));              \
    }                                                                   \
}                                                                       \
while(0)

/**@brief Records log about event (with parameter) related to current module.
 * @param verbosity         Verbosity level of the module in which log is recorded required to emit log.
 * @param local_event_id    Event identifier whose meaning is defined within scope of the module
 *                          in which log is recorded. Possible values 0...127
 * @param param_u16         Additional parameter to be logged with event. Meaning
 *                          of the parameter is defined by the module in which
 *                          the log is recorded and event_id.
 */
#define nrf_802154_debug_log_local_event(verbosity, local_event_id, param_u16)  \
do                                                                              \
{                                                                               \
    if (nrf_802154_debug_log_verbosity_allows(verbosity))                       \
    {                                                                           \
        nrf_802154_debug_log_write_raw(                                         \
                ((NRF_802154_DEBUG_LOG_TYPE_LOCAL_EVENT) << 28) |               \
                ((NRF_802154_DEBUG_LOG_MODULE_ID) << 22) |                      \
                ((local_event_id) << 16) |                                      \
                ((uint16_t)(param_u16) << 0));                                  \
    }                                                                           \
}                                                                               \
while(0)

/**@brief Records log about event (with parameter) related to global resource.
 * @param verbosity     Verbosity level of the module in which log is recorded required to emit log.
 * @param event_id      Event identifier whose meaning is defined globally. Possible values 0...127
 * @param param_u16     Additional parameter to be logged with event. Meaning
 *                      of the parameter is defined by value of global_event_id.
 */
#define nrf_802154_debug_log_global_event(verbosity, global_event_id, param_u16)\
do                                                                              \
{                                                                               \
    if (nrf_802154_debug_log_verbosity_allows(verbosity))                       \
    {                                                                           \
        nrf_802154_debug_log_write_raw(                                         \
                ((NRF_802154_DEBUG_LOG_TYPE_GLOBAL_EVENT) << 28) |              \
                ((NRF_802154_DEBUG_LOG_MODULE_ID) << 22) |                      \
                ((global_event_id) << 16) |                                     \
                ((uint16_t)(param_u16) << 0));                                  \
    }                                                                           \
}                                                                               \
while(0)


#endif /* NRF_802154_DEBUG_LOG_H_ */
