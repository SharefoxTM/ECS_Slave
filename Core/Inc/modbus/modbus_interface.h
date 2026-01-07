#ifndef MODBUS_INTERFACE_H
#define MODBUS_INTERFACE_H

#include "dipswitch.h"
#include "modbus_config.h"
#include "shift_register.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
  // Dynamic sizes based on detected slots
  uint8_t *coils;             // Read/Write coils for override
  uint8_t *discreteInputs;    // Read-only discrete inputs for slot states
  uint16_t inputRegisters[3]; // Status registers
  uint16_t *holdingRegisters; // Slot status + LED mode

  // System state
  ShiftRegister_t shiftReg;
  uint8_t slaveId;
  uint8_t totalSlots;
  uint16_t statusRegister;
  uint16_t ledMode;

} ModbusInterface_t;

// Function prototypes
ModbusConfig_t Modbus_ReadConfig(void);
void Modbus_Init(ModbusInterface_t *mb, uint8_t slaveId, uint8_t totalSlots);
void Modbus_Update(ModbusInterface_t *mb);
void Modbus_UpdateRegisters(ModbusInterface_t *mb);

// Modbus callback handlers
uint8_t Modbus_ReadCoil(ModbusInterface_t *mb, uint16_t address);
void Modbus_WriteCoil(ModbusInterface_t *mb, uint16_t address, uint8_t value);
uint8_t Modbus_ReadDiscreteInput(ModbusInterface_t *mb, uint16_t address);
uint16_t Modbus_ReadInputRegister(ModbusInterface_t *mb, uint16_t address);
uint16_t Modbus_ReadHoldingRegister(ModbusInterface_t *mb, uint16_t address);
void Modbus_WriteHoldingRegister(ModbusInterface_t *mb, uint16_t address,
                                 uint16_t value);

#endif // MODBUS_INTERFACE_H