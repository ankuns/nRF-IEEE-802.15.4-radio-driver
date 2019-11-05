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

#ifndef DEBUG_VERBOSITY
#define DEBUG_VERBOSITY 1
#endif

#ifndef CU_TEST
#if ENABLE_DEBUG_LOG
extern volatile uint32_t nrf_802154_debug_log_buffer[NRF_802154_DEBUG_LOG_BUFFER_LEN];
extern volatile uint32_t nrf_802154_debug_log_ptr;

#define nrf_802154_log(EVENT_CODE, EVENT_ARG)                                                   \
    do                                                                                          \
    {                                                                                           \
        uint32_t ptr = nrf_802154_debug_log_ptr;                                                \
                                                                                                \
        nrf_802154_debug_log_buffer[ptr] = ((EVENT_CODE) | ((EVENT_ARG) << 16));                \
        nrf_802154_debug_log_ptr         = (ptr + 1U) & (NRF_802154_DEBUG_LOG_BUFFER_LEN - 1U); \
    }                                                                                           \
    while (0)

#else // ENABLE_DEBUG_LOG

#define nrf_802154_log(EVENT_CODE, EVENT_ARG) (void)(EVENT_ARG)

#endif // ENABLE_DEBUG_LOG

#define nrf_802154_log_entry(function, verbosity)                     \
    do                                                                \
    {                                                                 \
        if (verbosity <= DEBUG_VERBOSITY)                             \
        {                                                             \
            nrf_802154_log(EVENT_TRACE_ENTER, FUNCTION_ ## function); \
        }                                                             \
    }                                                                 \
    while (0)

#define nrf_802154_log_exit(function, verbosity)                     \
    do                                                               \
    {                                                                \
        if (verbosity <= DEBUG_VERBOSITY)                            \
        {                                                            \
            nrf_802154_log(EVENT_TRACE_EXIT, FUNCTION_ ## function); \
        }                                                            \
    }                                                                \
    while (0)

#else // CU_TEST

#define nrf_802154_log(EVENT_CODE, EVENT_ARG)

#endif


#endif /* NRF_802154_DEBUG_LOG_H_ */
