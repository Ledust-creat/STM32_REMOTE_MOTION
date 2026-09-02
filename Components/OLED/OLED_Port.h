/** OLED component hardware abstraction boundary. */
#ifndef OLED_PORT_H
#define OLED_PORT_H

#include <stdbool.h>
#include <stdint.h>

bool OLED_Port_Write(uint8_t address7, const uint8_t *data, uint16_t size);
void OLED_Port_DelayMs(uint32_t delay_ms);

#endif /* OLED_PORT_H */
