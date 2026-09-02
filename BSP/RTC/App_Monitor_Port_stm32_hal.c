#include "App_Monitor_Port.h"

#include "App_Monitor_Config.h"

#include "main.h"
#include "rtc.h"

#include "BSP_UART.h"

extern void SystemClock_Config(void);

/*
*********************************************************************************************************
*   函 数 名: App_Monitor_Port_SetAlarmAfterSeconds
*   功能说明: 读取当前 RTC 时间，计算相对秒数后的 Alarm A 时间并启用 Alarm 中断。
*   形    参: seconds - 相对唤醒延迟，单位为秒，最大值由 APP_RTC_WAKEUP_MAX_SECONDS 配置。
*   返 回 值: true 表示 Alarm 已启用；false 表示参数或 HAL 操作失败。
*********************************************************************************************************
*/
bool App_Monitor_Port_SetAlarmAfterSeconds(uint32_t seconds)
{
    RTC_TimeTypeDef current_time = {0};
    RTC_DateTypeDef current_date = {0};
    RTC_AlarmTypeDef alarm = {0};
    uint32_t current_seconds;
    uint32_t alarm_seconds;

    if ((seconds == 0U) || (seconds > APP_RTC_WAKEUP_MAX_SECONDS))
    {
        return false;
    }

    if (HAL_RTC_GetTime(&hrtc, &current_time, RTC_FORMAT_BIN) != HAL_OK)
    {
        return false;
    }

    /* F1 RTC 读取时间后必须读取日期，才能正确解除时间寄存器影子锁存。 */
    if (HAL_RTC_GetDate(&hrtc, &current_date, RTC_FORMAT_BIN) != HAL_OK)
    {
        return false;
    }

    current_seconds = ((uint32_t)current_time.Hours * 3600U) +
                      ((uint32_t)current_time.Minutes * 60U) +
                      (uint32_t)current_time.Seconds;
    alarm_seconds = (current_seconds + seconds) % 86400U;

    alarm.Alarm = RTC_ALARM_A;
    alarm.AlarmTime.Hours = (uint8_t)(alarm_seconds / 3600U);
    alarm.AlarmTime.Minutes = (uint8_t)((alarm_seconds % 3600U) / 60U);
    alarm.AlarmTime.Seconds = (uint8_t)(alarm_seconds % 60U);

    (void)HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A);
    return HAL_RTC_SetAlarm_IT(&hrtc, &alarm, RTC_FORMAT_BIN) == HAL_OK;
}

/*
*********************************************************************************************************
*   函 数 名: App_Monitor_Port_GetRtcSeconds
*   功能说明: 获取当天已过秒数；用于跨 STOP 的短信冷却时间，不能使用会在 STOP 停止的 SysTick。
*   形    参: seconds - 输出当天 0 至 86399 秒。
*   返 回 值: true 表示读取成功；false 表示参数或 HAL 操作失败。
*********************************************************************************************************
*/
bool App_Monitor_Port_GetRtcSeconds(uint32_t *seconds)
{
    RTC_TimeTypeDef current_time = {0};
    RTC_DateTypeDef current_date = {0};

    if (seconds == NULL)
    {
        return false;
    }

    if (HAL_RTC_GetTime(&hrtc, &current_time, RTC_FORMAT_BIN) != HAL_OK)
    {
        return false;
    }

    if (HAL_RTC_GetDate(&hrtc, &current_date, RTC_FORMAT_BIN) != HAL_OK)
    {
        return false;
    }

    *seconds = ((uint32_t)current_time.Hours * 3600U) +
               ((uint32_t)current_time.Minutes * 60U) +
               (uint32_t)current_time.Seconds;
    return true;
}

/*
*********************************************************************************************************
*   函 数 名: App_Monitor_Port_EnterStopMode
*   功能说明: 进入 STOP；RTC Alarm 或已配置的 EXTI 可唤醒，返回后恢复 HSE/PLL 系统时钟。
*   返 回 值: 无
*********************************************************************************************************
*/
void App_Monitor_Port_EnterStopMode(void)
{
    /* SysTick 是周期中断，暂停后可避免它在 WFI 前已挂起而导致立即返回。 */
    HAL_SuspendTick();
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
	
    /* 先恢复 HAL 超时计时，再恢复 STOP 后已停止的 HSE/PLL 时钟树。 */
    HAL_ResumeTick();
    SystemClock_Config();
}

/*
*********************************************************************************************************
*   函 数 名: App_Monitor_Port_WriteDebug
*   功能说明: 向 USART1 调试口输出 APP 日志；不允许由中断上下文调用。
*   形    参: data - 输出字节；size - 字节数。
*   返 回 值: 无
*********************************************************************************************************
*/
void App_Monitor_Port_WriteDebug(const uint8_t *data, uint16_t size)
{
    (void)BSP_UART_Write(BSP_UART_CHANNEL_DEBUG, data, size);
}
