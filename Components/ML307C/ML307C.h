#ifndef ML307C_H
#define ML307C_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    ML307C_SMS_RESULT_NONE = 0U,    /* 尚无已完成的短信请求。 */
    ML307C_SMS_RESULT_SUCCESS,      /* 模块返回 +CMGS 且最终返回 OK。 */
    ML307C_SMS_RESULT_FAILED        /* 模块返回 ERROR 或任一步骤超时。 */
} ML307C_SmsResult_t;

bool ML307C_Init(uint32_t now_tick_ms);
void ML307C_Update(uint32_t now_tick_ms);
void ML307C_OnReceive(const uint8_t *data, uint16_t size, uint32_t now_tick_ms);
uint16_t ML307C_ReadRaw(uint8_t *data, uint16_t max_size);
bool ML307C_RequestSms(const char *text, uint32_t now_tick_ms);
bool ML307C_IsBusy(void);
ML307C_SmsResult_t ML307C_ConsumeResult(void);

#endif /* ML307C_H */
