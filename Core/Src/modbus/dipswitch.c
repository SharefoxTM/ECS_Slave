#include "../Inc/modbus/dipswitch.h"
#include "main.h"

uint8_t DipSwitch_GetSlaveId(void) {
  uint8_t id = 0;
  id |= (DIP_0_GPIO_Port->IDR >> 3) & 0x1F;
  id |= (DIP_5_GPIO_Port->IDR << 5) & 0xE0;
  return id;
}