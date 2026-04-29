#ifndef MODBUS_INTERFACE_H
#define MODBUS_INTERFACE_H

#include "dipswitch.h"
#include "main.h"
#include "modbus_config.h"
#include "shift_register.h"
#include "led/led_modes.h"
#include "circularBuffer.h"
#include "usart.h"

#define HOLDINGREG_SLOT_TAKEN_FLAG 0x0001
#define HOLDINGREG_SLOT_ERROR_FLAG 0x0002
#define HOLDINGREG_SLOT_NEWOP_FLAG 0x0004

#define MODBUS_FUNCTION_READ_COILS 0x01
#define MODBUS_FUNCTION_READ_DISCRETE_INPUTS 0x02
#define MODBUS_FUNCTION_READ_HOLDING_REGISTERS 0x03
#define MODBUS_FUNCTION_READ_INPUT_REGISTERS 0x04
#define MODBUS_FUNCTION_WRITE_SINGLE_COIL 0x05
#define MODBUS_FUNCTION_WRITE_SINGLE_HOLDING_REGISTER 0x06
#define MODBUS_FUNCTION_WRITE_MULTIPLE_COILS 0x0F
#define MODBUS_FUNCTION_WRITE_MULTIPLE_HOLDING_REGISTERS 0x10

typedef struct modbusHandler {
	// Dynamic sizes based on detected slots
	uint8_t *coils;             // Read/Write coils for override
	uint8_t *discreteInputs;    // Read-only discrete inputs for slot states
	uint16_t inputRegisters[3]; // Status registers
	uint16_t *holdingRegisters; // Slot status + LED mode

	// System state
	ShiftRegister_t shiftReg;
	uint8_t slaveId;
	uint16_t statusRegister;
	uint16_t ledMode;

} ModbusInterface_t;

extern ModbusInterface_t *hmb;

// Function prototypes
ModbusConfig_t Modbus_ReadConfig(void);
ModbusInterface_t Modbus_Init(uint8_t slaveId);
void Modbus_Update(ModbusInterface_t *mb);
void Modbus_UpdateRegisters(ModbusInterface_t *mb);
void Modbus_ProcessReceivedData(ModbusInterface_t *mb);
// Modbus callback handlers
uint8_t Modbus_ReadCoils(ModbusInterface_t *mb, uint16_t address,
                         uint16_t count, uint8_t *output);
uint8_t Modbus_ReadDiscreteInputs(ModbusInterface_t *mb, uint16_t address, uint16_t count, uint8_t *output);
uint16_t Modbus_ReadInputRegisters(ModbusInterface_t *mb, uint16_t address, uint16_t count, uint8_t *output);
uint16_t Modbus_ReadHoldingRegisters(ModbusInterface_t *mb, uint16_t address, uint16_t count, uint8_t *output);
void Modbus_WriteHoldingRegister(ModbusInterface_t *mb, uint16_t address,
                                 uint16_t value);
void Modbus_WriteMultipleHoldingRegisters(ModbusInterface_t *mb,
                                          uint16_t address, uint16_t count,
                                          uint16_t *values);
void Modbus_WriteCoil(ModbusInterface_t *mb, uint16_t address, uint8_t value);
void Modbus_WriteMultipleCoils(ModbusInterface_t *mb, uint16_t address,
                               uint16_t count, uint16_t *values);
uint8_t Modbus_ValidateCode(uint8_t functionCode);

void Modbus_OnTxComplete(void);
void Modbus_OnTxError(uint32_t errorCode);

#endif // MODBUS_INTERFACE_H