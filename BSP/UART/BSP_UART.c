#include "BSP_UART.h"

#include <stdio.h>

#include "usart.h"

/*
*********************************************************************************************************
*   宏定义
*********************************************************************************************************
*/

#define BSP_UART_TX_TIMEOUT_MS    (100U)  /* 单次阻塞发送的最长等待时间，单位 ms */
#define BSP_UART_MODEM_RX_SIZE    (256U)  /* ML307C 接收环形缓冲区容量，必须小于 uint16_t 最大值 */

/*
*********************************************************************************************************
*   模块变量
*********************************************************************************************************
*/

static uint8_t s_modem_rx_byte;                       /* USART2 当前中断接收的单字节暂存 */
static uint8_t s_modem_rx_buffer[BSP_UART_MODEM_RX_SIZE];
static volatile uint16_t s_modem_rx_head;             /* ISR 仅推进写指针 */
static volatile uint16_t s_modem_rx_tail;             /* 主循环仅推进读指针 */

/*
*********************************************************************************************************
*   函数定义
*********************************************************************************************************
*/

static UART_HandleTypeDef *BSP_UART_GetHandle(BSP_UART_Channel_t channel)
{
    switch (channel)
    {
        case BSP_UART_CHANNEL_DEBUG:
            return &huart1;

        case BSP_UART_CHANNEL_MODEM:
            return &huart2;

        default:
            return NULL;
    }
}

/*
*********************************************************************************************************
*   函 数 名: BSP_UART_Write
*   功能说明: 通过指定 UART 通道阻塞发送一段字节数据
*   形    参: channel - 发送通道；data - 调用者持有的发送缓冲区；size - 字节数
*   返 回 值: true 表示全部发送成功；false 表示通道或参数无效，或发送超时/失败
*********************************************************************************************************
*/
bool BSP_UART_Write(BSP_UART_Channel_t channel, const uint8_t *data, uint16_t size)
{
    UART_HandleTypeDef *handle = BSP_UART_GetHandle(channel);

    if ((handle == NULL) || ((data == NULL) && (size != 0U)))
    {
        return false;
    }

    if (size == 0U)
    {
        return true;
    }

    return HAL_UART_Transmit(handle, (uint8_t *)data, size, BSP_UART_TX_TIMEOUT_MS) == HAL_OK;
}

/*
*********************************************************************************************************
*   函 数 名: BSP_UART_Putc
*   功能说明: 通过指定 UART 通道发送一个字节
*   形    参: channel - 发送通道；ch - 待发送字节
*   返 回 值: true 表示发送成功；false 表示发送失败或超时
*********************************************************************************************************
*/
bool BSP_UART_Putc(BSP_UART_Channel_t channel, uint8_t ch)
{
    return BSP_UART_Write(channel, &ch, 1U);
}

/*
*********************************************************************************************************
*   函 数 名: BSP_UART_StartReceiveIT
*   功能说明: 启动指定 UART 通道的中断接收；当前仅 ML307C 通道支持接收缓冲
*   形    参: channel - UART 通道
*   返 回 值: true 表示接收已启动；false 表示通道不支持或 HAL 启动失败
*********************************************************************************************************
*/
bool BSP_UART_StartReceiveIT(BSP_UART_Channel_t channel)
{
    if (channel != BSP_UART_CHANNEL_MODEM)
    {
        return false;
    }

    s_modem_rx_head = 0U;
    s_modem_rx_tail = 0U;

    return HAL_UART_Receive_IT(&huart2, &s_modem_rx_byte, 1U) == HAL_OK;
}

/*
*********************************************************************************************************
*   函 数 名: BSP_UART_Read
*   功能说明: 从指定 UART 通道的接收环形缓冲区取出已接收字节
*   形    参: channel - UART 通道；data - 调用者输出缓冲区；max_size - 最多读取字节数
*   返 回 值: 实际读取字节数；0 表示无数据、通道不支持或参数无效
*********************************************************************************************************
*/
uint16_t BSP_UART_Read(BSP_UART_Channel_t channel, uint8_t *data, uint16_t max_size)
{
    uint16_t read_size = 0U;

    if ((channel != BSP_UART_CHANNEL_MODEM) || (data == NULL) || (max_size == 0U))
    {
        return 0U;
    }

    while ((read_size < max_size) && (s_modem_rx_tail != s_modem_rx_head))
    {
        data[read_size] = s_modem_rx_buffer[s_modem_rx_tail];
        read_size++;
        s_modem_rx_tail = (uint16_t)((s_modem_rx_tail + 1U) % BSP_UART_MODEM_RX_SIZE);
    }

    return read_size;
}

/*
*********************************************************************************************************
*   函 数 名: BSP_UART_OnModemRxComplete
*   功能说明: 由 USART2 接收完成回调调用，将一个字节写入环形缓冲并立即重启接收
*   返 回 值: 无
*********************************************************************************************************
*/
void BSP_UART_OnModemRxComplete(void)
{
    uint16_t next_head = (uint16_t)((s_modem_rx_head + 1U) % BSP_UART_MODEM_RX_SIZE);

    if (next_head != s_modem_rx_tail)
    {
        s_modem_rx_buffer[s_modem_rx_head] = s_modem_rx_byte;
        s_modem_rx_head = next_head;
    }

    /* 满缓冲时丢弃最新字节，保留尚未被主循环读取的原始 AT 返回。 */
    (void)HAL_UART_Receive_IT(&huart2, &s_modem_rx_byte, 1U);
}

/*
*********************************************************************************************************
*   函 数 名: fputc
*   功能说明: 将 C 库 printf 的单字节输出定向到当前调试串口
*   形    参: ch - 待输出字符；stream - C 库流对象，本工程不区分不同流
*   返 回 值: 成功返回字符本身；失败返回 EOF
*********************************************************************************************************
*/
int fputc(int ch, FILE *stream)
{
    (void)stream;

    return BSP_UART_Putc(BSP_UART_CHANNEL_DEBUG, (uint8_t)ch) ? ch : EOF;
}
