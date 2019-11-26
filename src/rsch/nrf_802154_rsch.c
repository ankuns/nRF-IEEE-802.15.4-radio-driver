
#define NRF_802154_MODULE_ID NRF_802154_MODULE_ID_RSCH

#include "nrf_802154_rsch.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <nrf.h>

#include "../nrf_802154_debug.h"
#include "nrf_802154_priority_drop.h"
#include "nrf_802154_stats.h"
#include "platform/clock/nrf_802154_clock.h"
#include "platform/coex/nrf_802154_wifi_coex.h"
#include "raal/nrf_raal_api.h"
#include "timer_scheduler/nrf_802154_timer_sched.h"

/* The following macro defines ramp-up time of preconditions [us]. It depends on HF clock,
 * which takes the longest to ramp-up out of all preconditions.
 * In case of nRF52811, the value of this macro is the sum of 360us of HFXO startup time,
 * 31us of timer granularity margin, 55us of POWER_CLOCK_IRQHandler processing time, 60us of
 * RTC_IRQHandler processing time and 9us of margin.
 * In case of nRF52840, the value of this macro is the sum of 256us of HFXO debounce time,
 * 75us of the worst case power-up time for an Epson crystal, 31us of timer granularity margin,
 * 50us of POWER_CLOCK_IRQHandler processing time, 60us of RTC_IRQHandler processing time
 * and 8us of margin.
 */
#ifdef NRF52811_XXAA
#define PREC_HFXO_STARTUP_TIME                 360
#define PREC_TIMER_GRANULARITY_MARGIN          31
#define PREC_POWER_CLOCK_IRQ_HANDLER_PROC_TIME 50
#define PREC_RTC_IRQ_HANDLER_PROC_TIME         60
#define PREC_RAMP_UP_MARGIN                    9
#define PREC_RAMP_UP_TIME                      (PREC_HFXO_STARTUP_TIME +                 \
                                                PREC_TIMER_GRANULARITY_MARGIN +          \
                                                PREC_POWER_CLOCK_IRQ_HANDLER_PROC_TIME + \
                                                PREC_RTC_IRQ_HANDLER_PROC_TIME +         \
                                                PREC_RAMP_UP_MARGIN)
#else
#define PREC_HFXO_DEBOUNCE_TIME                256
#define PREC_CRYSTAL_WORST_CASE_POWER_UP_TIME  75
#define PREC_TIMER_GRANULARITY_MARGIN          31
#define PREC_POWER_CLOCK_IRQ_HANDLER_PROC_TIME 50
#define PREC_RTC_IRQ_HANDLER_PROC_TIME         60
#define PREC_RAMP_UP_MARGIN                    8
#define PREC_RAMP_UP_TIME                      (PREC_HFXO_DEBOUNCE_TIME +                \
                                                PREC_CRYSTAL_WORST_CASE_POWER_UP_TIME +  \
                                                PREC_TIMER_GRANULARITY_MARGIN +          \
                                                PREC_POWER_CLOCK_IRQ_HANDLER_PROC_TIME + \
                                                PREC_RTC_IRQ_HANDLER_PROC_TIME +         \
                                                PREC_RAMP_UP_MARGIN)
#endif

static volatile uint8_t     m_ntf_mutex;                     ///< Mutex for notyfying core.
static volatile uint8_t     m_ntf_mutex_monitor;             ///< Mutex monitor, incremented every failed ntf mutex lock.
static volatile uint8_t     m_req_mutex;                     ///< Mutex for requesting preconditions.
static volatile uint8_t     m_req_mutex_monitor;             ///< Mutex monitor, incremented every failed req mutex lock.
static volatile rsch_prio_t m_last_notified_prio;            ///< Last reported approved priority level.
static volatile rsch_prio_t m_approved_prios[RSCH_PREC_CNT]; ///< Priority levels approved by each precondition.
static rsch_prio_t          m_requested_prio;                ///< Priority requested from all preconditions.
static rsch_prio_t          m_cont_mode_prio;                ///< Continuous mode priority level. If continuous mode is not requested equal to @ref RSCH_PRIO_IDLE.

