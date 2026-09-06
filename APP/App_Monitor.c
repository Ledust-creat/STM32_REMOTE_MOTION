#include "App_Monitor.h"

#include <stdio.h>

#include "App_Monitor_Config.h"
#include "App_Monitor_Port.h"
#include "ML307C.h"
#include "SHT30.h"

/*
*********************************************************************************************************
*   类型定义
*********************************************************************************************************
*/

/* 监测业务状态；状态切换仅由本模块完成，外部中断只投递待处理事件。 */
typedef enum
{
    APP_MONITOR_STATE_IDLE = 0U,        /* 等待 RTC 或雷达事件，不执行采样与短信业务。 */
    APP_MONITOR_STATE_MOTION_SETTLE,    /* 雷达上升沿后的短暂稳定期，用于滤除插拔和边沿抖动。 */
    APP_MONITOR_STATE_MOTION_CONFIRM,   /* 雷达持续高电平确认期，满足保持时间后才认定为有效运动。 */
    APP_MONITOR_STATE_PERIODIC_SAMPLE,  /* RTC 周期唤醒后的温湿度采样。 */
    APP_MONITOR_STATE_MOTION_SAMPLE,    /* 有效运动确认后的温湿度采样。 */
    APP_MONITOR_STATE_SMS_REQUEST,      /* 向 ML307C 提交短信请求，等待通信服务接受。 */
    APP_MONITOR_STATE_WAIT_SMS_RESULT,  /* 已提交短信，等待 ML307C 非阻塞服务给出最终结果。 */
    APP_MONITOR_STATE_SLEEP              /* 配置下一次 RTC 告警后进入 STOP 或返回空闲。 */
} App_Monitor_State_t;

/*
*********************************************************************************************************
*   模块变量
*********************************************************************************************************
*/

static volatile bool s_rtc_alarm_pending;              /* RTC 中断置位；由主循环清除并处理。 */
static volatile bool s_motion_pending;                 /* 雷达上升沿中断置位；不在 ISR 内做确认或发短信。 */
static App_Monitor_State_t s_state;                     /* 当前业务状态，仅由主循环状态机写入。 */
static uint32_t s_state_enter_tick_ms;                 /* 当前状态进入时刻，单位ms，用于无符号差值超时判断。 */
static bool s_first_cycle_pending;                      /* 上电后仅打印启动信息一次，随后立即建立休眠节奏。 */
static bool s_next_alarm_required;                     /* true 表示进入休眠前必须设置下一次 RTC 周期告警。 */
static bool s_last_motion_sms_time_valid;               /* 运动短信成功发送后，冷却时间基准才有效。 */
static uint32_t s_last_motion_sms_rtc_seconds;          /* 最近一次运动短信成功时的 RTC 当日秒数。 */
static bool s_current_sms_is_motion;                    /* 当前等待结果的短信是否属于运动事件。 */
static char s_sms_text[APP_SMS_TEXT_MAX_SIZE + 1U];     /* 待发送 ASCII 短信缓存，由 APP 独占。 */

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

/*
*********************************************************************************************************
*   函数声明
*********************************************************************************************************
*/

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

/*
*********************************************************************************************************
*   函数定义
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*   函 数 名: App_Monitor_TransitionTo
*   功能说明: 统一切换业务状态，并记录新状态的进入时刻
*   形    参: next_state  - 目标状态
*             now_tick_ms - 当前系统节拍，单位ms
*   返 回 值: 无
*********************************************************************************************************
*/
static void App_Monitor_TransitionTo(App_Monitor_State_t next_state, uint32_t now_tick_ms)
{
    s_state = next_state;
    s_state_enter_tick_ms = now_tick_ms;
}

/* 调试日志仅在主循环状态切换处输出，不应从中断上下文调用。 */
static void App_Monitor_WriteDebug(const uint8_t *data, uint16_t size)
{
    if (APP_MONITOR_DEBUG_ENABLED != 0U)
    {
        App_Monitor_Port_WriteDebug(data, size);
    }
}

/*
*********************************************************************************************************
*   函 数 名: App_Monitor_IsMotionCooldownActive
*   功能说明: 判断运动短信是否仍处于冷却窗口，避免持续运动导致重复短信
*   形    参: 无
*   返 回 值: true表示仍在冷却期；RTC 时间不可用时返回false，以避免阻断告警
*********************************************************************************************************
*/
static bool App_Monitor_IsMotionCooldownActive(void)
{
    uint32_t now_rtc_seconds;
    uint32_t elapsed_seconds;

    if ((!s_last_motion_sms_time_valid) || !App_Monitor_Port_GetRtcSeconds(&now_rtc_seconds))
    {
        return false;
    }

    /* RTC 仅保存当日秒数；模运算保证跨越午夜时仍能得到正确的冷却间隔。 */
    elapsed_seconds = (now_rtc_seconds + 86400U - s_last_motion_sms_rtc_seconds) % 86400U;
    return elapsed_seconds < APP_MOTION_SMS_COOLDOWN_SECONDS;
}

/* 首次循环建立低功耗节奏；之后按 RTC 优先、雷达事件次之的顺序消费待处理事件。 */
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

/*
* 雷达上升沿后先等待边沿稳定时间。该阶段只读当前电平，避免杜邦线插拔、EMI
* 或短脉冲直接进入告警确认流程。
*/
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

/* 雷达在整个确认窗口内保持高电平才视为有效运动；低电平立即取消并重新休眠。 */
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

/*
*********************************************************************************************************
*   函 数 名: App_Monitor_HandleSample
*   功能说明: 读取一次温湿度并生成本次告警或周期上报的 ASCII 短信正文
*   形    参: now_tick_ms - 当前系统节拍，单位ms
*             is_motion   - true表示运动事件；false表示 RTC 周期上报
*   返 回 值: 无；读取或格式化失败时仍生成明确的故障短信，缓存越界则直接休眠
*********************************************************************************************************
*/
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

/* 非阻塞提交短信请求；模块忙时持续等待，但超过配置时间不得阻塞下一轮休眠。 */
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

/*
* 仅消费一次 ML307C 的最终结果。运动短信只有成功返回后才写入冷却基准，
* 发送失败不更新冷却时间，保证下一个有效事件仍可再次尝试上报。
*/
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

/*
* 进入 STOP 前由 APP 负责设置下一次 RTC Alarm。若设置失败则回到 IDLE，
* 避免在没有周期唤醒源的情况下进入低功耗模式。
*/
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

/*
*********************************************************************************************************
*   函 数 名: App_Monitor_Init
*   功能说明: 初始化监测业务状态；不访问硬件，硬件初始化由 main 与各 Port 层完成
*   形    参: 无
*   返 回 值: 无
*********************************************************************************************************
*/
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

/*
*********************************************************************************************************
*   函 数 名: App_Monitor_Update
*   功能说明: 在主循环中执行一次监测业务状态机
*   形    参: now_tick_ms   - 当前系统节拍，单位ms
*             motion_active - 雷达 OUT 当前逻辑电平，true表示检测到运动
*   返 回 值: 无
*   注    意: 不应在中断中调用；ISR 仅通过 OnRtcAlarm/OnMotion 投递事件。
*********************************************************************************************************
*/
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

/* RTC Alarm ISR 的延后处理入口：只置位事件标志，采样和短信发送留给主循环。 */
void App_Monitor_OnRtcAlarm(void)
{
    s_rtc_alarm_pending = true;
}

/* 雷达 OUT 上升沿 ISR 的延后处理入口：实际高电平确认由状态机完成。 */
void App_Monitor_OnMotion(void)
{
    s_motion_pending = true;
}
