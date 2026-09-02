#include "SHT30.h"

#include <stddef.h>

#include "SHT30_Port.h"

/*
*********************************************************************************************************
*   宏定义
*********************************************************************************************************
*/

#define SHT30_CMD_SOFT_RESET_MSB       (0x30U)
#define SHT30_CMD_SOFT_RESET_LSB       (0xA2U)
#define SHT30_CMD_SINGLE_HIGH_MSB      (0x24U)
#define SHT30_CMD_SINGLE_HIGH_LSB      (0x00U)
#define SHT30_RESET_DELAY_MS           (2U)
#define SHT30_MEASURE_DELAY_MS         (15U)  /* 高重复性测量的最大转换时间 */
#define SHT30_CRC_INITIAL_VALUE        (0xFFU)
#define SHT30_CRC_POLYNOMIAL           (0x31U)

/*
*********************************************************************************************************
*   函数定义
*********************************************************************************************************
*/

static uint8_t SHT30_CalculateCrc(const uint8_t *data, uint8_t size)
{
    uint8_t crc = SHT30_CRC_INITIAL_VALUE;
    uint8_t index;
    uint8_t bit;

    for (index = 0U; index < size; index++)
    {
        crc ^= data[index];

        for (bit = 0U; bit < 8U; bit++)
        {
            crc = ((crc & 0x80U) != 0U) ? (uint8_t)((crc << 1U) ^ SHT30_CRC_POLYNOMIAL)
                                         : (uint8_t)(crc << 1U);
        }
    }

    return crc;
}

/*
*********************************************************************************************************
*   函 数 名: SHT30_Init
*   功能说明: 复位传感器并等待其重新进入 I2C 空闲状态
*   返 回 值: true 表示复位命令已应答；false 表示 I2C 通信失败
*********************************************************************************************************
*/
bool SHT30_Init(void)
{
    const uint8_t command[2] = {SHT30_CMD_SOFT_RESET_MSB, SHT30_CMD_SOFT_RESET_LSB};

    if (!SHT30_Port_Write(SHT30_I2C_ADDRESS_7BIT, command, sizeof(command)))
    {
        return false;
    }

    SHT30_Port_DelayMs(SHT30_RESET_DELAY_MS);
    return true;
}

/*
*********************************************************************************************************
*   函 数 名: SHT30_ReadSingleShot
*   功能说明: 采集一次高重复性温湿度数据
*   形    参: sample - 调用者提供的输出快照；所有失败路径都会清除 valid 标志
*   返 回 值: 仅当 I2C 通信和两组接收 CRC 均正确时返回 true
*********************************************************************************************************
*/
bool SHT30_ReadSingleShot(SHT30_Sample_t *sample)
{
    const uint8_t command[2] = {SHT30_CMD_SINGLE_HIGH_MSB, SHT30_CMD_SINGLE_HIGH_LSB};
    uint8_t response[6];
    uint16_t raw_temperature;
    uint16_t raw_humidity;

    if (sample == NULL)
    {
        return false;
    }

    sample->valid = false;

    if (!SHT30_Port_Write(SHT30_I2C_ADDRESS_7BIT, command, sizeof(command)))
    {
        return false;
    }

    SHT30_Port_DelayMs(SHT30_MEASURE_DELAY_MS);

    if (!SHT30_Port_Read(SHT30_I2C_ADDRESS_7BIT, response, sizeof(response)))
    {
        return false;
    }

    if ((SHT30_CalculateCrc(&response[0], 2U) != response[2]) ||
        (SHT30_CalculateCrc(&response[3], 2U) != response[5]))
    {
        return false;
    }

    raw_temperature = ((uint16_t)response[0] << 8U) | response[1];
    raw_humidity = ((uint16_t)response[3] << 8U) | response[4];

    /* T=-45+175*raw/65535、RH=100*raw/65535；结果统一保存为 0.01 单位。 */
    sample->temperature_centi_c = (int16_t)(-4500L + (((int32_t)raw_temperature * 17500L + 32767L) / 65535L));
    sample->humidity_centi_percent = (uint16_t)(((uint32_t)raw_humidity * 10000UL + 32767UL) / 65535UL);
    sample->sample_tick_ms = SHT30_Port_GetTickMs();
    sample->valid = true;

    return true;
}
