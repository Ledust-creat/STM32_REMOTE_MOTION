/** STM32 HAL implementation of the OLED hardware abstraction port. */
#include "OLED_Port.h"
#include "i2c.h"

#include <stddef.h>

#define OLED_PORT_TIMEOUT_MS  100U

bool OLED_Port_Write(uint8_t address7, const uint8_t *data, uint16_t size)
{
  if ((data == NULL) || (size == 0U))
  {
    return false;
  }

  return HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)(address7 << 1U),
                                 (uint8_t *)data, size,
                                 OLED_PORT_TIMEOUT_MS) == HAL_OK;
}

void OLED_Port_DelayMs(uint32_t delay_ms)
{
  HAL_Delay(delay_ms);
}
