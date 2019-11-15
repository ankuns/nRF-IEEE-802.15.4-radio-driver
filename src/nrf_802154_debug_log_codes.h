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

#ifndef NRF_802154_DEBUG_LOG_CODES_H_
#define NRF_802154_DEBUG_LOG_CODES_H_

#define NRF_802154_DEBUG_LOG_EVENT_FUNCTION_ENTRY   1U
#define NRF_802154_DEBUG_LOG_EVENT_FUNCTION_EXIT    2U

// Debug log module identifiers.
// Note: Certain value e.g. NRF_802154_DEBUG_LOG_MODULE_ID_RAAL may be shared
// between alternative implementations of module
// (e.g. raal_single_phy/raal_softdevice/raal_simulator)
// In other cases they must be unique.
#define NRF_802154_DEBUG_LOG_MODULE_ID_APPLICATION      1U
#define NRF_802154_DEBUG_LOG_MODULE_ID_CORE             2U
#define NRF_802154_DEBUG_LOG_MODULE_ID_RSCH             3U
#define NRF_802154_DEBUG_LOG_MODULE_ID_CRITICAL_SECTION 4U
#define NRF_802154_DEBUG_LOG_MODULE_ID_TIMER_COORD      5U
#define NRF_802154_DEBUG_LOG_MODULE_ID_TRX              6U
#define NRF_802154_DEBUG_LOG_MODULE_ID_TIMER_SCHED      7U
#define NRF_802154_DEBUG_LOG_MODULE_ID_CSMACA           8U
#define NRF_802154_DEBUG_LOG_MODULE_ID_DELAYED_TRX      9U
#define NRF_802154_DEBUG_LOG_MODULE_ID_ACK_TIMEOUT      10U
#define NRF_802154_DEBUG_LOG_MODULE_ID_RAAL             11U


#define EVENT_TRACE_ENTER           0x0001UL
#define EVENT_TRACE_EXIT            0x0002UL

#define EVENT_SET_STATE             0x0005UL
#define EVENT_RADIO_RESET           0x0006UL

#define EVENT_TIMESLOT_REQUEST        0x0007UL
#define EVENT_TIMESLOT_REQUEST_RESULT 0x0008UL

#define FUNCTION_AUTO_ACK_ABORT     0x0201UL
#define FUNCTION_TIMESLOT_STARTED   0x0202UL
#define FUNCTION_TIMESLOT_ENDED     0x0203UL
#define FUNCTION_CRIT_SECT_ENTER    0x0204UL
#define FUNCTION_CRIT_SECT_EXIT     0x0205UL

/* Reserved for RAAL: 0x0300 - 0x047F */
#define FUNCTION_RAAL_CRIT_SECT_ENTER          0x0301UL
#define FUNCTION_RAAL_CRIT_SECT_EXIT           0x0302UL
#define FUNCTION_RAAL_CONTINUOUS_ENTER         0x0303UL
#define FUNCTION_RAAL_CONTINUOUS_EXIT          0x0304UL

#define FUNCTION_RAAL_SIG_HANDLER              0x0400UL
#define FUNCTION_RAAL_SIG_EVENT_START          0x0401UL
#define FUNCTION_RAAL_SIG_EVENT_MARGIN         0x0402UL
#define FUNCTION_RAAL_SIG_EVENT_EXTEND         0x0403UL
#define FUNCTION_RAAL_SIG_EVENT_ENDED          0x0404UL
#define FUNCTION_RAAL_SIG_EVENT_RADIO          0x0405UL
#define FUNCTION_RAAL_SIG_EVENT_EXTEND_SUCCESS 0x0406UL
#define FUNCTION_RAAL_SIG_EVENT_EXTEND_FAIL    0x0407UL
#define FUNCTION_RAAL_EVT_BLOCKED              0x0408UL
#define FUNCTION_RAAL_EVT_SESSION_IDLE         0x0409UL
#define FUNCTION_RAAL_EVT_HFCLK_READY          0x040AUL
#define FUNCTION_RAAL_SIG_EVENT_MARGIN_MOVE    0x040BUL




#endif /* NRF_802154_DEBUG_LOG_CODES_H_ */
