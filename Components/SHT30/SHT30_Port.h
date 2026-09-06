#ifndef SHT30_PORT_H
#define SHT30_PORT_H

#include <stdbool.h>
#include <stdint.h>

bool SHT30_Port_Write(uint8_t address7, const uint8_t *data, uint16_t size);
bool SHT30_Port_Read(uint8_t address7, uint8_t *data, uint16_t size);
void SHT30_Port_DelayMs(uint32_t delay_ms);
uint32_t SHT30_Port_GetTickMs(void);

#endif /* SHT30_PORT_H */
