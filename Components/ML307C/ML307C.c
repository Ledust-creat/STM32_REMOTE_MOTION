#include "ML307C.h"

#include <string.h>

#include "ML307C_Config.h"
#include "ML307C_Port.h"

typedef enum
{
    ML307C_STATE_WAIT_BOOT = 0U,
    ML307C_STATE_WAIT_AT_OK,
    ML307C_STATE_READY,
    ML307C_STATE_WAIT_CMGF_OK,
    ML307C_STATE_WAIT_CSMP_OK,
    ML307C_STATE_WAIT_CMGS_PROMPT,
    ML307C_STATE_WAIT_SMS_RESULT
} ML307C_State_t;

static ML307C_State_t s_state;
static uint32_t s_state_tick_ms;
static uint8_t s_line_buffer[64];
static uint8_t s_line_size;
static char s_sms_text[ML307C_SMS_TEXT_MAX_SIZE + 1U];
static ML307C_SmsResult_t s_sms_result;

static const uint8_t s_at_command[] = "AT\r\n";
static const uint8_t s_cmgf_command[] = "AT+CMGF=1\r\n";
static const uint8_t s_csmp_command[] = "AT+CSMP=33,167,0,0\r\n";
static const uint8_t s_cmgs_command[] = "AT+CMGS=\"" MODEM_SMS_TARGET_PHONE "\"\r\n";
static const uint8_t s_sms_end_byte = ML307C_SMS_END_BYTE;

