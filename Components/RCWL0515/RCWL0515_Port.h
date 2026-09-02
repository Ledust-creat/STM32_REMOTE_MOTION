#ifndef RCWL0515_PORT_H
#define RCWL0515_PORT_H

#include <stdbool.h>
#include <stdint.h>

bool RCWL0515_Port_ReadMotionLevel(void);
uint32_t RCWL0515_Port_EnterCritical(void);
void RCWL0515_Port_ExitCritical(uint32_t interrupt_state);

#endif /* RCWL0515_PORT_H */
