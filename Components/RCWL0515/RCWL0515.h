#ifndef RCWL0515_H
#define RCWL0515_H

#include <stdbool.h>
#include <stdint.h>

/*
*********************************************************************************************************
*   函数声明
*********************************************************************************************************
*/

void RCWL0515_Init(void);
void RCWL0515_OnRisingEdge(uint32_t event_tick_ms);
bool RCWL0515_ConsumeMotionEvent(uint32_t *event_tick_ms);
bool RCWL0515_IsMotionActive(void);

#endif /* RCWL0515_H */
