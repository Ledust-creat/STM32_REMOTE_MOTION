#include "SHT30_Port.h"

#include <stddef.h>

#include "i2c.h"

/*
*********************************************************************************************************
*   Macro definitions
*********************************************************************************************************
*/

#define SHT30_PORT_I2C_TIMEOUT_MS    (100U)  /* Bounded blocking transfer timeout */

/*
*********************************************************************************************************
*   Function definitions
*********************************************************************************************************
*/

bool SHT30_Port_Write(uint8_t address7, const uint8_t *data, uint16_t size)
{
    if ((data == NULL) || (size == 0U))
    {
        return false;
    }

    return HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)(address7 << 1U),
                                   (uint8_t *)data, size,
                                   SHT30_PORT_I2C_TIMEOUT_MS) == HAL_OK;
}

bool SHT30_Port_Read(uint8_t address7, uint8_t *data, uint16_t size)
{
    if ((data == NULL) || (size == 0U))
    {
        return false;
    }

    return HAL_I2C_Master_Receive(&hi2c1, (uint16_t)(address7 << 1U), data, size,
                                  SHT30_PORT_I2C_TIMEOUT_MS) == HAL_OK;
}

void SHT30_Port_DelayMs(uint32_t delay_ms)
{
    HAL_Delay(delay_ms);
}

uint32_t SHT30_Port_GetTickMs(void)
{
    return HAL_GetTick();
}
