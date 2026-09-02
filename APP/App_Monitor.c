#include "App_Monitor.h"

#include <stdio.h>

#include "App_Monitor_Config.h"
#include "App_Monitor_Port.h"
#include "ML307C.h"
#include "SHT30.h"

typedef enum
{
    APP_MONITOR_STATE_IDLE = 0U,
    APP_MONITOR_STATE_MOTION_SETTLE,
    APP_MONITOR_STATE_MOTION_CONFIRM,
    APP_MONITOR_STATE_PERIODIC_SAMPLE,
    APP_MONITOR_STATE_MOTION_SAMPLE,
    APP_MONITOR_STATE_SMS_REQUEST,
    APP_MONITOR_STATE_WAIT_SMS_RESULT,
    APP_MONITOR_STATE_SLEEP
} App_Monitor_State_t;

static volatile bool s_rtc_alarm_pending;
static volatile bool s_motion_pending;
static App_Monitor_State_t s_state;
static uint32_t s_state_enter_tick_ms;
static bool s_first_cycle_pending;
static bool s_next_alarm_required;
static bool s_last_motion_sms_time_valid;
static uint32_t s_last_motion_sms_rtc_seconds;
static bool s_current_sms_is_motion;
static char s_sms_text[APP_SMS_TEXT_MAX_SIZE + 1U];

static const uint8_t s_boot_message[] = "APP: monitor started\r\n";
static const uint8_t s_alarm_set_failed_message[] = "APP: RTC alarm set failed\r\n";
static const uint8_t s_enter_stop_message[] = "APP: enter STOP\r\n";
static const uint8_t s_rtc_wakeup_message[] = "APP: RTC wakeup\r\n";
static const uint8_t s_motion_wakeup_message[] = "APP: motion wakeup, settling\r\n";
static const uint8_t s_motion_confirming_message[] = "APP: motion high, confirming\r\n";
static const uint8_t s_motion_cancelled_message[] = "APP: motion cancelled\r\n";
static const uint8_t s_motion_confirmed_message[] = "APP: MOTION CONFIRMED\r\n";
static const uint8_t s_motion_cooled_message[] = "APP: MOTION suppressed by cooldown\r\n";
static const uint8_t s_sms_requested_message[] = "APP: SMS requested\r\n";
static const uint8_t s_sms_success_message[] = "APP: SMS success\r\n";
static const uint8_t s_sms_failed_message[] = "APP: SMS failed\r\n";
static const uint8_t s_sms_request_timeout_message[] = "APP: SMS request timeout\r\n";

static void App_Monitor_TransitionTo(App_Monitor_State_t next_state, uint32_t now_tick_ms);
static void App_Monitor_WriteDebug(const uint8_t *data, uint16_t size);
static void App_Monitor_HandleIdle(uint32_t now_tick_ms);
static void App_Monitor_HandleMotionSettle(uint32_t now_tick_ms, bool motion_active);
static void App_Monitor_HandleMotionConfirm(uint32_t now_tick_ms, bool motion_active);
static void App_Monitor_HandleSample(uint32_t now_tick_ms, bool is_motion);
static void App_Monitor_HandleSmsRequest(uint32_t now_tick_ms);
static void App_Monitor_HandleWaitSmsResult(uint32_t now_tick_ms);
static void App_Monitor_HandleSleep(uint32_t now_tick_ms);
static bool App_Monitor_IsMotionCooldownActive(void);

static void App_Monitor_TransitionTo(App_Monitor_State_t next_state, uint32_t now_tick_ms)
{
    s_state = next_state;
    s_state_enter_tick_ms = now_tick_ms;
}

static void App_Monitor_WriteDebug(const uint8_t *data, uint16_t size)
{
    if (APP_MONITOR_DEBUG_ENABLED != 0U)
    {
        App_Monitor_Port_WriteDebug(data, size);
    }
}

static bool App_Monitor_IsMotionCooldownActive(void)
{
    uint32_t now_rtc_seconds;
    uint32_t elapsed_seconds;

    if ((!s_last_motion_sms_time_valid) || !App_Monitor_Port_GetRtcSeconds(&now_rtc_seconds))
    {
        return false;
    }

    elapsed_seconds = (now_rtc_seconds + 86400U - s_last_motion_sms_rtc_seconds) % 86400U;
    return elapsed_seconds < APP_MOTION_SMS_COOLDOWN_SECONDS;
}

