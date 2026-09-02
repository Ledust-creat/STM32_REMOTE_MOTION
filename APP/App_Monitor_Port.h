#ifndef APP_MONITOR_PORT_H
#define APP_MONITOR_PORT_H

#include <stdbool.h>
#include <stdint.h>

bool App_Monitor_Port_SetAlarmAfterSeconds(uint32_t seconds);
bool App_Monitor_Port_GetRtcSeconds(uint32_t *seconds);
void App_Monitor_Port_EnterStopMode(void);
void App_Monitor_Port_WriteDebug(const uint8_t *data, uint16_t size);

#endif /* APP_MONITOR_PORT_H */
