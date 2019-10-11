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

/**
 * @brief Module that contains radio hardware-related functions
 *        of the nRF IEEE 802.15.4 radio driver.
 *
 * @details
 */

#ifndef NRF_802154_TRX_H_
#define NRF_802154_TRX_H_

#include <stdbool.h>
#include <stdint.h>

#include "nrf_802154_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    TRX_STATE_DISABLED = 0,
    TRX_STATE_IDLE,
    TRX_STATE_GOING_IDLE,
    TRX_STATE_RXFRAME,

    /* PPIS disabled deconfigured
     * RADIO is DISABLED, RXDISABLE
     * RADIO shorts are 0
     * TIMER is running
     * FEM is going to powered or is powered depending if RADIO reached DISABLED
     */
    TRX_STATE_RXFRAME_FINISHED,

    TRX_STATE_RXACK,
    TRX_STATE_TXFRAME,
    TRX_STATE_TXACK,
    TRX_STATE_STANDALONE_CCA,
    TRX_STATE_CONTINUOUS_CARRIER,
    TRX_STATE_ENERGY_DETECTION,

    /* PPIS disabled deconfigured
     * RADIO is DISABLED, TXDISABLE, RXDISABLE
     * RADIO shorts are 0
     * TIMER is stopped
     * FEM is going to powered or is powered depending if RADIO reached DISABLED
     */
    TRX_STATE_FINISHED
} trx_state_t;

/**@brief Initializes trx module.
 *
 * This function must be called exactly once, before any other API call.
 */
void nrf_802154_trx_init(void);

void nrf_802154_trx_enable(void);
void nrf_802154_trx_disable(void);

/**@brief Sets radio channel to use.
 *
 * @param[in] channel   Channel number to set (11-26).
 */
void nrf_802154_trx_channel_set(uint8_t channel);

void nrf_802154_trx_cca_configuration_update(void);

/**@brief Puts the trx module into receive frame (non-ack) mode.
 *
 * The frame will be received into buffer set by @ref nrf_802154_trx_receive_buffer_set.
 *
 * When NRF_802154_DISABLE_BCC_MATCHING == 0
 * - during receive @ref nrf_802154_trx_receive_on_bcmatch handler (from ISR) is called when
 *   bcc octets is received.
 * - when a frame is received with correct crc, @ref nrf_802154_trx_receive_received is called (from ISR)
 * - when a frame is received with incorrect crc, @ref nrf_802154_trx_receive_crcerror is called (from ISR)
 *
 * When NRF_802154_DISABLE_BCC_MATCHING != 0
 * - during receive no handlers (no ISRs) are called
 * - after the frame is received with correct crc, @ref nrf_802154_trx_receive_received is called (from ISR)
 * - after the frame is received with incorrect crc:
 *     - when NRF_802154_NOTIFY_CRCERROR == 0:
 *         - no handler is called
 *         - the hardware restarts receive automatically
 *     - when NRF_802154_NOTIFY_CRCERROR != 0:
 *         - @ref nrf_802154_trx_receive_crcerror is called
 *         - the hardware restarts receive automatically
 *
 * When in @ref nrf_802154_trx_receive_received TIMER is running allowing sending respone (e.g. ACK frame)
 * in time regime by a call to nrf_802154_trx_transmit_after_frame_received.
 *
 * @param[in] bcc   Number of received bytes after which @ref nrf_802154_trx_receive_on_bcmatch will be called.
 *                  This must not be zero if @ref NRF_802154_DISABLE_BCC_MATCHING == 0.
 *                  When @ref NRF_802154_DISABLE_BCC_MATCHING != 0, this value must be zero.
 *
 */
void nrf_802154_trx_receive_frame(uint8_t bcc);

/**@brief Puts the trx module into receive ACK mode.
 *
 * The ack frame will be received into buffer set by @ref nrf_802154_trx_receive_buffer_set
 *
 * During receive of a frame:
 * - @ref nrf_802154_trx_receive_on_framestart is called when a frame has just started being received.
 * - when a frame is received with correct crc, @ref nrf_802154_trx_receive_received is called (from ISR)
 * - when a frame is received with incorrect crc, @ref nrf_802154_trx_receive_crcerror is called (from ISR)
 */
void nrf_802154_trx_receive_ack(void);

/**@brief Starts RSSI measurement.
 *
 * @note This function succeeds when TRX module is in receive frame state only.
 *
 * @retval true     When RSSI measurement has been started.
 * @retval false    When TRX state didn't allow start of RSSI measurement.
 */
bool nrf_802154_trx_rssi_measure(void);

/**@brief Checks if RSSI measurement is currently started.
 *
 * @retval true     When RSSI measurement is currently started. In this case user can
 *                  check if RSSI sample is already available by call to @ref nrf_802154_trx_rssi_sample_is_available.
 * @retval false    When RSSI measurement is not started.
 */
bool nrf_802154_trx_rssi_measure_is_started(void);