typedef struct
{
    nrf_802154_timer_t  timer; ///< Timer used to trigger delayed timeslot.
    rsch_dly_ts_param_t param; ///< Parameters of the delayed timeslot.
} dly_ts_t;

static dly_ts_t m_dly_ts[RSCH_DLY_TS_NUM];

/** @brief Non-blocking mutex for notifying core.
 *
 *  @param[inout]  p_mutex          Pointer to the mutex data.
 *  @param[inout]  p_mutex_monitor  Pointer to the mutex monitor counter.
 *
 *  @retval  true   Mutex was acquired.
 *  @retval  false  Mutex could not be acquired.
 */
static inline bool mutex_trylock(volatile uint8_t * p_mutex, volatile uint8_t * p_mutex_monitor)
{
    nrf_802154_log_function_enter(NRF_802154_LOG_VERBOSITY_HIGH);

    do
    {
        uint8_t mutex_value = __LDREXB(p_mutex);

        if (mutex_value)
        {
            __CLREX();

            (*p_mutex_monitor)++;

            nrf_802154_log_function_exit(NRF_802154_LOG_VERBOSITY_HIGH);

            return false;
        }
    }
    while (__STREXB(1, p_mutex));

    __DMB();

    nrf_802154_log_function_exit(NRF_802154_LOG_VERBOSITY_HIGH);

    return true;
}

/** @brief Release mutex. */
static inline void mutex_unlock(volatile uint8_t * p_mutex)
{
    nrf_802154_log_function_enter(NRF_802154_LOG_VERBOSITY_HIGH);

    __DMB();
    *p_mutex = 0;

    nrf_802154_log_function_exit(NRF_802154_LOG_VERBOSITY_HIGH);
}

/** @brief Check maximal priority level required by any of delayed timeslots at the moment.
 *
 * For delayed timeslots of the @ref RSCH_DLY_TS_TYPE_PRECISE type, in order to meet their
 * timing requirements there is a time window in which radio preconditions should be requested.
 * This function is used to prevent releasing preconditions in this time window.
 *
 * For delayed timeslots of the @ref RSCH_DLY_TS_TYPE_RELAXED type, this function prevents
 * releasing their preconditions as long as they are active (i.e. not cancelled explicitly).
 *
 * @return  Maximal priority level required by delayed timeslots.
 */
static rsch_prio_t max_prio_for_delayed_timeslot_get(void)
{
    nrf_802154_log_function_enter(NRF_802154_LOG_VERBOSITY_HIGH);

    rsch_prio_t result = RSCH_PRIO_IDLE;
    uint32_t    now    = nrf_802154_timer_sched_time_get();

    for (uint32_t i = 0; i < RSCH_DLY_TS_NUM; i++)
    {
        dly_ts_t  * p_dly_ts    = &m_dly_ts[i];
        rsch_prio_t dly_ts_prio = p_dly_ts->param.prio;
        uint32_t    t0          = p_dly_ts->param.t0;
        uint32_t    dt          = p_dly_ts->param.dt - PREC_RAMP_UP_TIME -
                                  nrf_802154_timer_sched_granularity_get();

        if ((p_dly_ts->param.type == RSCH_DLY_TS_TYPE_PRECISE) &&
            nrf_802154_timer_sched_time_is_in_future(now, t0, dt))
        {
            dly_ts_prio = RSCH_PRIO_IDLE;
        }

        result = dly_ts_prio > result ? dly_ts_prio : result;
    }

    nrf_802154_log_function_exit(NRF_802154_LOG_VERBOSITY_HIGH);

    return result;
}

