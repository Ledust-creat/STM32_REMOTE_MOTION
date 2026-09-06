#ifndef APP_MONITOR_CONFIG_H
#define APP_MONITOR_CONFIG_H

/*
*********************************************************************************************************
*   定时唤醒配置
*********************************************************************************************************
*/

#define APP_RTC_WAKEUP_INTERVAL_SECONDS  (120U)    /* RTC 定时唤醒周期；验证时使用 10 秒，正式日上报改为 86400U。 */
#define APP_RTC_WAKEUP_MAX_SECONDS       (86400U) /* 单次 RTC Alarm 最大允许延迟；当前按一天设计，修改周期时不得超过此值。 */
#define APP_MOTION_EDGE_SETTLE_MS         (100U)   /* RCWL 上升沿后的电平稳定等待时间；过滤插线和短导线接触抖动。 */
#define APP_MOTION_CONFIRM_HOLD_MS        (5000U)  /* RCWL 上升沿后 OUT 必须连续保持高电平的确认时长；验证时可先改为 3000U。 */
#define APP_MOTION_SMS_COOLDOWN_SECONDS   (300U)   /* MOTION 短信发送后的冷却时间；冷却内确认侵入仅记录日志，不重复发短信。 */
#define APP_SMS_REQUEST_TIMEOUT_MS        (15000U) /* 等待 ML307C 就绪并接受短信请求的最大时间；超时后放弃本轮并休眠。 */
#define APP_SMS_TEXT_MAX_SIZE              (96U)    /* APP 生成的英文 ASCII 短信正文最大字节数，不含字符串结束符。 */
#define APP_MONITOR_STOP_ENABLED          (1U)     /* STOP 功能总开关；1：每次配置闹钟后进入 STOP；0：只验证闹钟配置。 */
#define APP_MONITOR_DEBUG_ENABLED         (1U)     /* 串口调试开关；低功耗实测完成后可设为 0，避免日志增加运行时间。 */

#endif /* APP_MONITOR_CONFIG_H */
