#ifndef ML307C_CONFIG_H
#define ML307C_CONFIG_H

/*
*********************************************************************************************************
*   模块配置
*********************************************************************************************************
*/

#define MODEM_SMS_TARGET_PHONE       "13192312285"     /* 短信接收号码；修改此宏即可更换测试目标。 */
#define MODEM_SMS_TEST_TEXT          "ML307C SMS test" /* 本阶段一次性测试短信内容。 */
#define MODEM_SMS_ENABLED             (1U)             /* 短信服务总开关；后续正式告警功能使用此宏。 */
#define MODEM_SMS_TEST_ON_BOOT_ENABLED (0U)            /* 1：上电自动发送一条测试短信；APP 短信服务运行时保持 0。 */
#define MODEM_LOW_POWER_ENABLED      (0U)              /* 当前开发板无可控 DTR/安全 EN 电路，禁止宣称已低功耗 */
#define ML307C_DEBUG_ENABLED          (1U)             /* 1：USART1 镜像 AT 指令和模块原始回传；0：关闭镜像，不影响真实短信流程。 */
#define ML307C_BOOT_WAIT_MS          (5000U)           /* 上电后首次发送 AT 前等待模块启动完成 */
#define ML307C_COMMAND_TIMEOUT_MS    (10000U)          /* 单条 AT 命令的最大等待时间。 */
#define ML307C_SMS_END_BYTE          (0x1AU)           /* SMS 正文结束符：Ctrl+Z，不能发送字符 '.'。 */
#define ML307C_SMS_TEXT_MAX_SIZE      (96U)             /* 单次英文 ASCII 短信正文最大字节数，不含字符串结束符。 */

#endif /* ML307C_CONFIG_H */
