#include "RCWL0515.h"

#include <stddef.h>

#include "RCWL0515_Port.h"

/*
*********************************************************************************************************
*   模块变量
*********************************************************************************************************
*/

static volatile bool s_motion_event_pending;    /* EXTI 上升沿置位，主循环读取后清除 */
static volatile uint32_t s_last_motion_tick_ms; /* 最近一次 OUT 上升沿的毫秒时间戳 */

/*
*********************************************************************************************************
*   函数定义
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*   函 数 名: RCWL0515_Init
*   功能说明: 初始化雷达模块的软件事件状态；GPIO 和 EXTI 硬件配置由 CubeMX 生成
*   返 回 值: 无
*********************************************************************************************************
*/
void RCWL0515_Init(void)
{
    uint32_t interrupt_state = RCWL0515_Port_EnterCritical();

    s_motion_event_pending = false;
    s_last_motion_tick_ms = 0U;

    RCWL0515_Port_ExitCritical(interrupt_state);
}

/*
*********************************************************************************************************
*   函 数 名: RCWL0515_OnRisingEdge
*   功能说明: 记录 RCWL-0515 OUT 上升沿事件；允许由 EXTI 中断回调调用
*   形    参: event_tick_ms - 触发时的毫秒系统节拍
*   返 回 值: 无
*********************************************************************************************************
*/
void RCWL0515_OnRisingEdge(uint32_t event_tick_ms)
{
    s_last_motion_tick_ms = event_tick_ms;
    s_motion_event_pending = true;
}

/*
*********************************************************************************************************
*   函 数 名: RCWL0515_ConsumeMotionEvent
*   功能说明: 原子地读取并清除一条待处理的运动事件
*   形    参: event_tick_ms - 调用者提供的事件时间戳输出地址
*   返 回 值: true 表示读取到事件；false 表示无待处理事件或参数无效
*********************************************************************************************************
*/
bool RCWL0515_ConsumeMotionEvent(uint32_t *event_tick_ms)
{
    uint32_t interrupt_state;
    bool has_event;

    if (event_tick_ms == NULL)
    {
        return false;
    }

    interrupt_state = RCWL0515_Port_EnterCritical();
    has_event = s_motion_event_pending;

    if (has_event)
    {
        *event_tick_ms = s_last_motion_tick_ms;
        s_motion_event_pending = false;
    }

    RCWL0515_Port_ExitCritical(interrupt_state);
    return has_event;
}

/*
*********************************************************************************************************
*   函 数 名: RCWL0515_IsMotionActive
*   功能说明: 读取当前 OUT 电平；高电平表示模块仍处于一次运动触发的保持期
*   返 回 值: true 表示 OUT 为高；false 表示 OUT 为低
*********************************************************************************************************
*/
bool RCWL0515_IsMotionActive(void)
{
    return RCWL0515_Port_ReadMotionLevel();
}
