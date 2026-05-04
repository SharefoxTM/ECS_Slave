#ifndef MODBUS_CONFIG_H
#define MODBUS_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

// Modbus configuration
#define MAX_SHIFT_REGISTERS 5 // Maximum 40 slots (5 x 8-bit)
#define BITS_PER_SR 8
#define MAX_SLOTS (MAX_SHIFT_REGISTERS * BITS_PER_SR) // 40 maximum
#define MODBUS_HOLDING_REGISTER_COUNT 42

// Dipswitch configuration pins
#define DIPSWITCH_BIT0_PIN // Configure for your STM32
#define DIPSWITCH_BIT1_PIN
#define DIPSWITCH_BIT2_PIN
#define DIPSWITCH_BIT3_PIN

// Status register bits
#define STATUS_BIT_SYSTEM_READY 0x0001
#define STATUS_BIT_SCANNING 0x0002
#define STATUS_BIT_ERROR 0x0004
#define STATUS_BIT_OVERRIDE_ACTIVE 0x0008

// LED modes
#define LED_MODE_OFF 0x0000
#define LED_MODE_NORMAL 0x0001
#define LED_MODE_VEGAS 0x0002
#define LED_MODE_KNIGHT 0x0004

typedef struct {
	uint8_t slaveId;  // Modbus slave ID (1-247)
	uint8_t numSlots; // Detected number of slots (8-40, in multiples of 8)
} ModbusConfig_t;

// Function prototypes
void DipSwitch_Init(void);
ModbusConfig_t DipSwitch_ReadConfig(void);
uint8_t DipSwitch_GetSlaveId(void);
uint8_t DipSwitch_GetNumSlots(void);

#endif // MODBUS_CONFIG_H