/**@brief Checks if RSSI sample is available.
 *
 * @retval true    When RSSI sample is available.  The sample may be read by a call to @ref nrf_802154_trx_rssi_last_sample_get
 * @retval false   When RSSI sample is unavailable.
 */
bool nrf_802154_trx_rssi_sample_is_available(void);

/**@brief Returns last measured RSSI sample.
 *
 * @return RSSI sample. Returned value must be scaled using API provided by nrf_802154_rssi.h.
 */
uint8_t nrf_802154_trx_rssi_last_sample_get(void);

/**@brief Check if PSDU is currently being received.
 *
 * @retval true     If trx is in receive mode triggered by nrf_802154_trx_receive_frame and
 *                  a frame receive has been started but not finished yet.
 * @retval false    Otherwise.
 *
 * @note This function returns false when receive has been triggered by nrf_802154_trx_receive_ack
 *       regardless of the fact if the frame on air has been started or not.
 */
bool nrf_802154_trx_psdu_is_being_received(void);

bool nrf_802154_trx_receive_is_buffer_missing(void);

bool nrf_802154_trx_receive_buffer_set(void * p_receive_buffer);

void nrf_802154_trx_transmit_frame(const void * p_transmit_buffer, bool cca);

/**@brief Puts the trx module into transmit ACK mode.
 *
 *
 * @note This function may be called from @ref nrf_802154_trx_receive_received handler
 *       with state == TRX_STATE_RXFRAME passed only. This is because in this condition only
 *       the TIMER peripheral is running and allows timed transmission.
 *
 * @param[in] p_transmit_buffer     Pointer to a buffer containing ACK frame to be transmitted.
 * @param[in] delay_us              Delay (in microseconds)
 *
 * @retval true     If the function was called in time and ACK frame is scheduled for transmission.
 *                  When transmission starts and @ref NRF_802154_TX_STARTED_NOTIFY_ENABLED != 0
 *                  the function @ref nrf_802154_trx_transmit_started(@ref TRX_STATE_TXACK) will be called.
 *                  When transmission is finished the function @ref nrf_802154_trx_transmit_transmitted(@ref TRX_STATE_TXACK)
 *                  function will be called.
 * @retval false    If the function was called too late and given delay_us time gap
 *                  between received frame and ACK frame transmission could not be hold.
 *                  The TIMER peripheral is stopped and it is not possible to trigger @ref nrf_802154_trx_transmit_ack
 *                  again without receiving another frame again. No callbacks will be called.
 */
bool nrf_802154_trx_transmit_ack(const void * p_transmit_buffer, uint32_t delay_us);

bool nrf_802154_trx_go_idle(void);

void nrf_802154_trx_standalone_cca(void);

void nrf_802154_trx_continuous_carrier(void);
void nrf_802154_trx_continuous_carrier_restart(void);

/**@brief Puts trx module into energy detection mode.
 *
 * Operation ends up with a call to ref nrf_802154_trx_energy_detection_finished
 *
 * Operation can be terminated with a call to @ref nrf_802154_trx_energy_detection_abort,
 * @ref nrf_802154_trx_abort or @ref nrf_802154_trx_disable. In this case no callback is called.
 *
 * @param ed_count  Number of iterations to perform. Must be in range 1..2097152 (TODO: where define it)
 *                  One iteration takes (TODO: define, in procedures_duration.h?) 128us
 */
void nrf_802154_trx_energy_detection(uint32_t ed_count);

void nrf_802154_trx_abort(void);

/**@brief   Handler called from isr at the beginning of a frame reception (just after synchronization header is received).
 * @note Proper implementation of this function is out of scope of the trx module.
 */
extern void nrf_802154_trx_receive_on_shr(trx_state_t state);

/**@brief  Handler called from isr during reception of a frame, when given number of bytes is received.
 *
 * @note Proper implementation of this function is out of scope of the trx module.
 *
 * @note If the handler decides to abort receive by a call to (TODO) @ref nrf_802154_trx_receive_abort
 * it must return value equal to original bcc parameter passed.
 *
 * @param[in]   bcc   Number of bytes that have been already received.
 *
 * @return  Value greater than original value of bcc parameter will cause @ref nrf_802154_trx_receive_on_bcmatch
 *          to be called again when further data arrive. Value less than or equal to original bcc value will not cause this
 *          behavior.
 *
 */
extern uint8_t nrf_802154_trx_receive_on_bcmatch(uint8_t bcc);

extern void nrf_802154_trx_receive_received(trx_state_t state);
extern void nrf_802154_trx_receive_crcerror(trx_state_t state);

extern void nrf_802154_trx_transmit_ccabusy(void);
extern void nrf_802154_trx_transmit_started(trx_state_t state); // TODO change name nrf_802154_trx_transmit_on_shr
extern void nrf_802154_trx_transmit_transmitted(trx_state_t state);

extern void nrf_802154_trx_in_idle(void);

extern void nrf_802154_trx_standalone_cca_finished(bool channel_was_idle);

extern void nrf_802154_trx_energy_detection_finished(uint8_t ed_sample);

#ifdef __cplusplus
}
#endif

#endif /* NRF_802154_TRX_H_ */