static rsch_prio_t required_prio_lvl_get(void)
{
    nrf_802154_log_function_enter(NRF_802154_LOG_VERBOSITY_HIGH);

    rsch_prio_t result = max_prio_for_delayed_timeslot_get();

    if (m_cont_mode_prio > result)
    {
        result = m_cont_mode_prio;
    }

    nrf_802154_log_function_exit(NRF_802154_LOG_VERBOSITY_HIGH);

    return result;
}

/** @brief Set approved priority level @p prio on given precondition @p prec.
 *
 * When requested priority level equals to the @ref RSCH_PRIO_IDLE this function will approve only
 * the @ref RSCH_PRIO_IDLE priority level and drop other approved levels silently.
 *
 * @param[in]  prec    Precondition which state will be changed.
 * @param[in]  prio    Approved priority level for given precondition.
 */
static inline void prec_approved_prio_set(rsch_prec_t prec, rsch_prio_t prio)
{
    nrf_802154_log_function_enter(NRF_802154_LOG_VERBOSITY_HIGH);

    assert(prec <= RSCH_PREC_CNT);
    assert((m_approved_prios[prec] != prio) || (prio == RSCH_PRIO_IDLE));

    m_approved_prios[prec] = prio;

    nrf_802154_log_function_exit(NRF_802154_LOG_VERBOSITY_HIGH);
}

/** @brief Request all preconditions.
 */
static inline void all_prec_update(void)
{
    nrf_802154_log_function_enter(NRF_802154_LOG_VERBOSITY_HIGH);

    rsch_prio_t prev_prio;
    rsch_prio_t new_prio;
    uint8_t     monitor;

    do
    {
        if (!mutex_trylock(&m_req_mutex, &m_req_mutex_monitor))
        {
            break;
        }

        monitor   = m_req_mutex_monitor;
        prev_prio = m_requested_prio;
        new_prio  = required_prio_lvl_get();

        if (prev_prio != new_prio)
        {
            m_requested_prio = new_prio;

            if (new_prio == RSCH_PRIO_IDLE)
            {
                nrf_802154_priority_drop_hfclk_stop();
                prec_approved_prio_set(RSCH_PREC_HFCLK, RSCH_PRIO_IDLE);

                nrf_raal_continuous_mode_exit();
                prec_approved_prio_set(RSCH_PREC_RAAL, RSCH_PRIO_IDLE);
            }
            else if (prev_prio == RSCH_PRIO_IDLE)
            {
                // If the previously requested priority is IDLE,
                // both HFCLK and RAAL preconditions should be idle.
                assert(m_approved_prios[RSCH_PREC_HFCLK] == RSCH_PRIO_IDLE);
                assert(m_approved_prios[RSCH_PREC_RAAL] == RSCH_PRIO_IDLE);

                nrf_802154_priority_drop_hfclk_stop_terminate();
                nrf_802154_clock_hfclk_start();

                nrf_raal_continuous_mode_enter();
            }
            else
            {
                // Intentionally empty
            }

            nrf_802154_wifi_coex_prio_request(new_prio);
        }

        mutex_unlock(&m_req_mutex);
    }
    while (monitor != m_req_mutex_monitor);

    nrf_802154_log_function_exit(NRF_802154_LOG_VERBOSITY_HIGH);
}

/** @brief Get currently approved priority level.
 *
 * @return Maximal priority level approved by all radio preconditions.
 */
static inline rsch_prio_t approved_prio_lvl_get(void)
{
    nrf_802154_log_function_enter(NRF_802154_LOG_VERBOSITY_HIGH);

    rsch_prio_t result = RSCH_PRIO_MAX;

    for (uint32_t i = 0; i < RSCH_PREC_CNT; i++)
    {
        if (m_approved_prios[i] < result)
        {
            result = m_approved_prios[i];
        }
    }

    nrf_802154_log_function_exit(NRF_802154_LOG_VERBOSITY_HIGH);

    return result;
}

/** @brief Check if all preconditions are requested or met at given priority level or higher.
 *
 * @param[in]  prio  Minimal priority level requested from preconditions.
 *
 * @retval true   All preconditions are requested or met at given or higher level.
 * @retval false  At least one precondition is requested at lower level than required.
 */