static void App_Monitor_HandleIdle(uint32_t now_tick_ms)
{
    if (s_first_cycle_pending)
    {
        s_first_cycle_pending = false;
        App_Monitor_WriteDebug(s_boot_message, sizeof(s_boot_message) - 1U);
        App_Monitor_TransitionTo(APP_MONITOR_STATE_SLEEP, now_tick_ms);
    }
    else if (s_rtc_alarm_pending)
    {
        s_rtc_alarm_pending = false;
        s_next_alarm_required = true;
        App_Monitor_WriteDebug(s_rtc_wakeup_message, sizeof(s_rtc_wakeup_message) - 1U);
        App_Monitor_TransitionTo(APP_MONITOR_STATE_PERIODIC_SAMPLE, now_tick_ms);
    }
    else if (s_motion_pending)
    {
        s_motion_pending = false;
        App_Monitor_WriteDebug(s_motion_wakeup_message, sizeof(s_motion_wakeup_message) - 1U);
        App_Monitor_TransitionTo(APP_MONITOR_STATE_MOTION_SETTLE, now_tick_ms);
    }
}

static void App_Monitor_HandleMotionSettle(uint32_t now_tick_ms, bool motion_active)
{
    if ((uint32_t)(now_tick_ms - s_state_enter_tick_ms) < APP_MOTION_EDGE_SETTLE_MS)
    {
        return;
    }

    if (motion_active)
    {
        App_Monitor_WriteDebug(s_motion_confirming_message, sizeof(s_motion_confirming_message) - 1U);
        App_Monitor_TransitionTo(APP_MONITOR_STATE_MOTION_CONFIRM, now_tick_ms);
    }
    else
    {
        App_Monitor_WriteDebug(s_motion_cancelled_message, sizeof(s_motion_cancelled_message) - 1U);
        App_Monitor_TransitionTo(APP_MONITOR_STATE_SLEEP, now_tick_ms);
    }
}

static void App_Monitor_HandleMotionConfirm(uint32_t now_tick_ms, bool motion_active)
{
    if (!motion_active)
    {
        App_Monitor_WriteDebug(s_motion_cancelled_message, sizeof(s_motion_cancelled_message) - 1U);
        App_Monitor_TransitionTo(APP_MONITOR_STATE_SLEEP, now_tick_ms);
    }
    else if ((uint32_t)(now_tick_ms - s_state_enter_tick_ms) >= APP_MOTION_CONFIRM_HOLD_MS)
    {
        App_Monitor_WriteDebug(s_motion_confirmed_message, sizeof(s_motion_confirmed_message) - 1U);

        if (App_Monitor_IsMotionCooldownActive())
        {
            App_Monitor_WriteDebug(s_motion_cooled_message, sizeof(s_motion_cooled_message) - 1U);
            App_Monitor_TransitionTo(APP_MONITOR_STATE_SLEEP, now_tick_ms);
        }
        else
        {
            App_Monitor_TransitionTo(APP_MONITOR_STATE_MOTION_SAMPLE, now_tick_ms);
        }
    }
}

static void App_Monitor_HandleSample(uint32_t now_tick_ms, bool is_motion)
{
    SHT30_Sample_t sample;
    int length;

    if (SHT30_ReadSingleShot(&sample))
    {
        int32_t temperature_fraction = sample.temperature_centi_c % 100;

        if (temperature_fraction < 0)
        {
            temperature_fraction = -temperature_fraction;
        }

        length = snprintf(s_sms_text, sizeof(s_sms_text), "%s T=%ld.%02ldC RH=%lu.%02lu%%",
                          is_motion ? "MOTION" : "PERIODIC",
                          (long)(sample.temperature_centi_c / 100), (long)temperature_fraction,
                          (unsigned long)(sample.humidity_centi_percent / 100U),
                          (unsigned long)(sample.humidity_centi_percent % 100U));
    }
    else
    {
        length = snprintf(s_sms_text, sizeof(s_sms_text), "%s SHT30 READ FAILED",
                          is_motion ? "MOTION" : "PERIODIC");
    }

    if ((length <= 0) || (length >= (int)sizeof(s_sms_text)))
    {
        App_Monitor_TransitionTo(APP_MONITOR_STATE_SLEEP, now_tick_ms);
        return;
    }

    /* 保存本次短信类别；仅模块最终确认发送成功后才更新 MOTION 冷却时间。 */
    s_current_sms_is_motion = is_motion;

    App_Monitor_TransitionTo(APP_MONITOR_STATE_SMS_REQUEST, now_tick_ms);
}

