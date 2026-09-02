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

static void ML307C_SendCommand(const uint8_t *command, uint16_t size,
                                ML307C_State_t next_state, uint32_t now_tick_ms)
{
    (void)ML307C_Port_Write(command, size);
    s_line_size = 0U;
    s_state_tick_ms = now_tick_ms;
    s_state = next_state;
}

static void ML307C_FinishSms(ML307C_SmsResult_t result)
{
    s_sms_result = result;
    s_state = ML307C_STATE_READY;
    s_line_size = 0U;
}

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

bool ML307C_Init(uint32_t now_tick_ms)
{
    s_state_tick_ms = now_tick_ms;
    s_line_size = 0U;
    s_sms_text[0] = '\0';
    s_sms_result = ML307C_SMS_RESULT_NONE;
    s_state = ML307C_STATE_WAIT_BOOT;

    return ML307C_Port_StartReceive();
}

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

uint16_t ML307C_ReadRaw(uint8_t *data, uint16_t max_size)
{
    return ML307C_Port_Read(data, max_size);
}

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

bool ML307C_IsBusy(void)
{
    return (s_state >= ML307C_STATE_WAIT_CMGF_OK) && (s_state <= ML307C_STATE_WAIT_SMS_RESULT);
}

ML307C_SmsResult_t ML307C_ConsumeResult(void)
{
    ML307C_SmsResult_t result = s_sms_result;

    s_sms_result = ML307C_SMS_RESULT_NONE;
    return result;
}
