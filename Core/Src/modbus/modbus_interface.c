#include "../Inc/modbus/modbus_interface.h"

ModbusConfig_t Modbus_ReadConfig(void) {
  ModbusConfig_t config = {0};

  // Read 4 bits for slave ID (0-15)
  config.slaveId = DipSwitch_GetSlaveId();

  // Slave ID range 1-247, map dipswitch 0-15 to 1-16
  config.slaveId = (config.slaveId == 0) ? 1 : (config.slaveId + 1);
  if (config.slaveId > 247)
    config.slaveId = 247;

  // Slot count determined by hardware detection
  // Will be set by shift register detection
  config.numSlots = 0;

  return config;
}

void Modbus_Init(ModbusInterface_t *mb, uint8_t slaveId, uint8_t totalSlots) {
  mb->slaveId = slaveId;
  mb->totalSlots = totalSlots;

  // Allocate dynamic register arrays based on total slots
  mb->coils = (uint8_t *)malloc(totalSlots);
  mb->discreteInputs = (uint8_t *)malloc(totalSlots);
  mb->holdingRegisters =
      (uint16_t *)malloc((totalSlots + 1) * sizeof(uint16_t));

  // Clear all registers
  memset(mb->coils, 0, totalSlots);
  memset(mb->discreteInputs, 0, totalSlots);
  memset(mb->inputRegisters, 0, sizeof(mb->inputRegisters));
  memset(mb->holdingRegisters, 0, (totalSlots + 1) * sizeof(uint16_t));

  // Initialize shift registers
  ShiftRegister_Init(&mb->shiftReg);

  // Set initial values
  mb->inputRegisters[0] = totalSlots; // Total slots
  mb->ledMode = LED_MODE_SLOT_STATUS;
  mb->statusRegister = STATUS_BIT_SYSTEM_READY;
}

void Modbus_UpdateRegisters(ModbusInterface_t *mb) {
  // Update discrete inputs from sensor readings
  for (uint8_t slot = 0; slot < mb->totalSlots; slot++) {
    mb->discreteInputs[slot] =
        ShiftRegister_GetSlotState(&mb->shiftReg, slot) ? 1 : 0;
  }

  // Update input registers
  mb->inputRegisters[0] = mb->totalSlots;
  mb->inputRegisters[1] = ShiftRegister_GetFreeCount(&mb->shiftReg);
  mb->inputRegisters[2] = mb->statusRegister;

  // Update holding registers with current slot states
  for (uint8_t slot = 0; slot < mb->totalSlots; slot++) {
    mb->holdingRegisters[slot] =
        ShiftRegister_GetSlotState(&mb->shiftReg, slot) ? 1 : 0;
  }

  mb->holdingRegisters[mb->totalSlots] = mb->ledMode;
}

void Modbus_Update(ModbusInterface_t *mb) {
  // Read Hall effect sensors
  ShiftRegister_ReadSensors(&mb->shiftReg);

  // Update all Modbus registers
  Modbus_UpdateRegisters(mb);

  // Update status
  mb->statusRegister = STATUS_BIT_SYSTEM_READY | STATUS_BIT_SCANNING;
}

uint8_t Modbus_ReadCoil(ModbusInterface_t *mb, uint16_t address) {
  if (address >= mb->totalSlots) {
    return 0;
  }
  return mb->coils[address];
}

void Modbus_WriteCoil(ModbusInterface_t *mb, uint16_t address, uint8_t value) {
  if (address >= mb->totalSlots) {
    return;
  }

  mb->coils[address] = value ? 1 : 0;

  // Apply override to shift register
  ShiftRegister_SetOverride(&mb->shiftReg, address, value != 0);

  if (value) {
    mb->statusRegister |= STATUS_BIT_OVERRIDE_ACTIVE;
  }
}

uint8_t Modbus_ReadDiscreteInput(ModbusInterface_t *mb, uint16_t address) {
  if (address >= mb->totalSlots) {
    return 0;
  }
  return mb->discreteInputs[address];
}

uint16_t Modbus_ReadInputRegister(ModbusInterface_t *mb, uint16_t address) {
  if (address > 2) {
    return 0;
  }
  return mb->inputRegisters[address];
}

uint16_t Modbus_ReadHoldingRegister(ModbusInterface_t *mb, uint16_t address) {
  if (address > mb->totalSlots) {
    return 0;
  }
  return mb->holdingRegisters[address];
}

void Modbus_WriteHoldingRegister(ModbusInterface_t *mb, uint16_t address,
                                 uint16_t value) {
  if (address > mb->totalSlots) {
    return;
  }

  mb->holdingRegisters[address] = value;

  // Handle LED mode register
  if (address == mb->totalSlots) {
    mb->ledMode = value;
  }
}