static inline bool requested_prio_lvl_is_at_least(rsch_prio_t prio)
{
    nrf_802154_log_function_enter(NRF_802154_LOG_VERBOSITY_HIGH);
    nrf_802154_log_function_exit(NRF_802154_LOG_VERBOSITY_HIGH);

    return m_requested_prio >= prio;
}

/** @brief Notify core if preconditions are approved or denied if current state differs from last reported.
 *
 * @retval true   Core was notified.
 * @retval false  Otherwise.
 */
static inline bool notify_core(void)
{
    nrf_802154_log_function_enter(NRF_802154_LOG_VERBOSITY_HIGH);

    rsch_prio_t approved_prio_lvl;
    uint8_t     temp_mon;
    bool        notified = false;

    do
    {
        if (!mutex_trylock(&m_ntf_mutex, &m_ntf_mutex_monitor))
        {
            break;
        }

        /* It is possible that preemption is not detected (m_ntf_mutex_monitor is read after
         * acquiring mutex). It is not a problem because we will call proper handler function
         * requested by preempting context. Avoiding this race would generate one additional
         * iteration without any effect.
         */
        temp_mon          = m_ntf_mutex_monitor;
        approved_prio_lvl = approved_prio_lvl_get();

        if (m_last_notified_prio != approved_prio_lvl)
        {
            m_last_notified_prio = approved_prio_lvl;

            nrf_802154_rsch_continuous_prio_changed(approved_prio_lvl);

            notified = true;
        }

        mutex_unlock(&m_ntf_mutex);
    }
    while (temp_mon != m_ntf_mutex_monitor);

    nrf_802154_log_function_exit(NRF_802154_LOG_VERBOSITY_HIGH);

    return notified;
}

/** Timer callback used to trigger delayed timeslot.
 *
 * @param[in]  p_context  Index of the delayed timeslot operation (TX or RX).
 */
static void delayed_timeslot_start(void * p_context)
{
    nrf_802154_log_function_enter(NRF_802154_LOG_VERBOSITY_LOW);

    rsch_dly_ts_id_t dly_ts_id = (rsch_dly_ts_id_t)(uint32_t)p_context;
    dly_ts_t       * p_dly_ts  = &m_dly_ts[dly_ts_id];

    p_dly_ts->param.started_callback(dly_ts_id);

    // Drop preconditions immediately if precise timeslot type was selected
    if (p_dly_ts->param.type == RSCH_DLY_TS_TYPE_PRECISE)
    {
        p_dly_ts->param.prio = RSCH_PRIO_IDLE;
        all_prec_update();
        notify_core();
    }

    nrf_802154_log_function_exit(NRF_802154_LOG_VERBOSITY_LOW);
}

/** Timer callback used to request preconditions for delayed timeslot.
 *
 * @param[in]  p_context  Index of the delayed timeslot operation (TX or RX).
 */
static void delayed_timeslot_prec_request(void * p_context)
{
    nrf_802154_log_function_enter(NRF_802154_LOG_VERBOSITY_LOW);

    rsch_dly_ts_id_t dly_ts_id = (rsch_dly_ts_id_t)(uint32_t)p_context;
    dly_ts_t       * p_dly_ts  = &m_dly_ts[dly_ts_id];

    all_prec_update();

    p_dly_ts->timer.t0        = p_dly_ts->param.t0;
    p_dly_ts->timer.dt        = p_dly_ts->param.dt;
    p_dly_ts->timer.callback  = delayed_timeslot_start;
    p_dly_ts->timer.p_context = p_context;

    nrf_802154_timer_sched_add(&p_dly_ts->timer, true);

    nrf_802154_log_function_exit(NRF_802154_LOG_VERBOSITY_LOW);
}