static void App_Monitor_HandleSmsRequest(uint32_t now_tick_ms)
{
    if (ML307C_RequestSms(s_sms_text, now_tick_ms))
    {
        App_Monitor_WriteDebug(s_sms_requested_message, sizeof(s_sms_requested_message) - 1U);
        App_Monitor_TransitionTo(APP_MONITOR_STATE_WAIT_SMS_RESULT, now_tick_ms);
    }
    else if ((uint32_t)(now_tick_ms - s_state_enter_tick_ms) >= APP_SMS_REQUEST_TIMEOUT_MS)
    {
        App_Monitor_WriteDebug(s_sms_request_timeout_message, sizeof(s_sms_request_timeout_message) - 1U);
        App_Monitor_TransitionTo(APP_MONITOR_STATE_SLEEP, now_tick_ms);
    }
}

static void App_Monitor_HandleWaitSmsResult(uint32_t now_tick_ms)
{
    ML307C_SmsResult_t result = ML307C_ConsumeResult();

    if (result == ML307C_SMS_RESULT_SUCCESS)
    {
        if (s_current_sms_is_motion && App_Monitor_Port_GetRtcSeconds(&s_last_motion_sms_rtc_seconds))
        {
            s_last_motion_sms_time_valid = true;
        }

        App_Monitor_WriteDebug(s_sms_success_message, sizeof(s_sms_success_message) - 1U);
        App_Monitor_TransitionTo(APP_MONITOR_STATE_SLEEP, now_tick_ms);
    }
    else if (result == ML307C_SMS_RESULT_FAILED)
    {
        App_Monitor_WriteDebug(s_sms_failed_message, sizeof(s_sms_failed_message) - 1U);
        App_Monitor_TransitionTo(APP_MONITOR_STATE_SLEEP, now_tick_ms);
    }
}

static void App_Monitor_HandleSleep(uint32_t now_tick_ms)
{
    if (s_next_alarm_required)
    {
        if (!App_Monitor_Port_SetAlarmAfterSeconds(APP_RTC_WAKEUP_INTERVAL_SECONDS))
        {
            App_Monitor_WriteDebug(s_alarm_set_failed_message, sizeof(s_alarm_set_failed_message) - 1U);
            App_Monitor_TransitionTo(APP_MONITOR_STATE_IDLE, now_tick_ms);
            return;
        }

        s_next_alarm_required = false;
    }

    if (APP_MONITOR_STOP_ENABLED != 0U)
    {
        App_Monitor_WriteDebug(s_enter_stop_message, sizeof(s_enter_stop_message) - 1U);
        App_Monitor_TransitionTo(APP_MONITOR_STATE_IDLE, now_tick_ms);
        App_Monitor_Port_EnterStopMode();
    }
    else
    {
        App_Monitor_TransitionTo(APP_MONITOR_STATE_IDLE, now_tick_ms);
    }
}

void App_Monitor_Init(void)
{
    s_rtc_alarm_pending = false;
    s_motion_pending = false;
    s_first_cycle_pending = true;
    s_next_alarm_required = true;
    s_last_motion_sms_time_valid = false;
    s_last_motion_sms_rtc_seconds = 0U;
    s_current_sms_is_motion = false;
    s_sms_text[0] = '\0';
    App_Monitor_TransitionTo(APP_MONITOR_STATE_IDLE, 0U);
}

void App_Monitor_Update(uint32_t now_tick_ms, bool motion_active)
{
    switch (s_state)
    {
        case APP_MONITOR_STATE_IDLE:            App_Monitor_HandleIdle(now_tick_ms); break;
        case APP_MONITOR_STATE_MOTION_SETTLE:   App_Monitor_HandleMotionSettle(now_tick_ms, motion_active); break;
        case APP_MONITOR_STATE_MOTION_CONFIRM:  App_Monitor_HandleMotionConfirm(now_tick_ms, motion_active); break;
        case APP_MONITOR_STATE_PERIODIC_SAMPLE: App_Monitor_HandleSample(now_tick_ms, false); break;
        case APP_MONITOR_STATE_MOTION_SAMPLE:   App_Monitor_HandleSample(now_tick_ms, true); break;
        case APP_MONITOR_STATE_SMS_REQUEST:     App_Monitor_HandleSmsRequest(now_tick_ms); break;
        case APP_MONITOR_STATE_WAIT_SMS_RESULT: App_Monitor_HandleWaitSmsResult(now_tick_ms); break;
        case APP_MONITOR_STATE_SLEEP:           App_Monitor_HandleSleep(now_tick_ms); break;
        default: App_Monitor_TransitionTo(APP_MONITOR_STATE_IDLE, now_tick_ms); break;
    }
}

void App_Monitor_OnRtcAlarm(void)
{
    s_rtc_alarm_pending = true;
}

void App_Monitor_OnMotion(void)
{
    s_motion_pending = true;
}
