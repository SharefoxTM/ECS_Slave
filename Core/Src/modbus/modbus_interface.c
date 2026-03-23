#include "modbus/modbus_interface.h"
#include "led/led_interface.h"

void setOperationFlag(ModbusInterface_t *mb, uint16_t address);

uint8_t MODBUS_DMA_RXData[256];

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

ModbusInterface_t Modbus_Init(uint8_t slaveId) {
	ModbusInterface_t mb = {0};
	uint8_t pdata[256] = {0};
	hcbuf_modbus = cbuf_init(pdata, 256);
	if (hcbuf_modbus == NULL) {
		return mb;
	}

	mb.slaveId = slaveId;

	mb.shiftReg = ShiftRegister_Init();

	// Allocate dynamic register arrays based on total slots and clear them
	mb.coils = (uint8_t *)calloc(mb.shiftReg.totalSlots, sizeof(uint8_t));
	if (mb.coils == NULL) {
		// Handle allocation failure
		// TODO: HANDLE ALLOCATION ERROR
		return mb;
	}
	mb.discreteInputs =
	  (uint8_t *)calloc(mb.shiftReg.totalSlots, sizeof(uint8_t));
	if (mb.discreteInputs == NULL) {
		// Handle allocation failure
		free(mb.coils);
		return mb;
	}
	mb.holdingRegisters =
	  (uint16_t *)calloc((mb.shiftReg.totalSlots + 1), sizeof(uint16_t));
	if (mb.holdingRegisters == NULL) {
		// Handle allocation failure
		free(mb.coils);
		free(mb.discreteInputs);
		return mb;
	}
	// Clear input registers
	for (uint8_t i = 0; i < 3; i++) {
		mb.inputRegisters[i] = 0;
	}

	// Initialize shift registers

	// Set initial values
	mb.inputRegisters[0] = mb.shiftReg.totalSlots; // Total slots
	mb.ledMode = LED_MODE_NORMAL;
	mb.statusRegister = STATUS_BIT_SYSTEM_READY;
	HAL_UARTEx_ReceiveToIdle_DMA(&huart1, MODBUS_DMA_RXData, 256);
	return mb;
}

/**
 * @brief Updates all Modbus registers based on current hardware state.
 * @param mb Pointer to the ModbusInterface_t structure.
 * Call periodically when idle
 */
void Modbus_UpdateRegisters(ModbusInterface_t *mb) {
	// Update discrete inputs from sensor readings
	for (uint8_t slot = 0; slot < mb->shiftReg.totalSlots; slot++) {
		mb->discreteInputs[slot] =
		  ShiftRegister_GetSlotState(&mb->shiftReg, slot) ? 1 : 0;
	}

	// Update input registers
	mb->inputRegisters[0] = mb->shiftReg.totalSlots;
	mb->inputRegisters[1] = ShiftRegister_GetFreeCount(&mb->shiftReg);
	mb->inputRegisters[2] = mb->statusRegister;

	// Update holding registers with current slot states
	for (uint8_t slot = 0; slot < mb->shiftReg.totalSlots; slot++) {
		mb->holdingRegisters[slot] =
		  ShiftRegister_GetSlotState(&mb->shiftReg, slot) ? 1 : 0;
	}

	mb->holdingRegisters[mb->shiftReg.totalSlots] = mb->ledMode;
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
	if (address >= mb->shiftReg.totalSlots) {
		return 0;
	}
	return mb->coils[address];
}

void Modbus_WriteCoil(ModbusInterface_t *mb, uint16_t address, uint8_t value) {
	if (address >= mb->shiftReg.totalSlots) {
		return;
	}

	mb->coils[address] =
	  value ? 1 : 0; // Make sure all non-zero is 1 and zero is 0
	// TODO: implement write coil
	if (mb->coils[address] != mb->discreteInputs[address]) {
		mb->holdingRegisters[address] |= HOLDINGREG_SLOT_NEWOP_FLAG;
	} else {
		mb->holdingRegisters[address] = 0;
		if (mb->coils[address])
			mb->holdingRegisters[address] = HOLDINGREG_SLOT_TAKEN_FLAG;
	}

	if (mb->ledMode != LED_MODE_NORMAL) {
		mb->ledMode = LED_MODE_NORMAL;
		led_updateMode();
	} else {
		argb_t colorHolder;
		colorHolder.brightness = LED_BRIGHTNESS_MEDIUM_HIGH;
		if (mb->holdingRegisters[address] & HOLDINGREG_SLOT_NEWOP_FLAG)
			colorHolder.color = led_blue;
		else if (mb->holdingRegisters[address] & HOLDINGREG_SLOT_TAKEN_FLAG)
			colorHolder.color = led_green;
		else
			colorHolder.color = led_black;
		led_set_color(address, colorHolder);
	}
}

uint8_t Modbus_ReadDiscreteInput(ModbusInterface_t *mb, uint16_t address) {
	if (address >= mb->shiftReg.totalSlots) {
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
	if (address > mb->shiftReg.totalSlots) {
		return 0;
	}
	return mb->holdingRegisters[address];
}

void Modbus_WriteHoldingRegister(ModbusInterface_t *mb, uint16_t address,
                                 uint16_t value) {
	if (address > mb->shiftReg.totalSlots) {
		return;
	}

	mb->holdingRegisters[address] = value;

  // Handle LED mode register
  if (address == mb->shiftReg.totalSlots) {
    mb->ledMode = value;
  }
}

/**
 * @brief Sets the operation flag in the holding register for a specific slot.
 * @param mb Pointer to the ModbusInterface_t structure.
 * @param address The address of the slot.
 */
void setOperationFlag(ModbusInterface_t *mb, uint16_t address) {
  if (mb->coils[address] != mb->discreteInputs[address]) {
    mb->holdingRegisters[address] |= HOLDINGREG_SLOT_NEWOP_FLAG;
  } else {
    mb->holdingRegisters[address] = 0;
    if (mb->coils[address])
      mb->holdingRegisters[address] = HOLDINGREG_SLOT_TAKEN_FLAG;
  }

  if (mb->ledMode != LED_MODE_NORMAL) {
    mb->ledMode = LED_MODE_NORMAL;
    led_updateMode();
  } else {
    if (mb->holdingRegisters[address] & HOLDINGREG_SLOT_NEWOP_FLAG)
      led_set_color(address, led_blue);
    else if (mb->holdingRegisters[address] & HOLDINGREG_SLOT_TAKEN_FLAG)
      led_set_color(address, led_green);
    else
      led_set_color(address, led_black);
  }
}