/**
 * @brief Requests a @ref RSCH_DLY_TS_TYPE_PRECISE timeslot.
 *
 * @param[in]  p_dly_ts        ID of the requested timeslot.
 * @param[in]  p_dly_ts_param  Parameters of the requested delayed timeslot.
 *
 * @retval true   Requested timeslot has been scheduled.
 * @retval false  Requested timeslot cannot be scheduled and will not be granted.
 */
static bool precise_delayed_timeslot_request(dly_ts_t                  * p_dly_ts,
                                             const rsch_dly_ts_param_t * p_param)
{
    nrf_802154_log_function_enter(NRF_802154_LOG_VERBOSITY_LOW);

    // Assert that no operation is scheduled for the selected slot
    assert(p_dly_ts->param.prio == RSCH_PRIO_IDLE);
    assert(!nrf_802154_timer_sched_is_running(&p_dly_ts->timer));

    uint32_t now    = nrf_802154_timer_sched_time_get();
    uint32_t req_dt = p_param->dt - PREC_RAMP_UP_TIME;
    bool     result = false;

    if (nrf_802154_timer_sched_time_is_in_future(now, p_param->t0, req_dt))
    {
        // There is enough time for preconditions ramp-up no matter their current state.

        p_dly_ts->param = *p_param;

        p_dly_ts->timer.t0        = p_param->t0;
        p_dly_ts->timer.dt        = req_dt;
        p_dly_ts->timer.callback  = delayed_timeslot_prec_request;
        p_dly_ts->timer.p_context = (void *)p_param->id;

        nrf_802154_timer_sched_add(&p_dly_ts->timer, false);

        result = true;
    }
    else if (requested_prio_lvl_is_at_least(RSCH_PRIO_IDLE_LISTENING) &&
             nrf_802154_timer_sched_time_is_in_future(now, p_param->t0, p_param->dt))
    {
        // There is not enough time to perform full precondition ramp-up.
        // Try with the currently approved preconditions

        p_dly_ts->param = *p_param;

        p_dly_ts->timer.t0        = p_param->t0;
        p_dly_ts->timer.dt        = p_param->dt;
        p_dly_ts->timer.callback  = delayed_timeslot_start;
        p_dly_ts->timer.p_context = (void *)p_param->id;

        all_prec_update();
        nrf_802154_timer_sched_add(&p_dly_ts->timer, true);

        result = true;
    }
    else
    {
        // The requested time is in the past.
    }

    nrf_802154_log_function_exit(NRF_802154_LOG_VERBOSITY_LOW);

    return result;
}

/**
 * @brief Requests a @ref RSCH_DLY_TS_TYPE_RELAXED timeslot.
 *
 * @param[in]  p_dly_ts        ID of the requested timeslot.
 * @param[in]  p_dly_ts_param  Parameters of the requested delayed timeslot.
 *
 * @return This function always returns true.
 */
static bool relaxed_delayed_timeslot_request(dly_ts_t                  * p_dly_ts,
                                             const rsch_dly_ts_param_t * p_param)
{
    nrf_802154_log_function_enter(NRF_802154_LOG_VERBOSITY_LOW);

    p_dly_ts->param = *p_param;

    p_dly_ts->timer.t0        = p_param->t0;
    p_dly_ts->timer.dt        = p_param->dt;
    p_dly_ts->timer.callback  = delayed_timeslot_start;
    p_dly_ts->timer.p_context = (void *)p_param->id;

    all_prec_update();

    nrf_802154_timer_sched_add(&p_dly_ts->timer, false);

    nrf_802154_log_function_exit(NRF_802154_LOG_VERBOSITY_LOW);

    return true;
}

/***************************************************************************************************
 * Public API
 **************************************************************************************************/

