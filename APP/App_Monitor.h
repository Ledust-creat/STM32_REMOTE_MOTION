#ifndef APP_MONITOR_H
#define APP_MONITOR_H

#include <stdbool.h>
#include <stdint.h>

void App_Monitor_Init(void);
void App_Monitor_Update(uint32_t now_tick_ms, bool motion_active);
void App_Monitor_OnRtcAlarm(void);
void App_Monitor_OnMotion(void);

#endif /* APP_MONITOR_H */
