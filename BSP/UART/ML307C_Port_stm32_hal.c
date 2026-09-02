#include "ML307C_Port.h"

#include "BSP_UART.h"

static const uint8_t s_tx_prefix[] = "ML307C TX: ";
static const uint8_t s_tx_suffix[] = "\r\n";
static const uint8_t s_tx_ctrl_z[] = "ML307C TX: [0x1A]\r\n";

bool ML307C_Port_Write(const uint8_t *data, uint16_t size)
{
    /* 调试口镜像每一段发往模块的数据；Ctrl+Z 为不可见控制字节，单独以文本形式显示。 */
    if ((size == 1U) && (data[0] == 0x1AU))
    {
        (void)BSP_UART_Write(BSP_UART_CHANNEL_DEBUG, s_tx_ctrl_z, sizeof(s_tx_ctrl_z) - 1U);
    }
    else
    {
        (void)BSP_UART_Write(BSP_UART_CHANNEL_DEBUG, s_tx_prefix, sizeof(s_tx_prefix) - 1U);
        (void)BSP_UART_Write(BSP_UART_CHANNEL_DEBUG, data, size);
        (void)BSP_UART_Write(BSP_UART_CHANNEL_DEBUG, s_tx_suffix, sizeof(s_tx_suffix) - 1U);
    }

    return BSP_UART_Write(BSP_UART_CHANNEL_MODEM, data, size);
}

bool ML307C_Port_StartReceive(void)
{
    return BSP_UART_StartReceiveIT(BSP_UART_CHANNEL_MODEM);
}

uint16_t ML307C_Port_Read(uint8_t *data, uint16_t max_size)
{
    return BSP_UART_Read(BSP_UART_CHANNEL_MODEM, data, max_size);
}