void nrf_802154_rsch_init(void)
{
    nrf_raal_init();

    m_ntf_mutex          = 0;
    m_req_mutex          = 0;
    m_last_notified_prio = RSCH_PRIO_IDLE;
    m_cont_mode_prio     = RSCH_PRIO_IDLE;
    m_requested_prio     = RSCH_PRIO_IDLE;

    for (uint32_t i = 0; i < RSCH_DLY_TS_NUM; i++)
    {
        m_dly_ts[i].param.prio = RSCH_PRIO_IDLE;
    }

    for (uint32_t i = 0; i < RSCH_PREC_CNT; i++)
    {
        m_approved_prios[i] = RSCH_PRIO_IDLE;
    }

    nrf_802154_wifi_coex_init();
}

void nrf_802154_rsch_uninit(void)
{
    for (uint32_t i = 0; i < RSCH_DLY_TS_NUM; i++)
    {
        nrf_802154_timer_sched_remove(&m_dly_ts[i].timer, NULL);
    }

    nrf_802154_wifi_coex_uninit();
    nrf_raal_uninit();
}

void nrf_802154_rsch_continuous_mode_priority_set(rsch_prio_t prio)
{
    nrf_802154_log_function_enter(NRF_802154_LOG_VERBOSITY_LOW);

    nrf_802154_log_local_event(NRF_802154_LOG_VERBOSITY_LOW,
                               NRF_802154_LOG_LOCAL_EVENT_ID_RSCH_PRIORITY_SET, (uint32_t)prio);

    m_cont_mode_prio = prio;
    __DMB();

    all_prec_update();
    notify_core();

    nrf_802154_log_function_exit(NRF_802154_LOG_VERBOSITY_LOW);
}

void nrf_802154_rsch_continuous_ended(void)
{
    nrf_raal_continuous_ended();
}

bool nrf_802154_rsch_timeslot_request(uint32_t length_us)
{
    return nrf_raal_timeslot_request(length_us);
}

bool nrf_802154_rsch_delayed_timeslot_request(const rsch_dly_ts_param_t * p_dly_ts_param)
{
    nrf_802154_log_function_enter(NRF_802154_LOG_VERBOSITY_LOW);

    assert(p_dly_ts_param->id < RSCH_DLY_TS_NUM);
    assert(p_dly_ts_param->prio != RSCH_PRIO_IDLE);

    dly_ts_t * p_dly_ts = &m_dly_ts[p_dly_ts_param->id];
    bool       result   = true;

    switch (p_dly_ts_param->type)
    {
        case RSCH_DLY_TS_TYPE_PRECISE:
            result = precise_delayed_timeslot_request(p_dly_ts, p_dly_ts_param);
            break;

        case RSCH_DLY_TS_TYPE_RELAXED:
            result = relaxed_delayed_timeslot_request(p_dly_ts, p_dly_ts_param);
            break;

        default:
            result = false;
            assert(false);
            break;
    }

    nrf_802154_log_function_exit(NRF_802154_LOG_VERBOSITY_LOW);

    return result;
}

bool nrf_802154_rsch_delayed_timeslot_cancel(rsch_dly_ts_id_t dly_ts_id)
{
    nrf_802154_log_function_enter(NRF_802154_LOG_VERBOSITY_LOW);

    assert(dly_ts_id < RSCH_DLY_TS_NUM);

    bool       result   = false;
    dly_ts_t * p_dly_ts = &m_dly_ts[dly_ts_id];
    bool       was_running;

    nrf_802154_timer_sched_remove(&p_dly_ts->timer, &was_running);

    if (p_dly_ts->param.prio != RSCH_PRIO_IDLE)
    {
        p_dly_ts->param.prio = RSCH_PRIO_IDLE;
        all_prec_update();
        notify_core();
    }

    switch (p_dly_ts->param.type)
    {
        case RSCH_DLY_TS_TYPE_PRECISE:
            result = was_running;
            break;

        case RSCH_DLY_TS_TYPE_RELAXED:
            result = true;
            break;

        default:
            assert(false);
    }

    nrf_802154_log_function_exit(NRF_802154_LOG_VERBOSITY_LOW);

    return result;
}

