#ifndef BSP_UART_H
#define BSP_UART_H

#include <stdbool.h>
#include <stdint.h>

/*
*********************************************************************************************************
*   类型定义
*********************************************************************************************************
*/

typedef enum
{
    BSP_UART_CHANNEL_DEBUG = 0U,  /* 当前映射到 USART1，用于开发调试输出 */
    BSP_UART_CHANNEL_MODEM,       /* 当前映射到 USART2，用于 ML307C AT 通信 */
    BSP_UART_CHANNEL_COUNT
} BSP_UART_Channel_t;

/*
*********************************************************************************************************
*   函数声明
*********************************************************************************************************
*/

bool BSP_UART_Write(BSP_UART_Channel_t channel, const uint8_t *data, uint16_t size);
bool BSP_UART_Putc(BSP_UART_Channel_t channel, uint8_t ch);
bool BSP_UART_StartReceiveIT(BSP_UART_Channel_t channel);
uint16_t BSP_UART_Read(BSP_UART_Channel_t channel, uint8_t *data, uint16_t max_size);
void BSP_UART_OnModemRxComplete(void);

#endif /* BSP_UART_H */