/*
*********************************************************************************************************
*   函 数 名: ML307C_SendCommand
*
*   功能说明:
*       发送一条AT指令，并切换状态机等待对应响应
*
*   形    参:
*       command       - AT指令字符串
*       size          - 指令长度
*       next_state    - 发送后等待状态
*       now_tick_ms   - 当前系统时间
*
*   返 回 值:
*       无
*********************************************************************************************************
*/
static void ML307C_SendCommand(const uint8_t *command, uint16_t size,
                                ML307C_State_t next_state, uint32_t now_tick_ms)
{
    (void)ML307C_Port_Write(command, size);
    s_line_size = 0U;
    s_state_tick_ms = now_tick_ms;
    s_state = next_state;
}
/*
*********************************************************************************************************
*   函 数 名: ML307C_FinishSms
*
*   功能说明:
*       完成短信发送流程，保存结果并恢复空闲状态
*
*   形    参:
*       result - 短信发送结果
*
*   返 回 值:
*       无
*********************************************************************************************************
*/
static void ML307C_FinishSms(ML307C_SmsResult_t result)
{
    s_sms_result = result;
    s_state = ML307C_STATE_READY;
    s_line_size = 0U;
}
/*
*********************************************************************************************************
*   函 数 名: ML307C_ProcessLine
*
*   功能说明:
*       处理模块返回的一行AT响应数据
*
*   处理内容:
*       1. 判断ERROR错误
*       2. 判断OK响应
*       3. 根据当前状态推进状态机
*
*   形    参:
*       now_tick_ms - 当前系统时间
*
*   返 回 值:
*       无
*********************************************************************************************************
*/
static void ML307C_ProcessLine(uint32_t now_tick_ms)
{
    if ((s_line_size == 5U) && (memcmp(s_line_buffer, "ERROR", 5U) == 0))
    {
        if (ML307C_IsBusy())
        {
            ML307C_FinishSms(ML307C_SMS_RESULT_FAILED);
        }
    }
    else if ((s_line_size == 2U) && (memcmp(s_line_buffer, "OK", 2U) == 0))
    {
        if (s_state == ML307C_STATE_WAIT_AT_OK)
        {
            s_state = ML307C_STATE_READY;

            /* 保留独立模块自测入口；正式 APP 运行时该宏默认关闭。 */
            if ((MODEM_SMS_ENABLED != 0U) && (MODEM_SMS_TEST_ON_BOOT_ENABLED != 0U))
            {
                (void)ML307C_RequestSms(MODEM_SMS_TEST_TEXT, now_tick_ms);
            }
        }
        else if (s_state == ML307C_STATE_WAIT_CMGF_OK)
        {
            ML307C_SendCommand(s_csmp_command, sizeof(s_csmp_command) - 1U,
                                ML307C_STATE_WAIT_CSMP_OK, now_tick_ms);
        }
        else if (s_state == ML307C_STATE_WAIT_CSMP_OK)
        {
            ML307C_SendCommand(s_cmgs_command, sizeof(s_cmgs_command) - 1U,
                                ML307C_STATE_WAIT_CMGS_PROMPT, now_tick_ms);
        }
        else if (s_state == ML307C_STATE_WAIT_SMS_RESULT)
        {
            ML307C_FinishSms(ML307C_SMS_RESULT_SUCCESS);
        }
    }

    s_line_size = 0U;
}
/*
*********************************************************************************************************
*   函 数 名: ML307C_Init
*
*   功能说明:
*       初始化ML307C模块驱动状态机
*
*   初始化内容:
*       1. 初始化状态计时
*       2. 清空接收缓存
*       3. 清空短信缓存
*       4. 设置模块等待启动状态
*       5. 开启串口接收
*
*   形    参:
*       now_tick_ms - 当前系统时间
*
*   返 回 值:
*       true  - 串口接收启动成功
*       false - 启动失败
*********************************************************************************************************
*/
bool ML307C_Init(uint32_t now_tick_ms)
{
    s_state_tick_ms = now_tick_ms;
    s_line_size = 0U;
    s_sms_text[0] = '\0';
    s_sms_result = ML307C_SMS_RESULT_NONE;
    s_state = ML307C_STATE_WAIT_BOOT;

    return ML307C_Port_StartReceive();
}
/*
*********************************************************************************************************
*   函 数 名: ML307C_Update
*
*   功能说明:
*       ML307C状态机周期任务
*
*   主要功能:
*       1. 等待模块启动完成
*       2. 发送AT测试指令
*       3. 处理各阶段超时
*       4. 防止模块状态机永久阻塞
*
*   形    参:
*       now_tick_ms - 当前系统时间
*
*   返 回 值:
*       无
*********************************************************************************************************
*/
void ML307C_Update(uint32_t now_tick_ms)
{
    if ((s_state == ML307C_STATE_WAIT_BOOT) &&
        ((uint32_t)(now_tick_ms - s_state_tick_ms) >= ML307C_BOOT_WAIT_MS))
    {
        ML307C_SendCommand(s_at_command, sizeof(s_at_command) - 1U,
                            ML307C_STATE_WAIT_AT_OK, now_tick_ms);
    }
    else if ((s_state == ML307C_STATE_WAIT_AT_OK) ||
             ((s_state >= ML307C_STATE_WAIT_CMGF_OK) && (s_state <= ML307C_STATE_WAIT_SMS_RESULT)))
    {
        if ((uint32_t)(now_tick_ms - s_state_tick_ms) >= ML307C_COMMAND_TIMEOUT_MS)
        {
            if (s_state == ML307C_STATE_WAIT_AT_OK)
            {
                s_state = ML307C_STATE_WAIT_BOOT;
                s_state_tick_ms = now_tick_ms;
            }
            else
            {
                ML307C_FinishSms(ML307C_SMS_RESULT_FAILED);
            }
        }
    }
}
/*
*********************************************************************************************************
*   函 数 名: ML307C_OnReceive
*
*   功能说明:
*       ML307C串口接收数据处理函数
*
*   处理内容:
*       1. 检测短信输入提示符 >
*       2. 发送短信正文
*       3. 发送Ctrl+Z结束符
*       4. 按行解析AT返回结果
*
*   形    参:
*       data        - 接收到的数据
*       size        - 数据长度
*       now_tick_ms - 当前系统时间
*
*   返 回 值:
*       无
*********************************************************************************************************
*/
void ML307C_OnReceive(const uint8_t *data, uint16_t size, uint32_t now_tick_ms)
{
    uint16_t index;

    for (index = 0U; index < size; index++)
    {
        if ((data[index] == (uint8_t)'>') && (s_state == ML307C_STATE_WAIT_CMGS_PROMPT))
        {
            (void)ML307C_Port_Write((const uint8_t *)s_sms_text, (uint16_t)strlen(s_sms_text));
            (void)ML307C_Port_Write(&s_sms_end_byte, 1U);
            s_line_size = 0U;
            s_state_tick_ms = now_tick_ms;
            s_state = ML307C_STATE_WAIT_SMS_RESULT;
        }
        else if (data[index] == (uint8_t)'\n')
        {
            ML307C_ProcessLine(now_tick_ms);
        }
        else if (data[index] != (uint8_t)'\r')
        {
            if (s_line_size < sizeof(s_line_buffer))
            {
                s_line_buffer[s_line_size++] = data[index];
            }
            else
            {
                s_line_size = 0U;
            }
        }
    }
}
/*
*********************************************************************************************************
*   函 数 名: ML307C_ReadRaw
*
*   功能说明:
*       读取底层串口缓存中的原始数据
*
*   说明:
*       该接口主要用于调试或者特殊数据透传场景。
*       正常短信流程使用ML307C_OnReceive解析。
*
*   形    参:
*       data      - 数据存储缓存
*       max_size  - 最大读取长度
*
*   返 回 值:
*       实际读取的数据长度
*********************************************************************************************************
*/
uint16_t ML307C_ReadRaw(uint8_t *data, uint16_t max_size)
{
    return ML307C_Port_Read(data, max_size);
}
/*
*********************************************************************************************************
*   函 数 名: ML307C_RequestSms
*
*   功能说明:
*       请求发送一条短信
*
*   执行流程:
*
*       1. 判断短信功能是否开启
*       2. 判断参数是否合法
*       3. 保存短信内容
*       4. 设置短信模式
*       5. 由状态机继续完成:
*
*          CMGF
*             ↓
*          CSMP
*             ↓
*          CMGS
*             ↓
*          输入正文
*             ↓
*          Ctrl+Z发送
*
*
*   形    参:
*       text          - 短信正文
*       now_tick_ms   - 当前系统时间
*
*   返 回 值:
*       true  - 请求成功
*       false - 请求失败
*********************************************************************************************************
*/
bool ML307C_RequestSms(const char *text, uint32_t now_tick_ms)
{
    uint16_t text_size;

    if ((MODEM_SMS_ENABLED == 0U) || (text == NULL) || (s_state != ML307C_STATE_READY))
    {
        return false;
    }

    text_size = (uint16_t)strlen(text);
    if ((text_size == 0U) || (text_size > ML307C_SMS_TEXT_MAX_SIZE))
    {
        return false;
    }

    memcpy(s_sms_text, text, text_size + 1U);
    s_sms_result = ML307C_SMS_RESULT_NONE;
    ML307C_SendCommand(s_cmgf_command, sizeof(s_cmgf_command) - 1U,
                        ML307C_STATE_WAIT_CMGF_OK, now_tick_ms);
    return true;
}
/*
*********************************************************************************************************
*   函 数 名: ML307C_IsBusy
*
*   功能说明:
*       判断模块当前是否正在执行短信发送流程
*
*   判断范围:
*
*       WAIT_CMGF_OK
*           ↓
*       WAIT_SMS_RESULT
*
*   说明:
*       短信发送过程中禁止重复发送请求。
*
*   返 回 值:
*       true  - 正在发送短信
*       false - 空闲
*********************************************************************************************************
*/
bool ML307C_IsBusy(void)
{
    return (s_state >= ML307C_STATE_WAIT_CMGF_OK) && (s_state <= ML307C_STATE_WAIT_SMS_RESULT);
}
/*
*********************************************************************************************************
*   函 数 名: ML307C_ConsumeResult
*
*   功能说明:
*       获取短信发送结果，并清除结果状态
*
*   说明:
*       采用一次性消费机制:
*
*       调用一次:
*           返回当前结果
*
*       随后:
*           清除结果等待下一次发送
*
*   返 回 值:
*       ML307C_SmsResult_t
*********************************************************************************************************
*/
ML307C_SmsResult_t ML307C_ConsumeResult(void)
{
    ML307C_SmsResult_t result = s_sms_result;

    s_sms_result = ML307C_SMS_RESULT_NONE;
    return result;
}