bool nrf_802154_rsch_delayed_timeslot_priority_update(rsch_dly_ts_id_t dly_ts_id,
                                                      rsch_prio_t      dly_ts_prio)
{
    assert(dly_ts_id < RSCH_DLY_TS_NUM);

    dly_ts_t * p_dly_ts = &m_dly_ts[dly_ts_id];

    // Do not modify inactive timeslot
    if (p_dly_ts->param.prio == RSCH_PRIO_IDLE)
    {
        return false;
    }
    else
    {
        p_dly_ts->param.prio = dly_ts_prio;
        return true;
    }
}

bool nrf_802154_rsch_timeslot_is_requested(void)
{
    bool result = false;

    for (uint32_t i = 0; i < RSCH_PREC_CNT; i++)
    {
        if (m_approved_prios[i] > RSCH_PRIO_IDLE)
        {
            result = true;
            break;
        }
    }

    return result;
}

bool nrf_802154_rsch_prec_is_approved(rsch_prec_t prec, rsch_prio_t prio)
{
    assert(prec < RSCH_PREC_CNT);
    return m_approved_prios[prec] >= prio;
}

uint32_t nrf_802154_rsch_timeslot_us_left_get(void)
{
    return nrf_raal_timeslot_us_left_get();
}

// External handlers

void nrf_raal_timeslot_started(void)
{
    nrf_802154_log_function_enter(NRF_802154_LOG_VERBOSITY_LOW);

    prec_approved_prio_set(RSCH_PREC_RAAL, RSCH_PRIO_MAX);
    notify_core();

    nrf_802154_log_function_exit(NRF_802154_LOG_VERBOSITY_LOW);
}

void nrf_raal_timeslot_ended(void)
{
    nrf_802154_log_function_enter(NRF_802154_LOG_VERBOSITY_LOW);

    prec_approved_prio_set(RSCH_PREC_RAAL, RSCH_PRIO_IDLE);

    // Ensure that RAAL can finish its processing even if core is not informed about it.
    if (!notify_core())
    {
        nrf_802154_rsch_continuous_ended();
    }

    nrf_802154_log_function_exit(NRF_802154_LOG_VERBOSITY_LOW);
}

void nrf_802154_clock_hfclk_ready(void)
{
    nrf_802154_log_function_enter(NRF_802154_LOG_VERBOSITY_LOW);

    prec_approved_prio_set(RSCH_PREC_HFCLK, RSCH_PRIO_MAX);
    notify_core();

    nrf_802154_log_function_exit(NRF_802154_LOG_VERBOSITY_LOW);
}

void nrf_802154_wifi_coex_granted(void)
{
    nrf_802154_log_function_enter(NRF_802154_LOG_VERBOSITY_LOW);

    prec_approved_prio_set(RSCH_PREC_COEX, RSCH_PRIO_MAX);
    // TODO: make conditional on CONFIG/PRIORITY pin: tx/rx granted
    notify_core();

    nrf_802154_log_function_exit(NRF_802154_LOG_VERBOSITY_LOW);
}

void nrf_802154_wifi_coex_denied(void)
{
    nrf_802154_log_function_enter(NRF_802154_LOG_VERBOSITY_LOW);

    prec_approved_prio_set(RSCH_PREC_COEX, RSCH_PRIO_RX);
    notify_core();

    nrf_802154_log_function_exit(NRF_802154_LOG_VERBOSITY_LOW);
}

void nrf_802154_wifi_coex_request_changed(
    nrf_802154_wifi_coex_request_state_t curr_request_state,
    nrf_802154_wifi_coex_request_state_t prev_request_state)
{
    (void)prev_request_state;

    // Note: state can change also between WFFI_COEX_REQUEST_STATE_RX
    // and WFFI_COEX_REQUEST_STATE_TX states.

    if ((prev_request_state == WFFI_COEX_REQUEST_STATE_NO_REQUEST) &&
        (curr_request_state != WFFI_COEX_REQUEST_STATE_NO_REQUEST))
    {
        nrf_802154_stat_counter_increment(coex_requests);
    }
}


