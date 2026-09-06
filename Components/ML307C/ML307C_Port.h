#ifndef ML307C_PORT_H
#define ML307C_PORT_H

#include <stdbool.h>
#include <stdint.h>

bool ML307C_Port_Write(const uint8_t *data, uint16_t size);
bool ML307C_Port_StartReceive(void);
uint16_t ML307C_Port_Read(uint8_t *data, uint16_t max_size);

#endif /* ML307C_PORT_H */
