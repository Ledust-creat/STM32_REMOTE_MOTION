/** Portable SSD1306-compatible 128x64 OLED component. */
#include "OLED.h"
#include "OLED_Font.h"
#include "OLED_Port.h"

#include <stddef.h>

static bool s_last_transfer_ok = true;

static void OLED_WriteCommand(uint8_t command)
{
  const uint8_t frame[2] = {0x00U, command};
  s_last_transfer_ok = OLED_Port_Write(OLED_I2C_ADDRESS_7BIT, frame, sizeof(frame));
}

static void OLED_WriteData(uint8_t data)
{
  const uint8_t frame[2] = {0x40U, data};
  s_last_transfer_ok = OLED_Port_Write(OLED_I2C_ADDRESS_7BIT, frame, sizeof(frame));
}

static void OLED_WriteDataBlock(const uint8_t *data, uint8_t size)
{
  uint8_t frame[129];
  uint8_t index;

  frame[0] = 0x40U;
  for (index = 0U; index < size; index++)
  {
    frame[index + 1U] = data[index];
  }
  s_last_transfer_ok = OLED_Port_Write(OLED_I2C_ADDRESS_7BIT, frame, (uint16_t)size + 1U);
}

static void OLED_SetCursor(uint8_t page, uint8_t column)
{
  OLED_WriteCommand(0xB0U | page);
  OLED_WriteCommand(0x10U | ((column & 0xF0U) >> 4U));
  OLED_WriteCommand(0x00U | (column & 0x0FU));
}

static uint32_t OLED_Pow(uint32_t base, uint32_t exponent)
{
  uint32_t result = 1U;
  while (exponent-- > 0U)
  {
    result *= base;
  }
  return result;
}

bool OLED_GetLastTransferOk(void)
{
  return s_last_transfer_ok;
}

void OLED_Clear(void)
{
  static const uint8_t clear_data[128] = {0};
  uint8_t page;

  for (page = 0U; page < 8U; page++)
  {
    OLED_SetCursor(page, 0U);
    OLED_WriteDataBlock(clear_data, sizeof(clear_data));
  }
}

void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{
  uint8_t index;
  uint8_t font_index;

  if ((Line < 1U) || (Line > 4U) || (Column < 1U) || (Column > 16U) ||
      (Char < ' ') || (Char > '~'))
  {
    return;
  }

  font_index = (uint8_t)(Char - ' ');
  OLED_SetCursor((uint8_t)((Line - 1U) * 2U), (uint8_t)((Column - 1U) * 8U));
  for (index = 0U; index < 8U; index++)
  {
    OLED_WriteData(OLED_F8x16[font_index][index]);
  }
  OLED_SetCursor((uint8_t)((Line - 1U) * 2U + 1U), (uint8_t)((Column - 1U) * 8U));
  for (index = 0U; index < 8U; index++)
  {
    OLED_WriteData(OLED_F8x16[font_index][index + 8U]);
  }
}

void OLED_ShowString(uint8_t Line, uint8_t Column, const char *String)
{
  uint8_t offset = 0U;

  if (String == NULL)
  {
    return;
  }
  while ((String[offset] != '\0') && ((uint16_t)Column + offset <= 16U))
  {
    OLED_ShowChar(Line, (uint8_t)(Column + offset), String[offset]);
    offset++;
  }
}

void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
  uint8_t index;
  for (index = 0U; index < Length; index++)
  {
    OLED_ShowChar(Line, (uint8_t)(Column + index),
                  (char)(Number / OLED_Pow(10U, Length - index - 1U) % 10U + '0'));
  }
}

void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length)
{
  uint8_t index;
  uint32_t magnitude;

  if (Number >= 0)
  {
    OLED_ShowChar(Line, Column, '+');
    magnitude = (uint32_t)Number;
  }
  else
  {
    OLED_ShowChar(Line, Column, '-');
    magnitude = (uint32_t)(-(Number + 1)) + 1U;
  }
  for (index = 0U; index < Length; index++)
  {
    OLED_ShowChar(Line, (uint8_t)(Column + index + 1U),
                  (char)(magnitude / OLED_Pow(10U, Length - index - 1U) % 10U + '0'));
  }
}

void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
  uint8_t index;
  uint8_t digit;
  for (index = 0U; index < Length; index++)
  {
    digit = (uint8_t)(Number / OLED_Pow(16U, Length - index - 1U) % 16U);
    OLED_ShowChar(Line, (uint8_t)(Column + index),
                  (char)(digit < 10U ? digit + '0' : digit - 10U + 'A'));
  }
}

void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
  uint8_t index;
  for (index = 0U; index < Length; index++)
  {
    OLED_ShowChar(Line, (uint8_t)(Column + index),
                  (char)(Number / OLED_Pow(2U, Length - index - 1U) % 2U + '0'));
  }
}

void OLED_Init(void)
{
  OLED_Port_DelayMs(100U);
  OLED_WriteCommand(0xAEU);
  OLED_WriteCommand(0xD5U);
  OLED_WriteCommand(0x80U);
  OLED_WriteCommand(0xA8U);
  OLED_WriteCommand(0x3FU);
  OLED_WriteCommand(0xD3U);
  OLED_WriteCommand(0x00U);
  OLED_WriteCommand(0x40U);
  OLED_WriteCommand(0xA1U);
  OLED_WriteCommand(0xC8U);
  OLED_WriteCommand(0xDAU);
  OLED_WriteCommand(0x12U);
  OLED_WriteCommand(0x81U);
  OLED_WriteCommand(0xCFU);
  OLED_WriteCommand(0xD9U);
  OLED_WriteCommand(0xF1U);
  OLED_WriteCommand(0xDBU);
  OLED_WriteCommand(0x30U);
  OLED_WriteCommand(0xA4U);
  OLED_WriteCommand(0xA6U);
  OLED_WriteCommand(0x8DU);
  OLED_WriteCommand(0x14U);
  OLED_WriteCommand(0xAFU);
  OLED_Clear();
}
