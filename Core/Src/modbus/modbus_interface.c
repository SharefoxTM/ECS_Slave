#include "modbus/modbus_interface.h"
#include "Utilities/log.h"
#include "led/led_interface.h"
#include "modbus/modbus_crc.h"
#include "shift_register.h"
#include "stm32f0xx_hal_uart_ex.h"
#include <stdint.h>
#include <string.h>

void updateRegisters(ModbusInterface_t *mb);
void setOperationFlag(ModbusInterface_t *mb, uint16_t address);
void processReceivedPackage(ModbusInterface_t *mb, uint8_t *data);
HAL_StatusTypeDef sendResponseRead(ModbusInterface_t *mb, uint8_t *requestData, uint8_t byteCount, uint8_t *responseData);
HAL_StatusTypeDef sendResponseWrite(ModbusInterface_t *mb, uint8_t *requestData);
HAL_StatusTypeDef sendResponseWriteMultiple(ModbusInterface_t *mb, uint8_t *requestData);
HAL_StatusTypeDef sendExceptionResponse(ModbusInterface_t *mb, uint8_t functionCode, uint8_t exceptionCode);
HAL_StatusTypeDef startTx(const uint8_t *data, uint16_t len, uint32_t timeout);
uint8_t checkCrc(const uint8_t *frame, uint16_t frameLen);
void appendCrc(uint8_t *frame, uint16_t length);

uint8_t MODBUS_RXData[256];

/**
 * @brief Reads the Modbus configuration from hardware.
 * @details Reads the slave ID from the DIP switch and maps it to the range 1-16.
 *          Slot count is initialized to 0 and determined later by shift register detection.
 * @return ModbusConfig_t Populated configuration structure.
 */
ModbusConfig_t Modbus_ReadConfig(void) {
	ModbusConfig_t config = {0};

	config.slaveId = DipSwitch_GetSlaveId();

	if (config.slaveId == 0) {
		LOG_WARN("DIP switch slave ID is 0, which is reserved for broadcast. Defaulting to slave ID 1.");
		config.slaveId = 1;
	} else if (config.slaveId > 16) {
		LOG_WARN("DIP switch slave ID %u is out of range (1-16). Defaulting to slave ID 16.", config.slaveId);
		config.slaveId = 16;
	}

	ShiftRegister_t sr = ShiftRegister_Init();
	config.numSlots = sr.totalSlots;
	LOG_INFO("Modbus config loaded: slaveId=%u, numSlots=%u", config.slaveId, config.numSlots);

	return config;
}

/**
 * @brief Initializes the Modbus interface.
 * @details Allocates dynamic arrays for coils, discrete inputs, and holding registers
 *          based on the number of detected slots. Initializes the shift register,
 *          input registers, and starts DMA UART reception.
 * @param slaveId The Modbus slave ID to assign to this device.
 * @return ModbusInterface_t Initialized interface structure. On allocation failure,
 *         returns a zeroed structure.
 */
ModbusInterface_t Modbus_Init(uint8_t slaveId) {
	ModbusInterface_t mb = {0};
	LOG_INFO("Modbus init start: requestedSlaveId=%u", slaveId);

	ModbusConfig_t config = Modbus_ReadConfig();

	mb.slaveId = config.slaveId;
	mb.shiftReg = ShiftRegister_Init();

	mb.coils = (uint8_t *)calloc(mb.shiftReg.totalSlots, sizeof(uint8_t));
	if (mb.coils == NULL) {
		LOG_ERROR("Modbus init failed: coils allocation failed, slots=%u", mb.shiftReg.totalSlots);
		return mb;
	}
	mb.discreteInputs =
	  (uint8_t *)calloc(mb.shiftReg.totalSlots, sizeof(uint8_t));
	if (mb.discreteInputs == NULL) {
		LOG_ERROR("Modbus init failed: discreteInputs allocation failed, slots=%u", mb.shiftReg.totalSlots);
		free(mb.coils);
		return mb;
	}
	mb.holdingRegisters =
	  (uint16_t *)calloc((MODBUS_HOLDING_REGISTER_COUNT), sizeof(uint16_t));
	if (mb.holdingRegisters == NULL) {
		LOG_ERROR("Modbus init failed: holdingRegisters allocation failed, slots=%u", mb.shiftReg.totalSlots);
		free(mb.coils);
		free(mb.discreteInputs);
		return mb;
	}

	for (uint8_t i = 0; i < 3; i++) {
		mb.inputRegisters[i] = 0;
	}

	mb.inputRegisters[0] = mb.shiftReg.totalSlots;
	mb.ledMode = LED_MODE_NORMAL;
	mb.statusRegister = STATUS_BIT_SYSTEM_READY;
	if (HAL_UARTEx_ReceiveToIdle_IT(&huart1, MODBUS_RXData, sizeof(MODBUS_RXData)) != HAL_OK) {
		LOG_ERROR("Failed to start UART ReceiveToIdle IT during Modbus init");
	}
	LOG_INFO("Modbus init complete: slaveId=%u, slots=%u", mb.slaveId, mb.shiftReg.totalSlots);
	return mb;
}

/**
 * @brief Performs a full Modbus update cycle.
 * @details Reads Hall effect sensors via the shift register, updates all Modbus
 *          registers, and sets the status register to indicate scanning is active.
 * @param mb Pointer to the ModbusInterface_t structure.
 */
void Modbus_Update(ModbusInterface_t *mb) {
	mb->statusRegister = STATUS_BIT_SYSTEM_READY | STATUS_BIT_SCANNING;
	ShiftRegister_ReadSensors(&mb->shiftReg);
	updateRegisters(mb);
	mb->statusRegister &= ~STATUS_BIT_SCANNING;
}

/**
 * @brief Processes all pending data in the Modbus receive circular buffer.
 * @details Peeks at the first three bytes to identify the incoming packet, then
 *          dispatches to processReceivedPackage(). Flushes the buffer if a read
 *          error is encountered.
 * @param mb Pointer to the ModbusInterface_t structure.
 * @param pData Pointer to the received data.
 * @param size Size of the received data.
 */
void Modbus_ProcessReceivedData(ModbusInterface_t *mb, uint8_t *pData, uint16_t size) {
	uint8_t len;
	if (size < 3) {
		LOG_WARN("Received Modbus data too short to process: size=%u", size);
		return;
	}
	if (pData[1] == MODBUS_FUNCTION_WRITE_MULTIPLE_HOLDING_REGISTERS || pData[1] == MODBUS_FUNCTION_WRITE_MULTIPLE_COILS) {
		len = 9 + pData[6]; // 9 bytes header + byte count
	} else {
		len = 8;
	}
	if (!checkCrc(pData, len)) {
		LOG_WARN("Request CRC invalid");
		// FIXME: Send Modbus exception response for CRC error
		return;
	}
	processReceivedPackage(mb, pData);
}

/**
 * @brief Validates a Modbus function code.
 * @param functionCode The function code byte from the received packet.
 * @return 0 if the function code is supported, 1 if it is unsupported.
 */
uint8_t Modbus_ValidateCode(uint8_t functionCode) {
	LOG_DEBUG("Validating Modbus function code: 0x%02X", functionCode);
	switch (functionCode) {
		case MODBUS_FUNCTION_READ_COILS:                       // Read Coils
		case MODBUS_FUNCTION_READ_DISCRETE_INPUTS:             // Read Discrete Inputs
		case MODBUS_FUNCTION_READ_HOLDING_REGISTERS:           // Read Holding Registers
		case MODBUS_FUNCTION_READ_INPUT_REGISTERS:             // Read Input Registers
		case MODBUS_FUNCTION_WRITE_SINGLE_COIL:                // Write Single Coil
		case MODBUS_FUNCTION_WRITE_SINGLE_HOLDING_REGISTER:    // Write Single Holding Register
		case MODBUS_FUNCTION_WRITE_MULTIPLE_COILS:             // Write Multiple Coils
		case MODBUS_FUNCTION_WRITE_MULTIPLE_HOLDING_REGISTERS: // Write Multiple Holding Registers
			return 0;                                            // Valid function code
		default:
			LOG_WARN("Unsupported Modbus function code: 0x%02X", functionCode);
			return 1; // Invalid function code
	}
}

/**
 * @brief Reads coil states into a packed byte array.
 * @details Packs up to @p count coil values starting at @p address (1-based)
 *          into @p output, with 8 coils per byte LSB-first.
 * @param mb      Pointer to the ModbusInterface_t structure.
 * @param data		Buffer containing the Modbus request data, including address and count.
 * @return HAL_OK if the response was sent successfully, or an error status if sending failed.
 */
HAL_StatusTypeDef Modbus_ReadCoils(ModbusInterface_t *mb, uint8_t *data) {
	uint8_t byteCount = 0;
	uint16_t address = (data[2] << 8) | data[3];
	uint16_t count = (data[4] << 8) | data[5];
	uint8_t holder[5] = {0};
	LOG_DEBUG("Read coils request: startAddress=%u, count=%u", address, count);
	for (uint16_t i = address; i < mb->shiftReg.totalSlots && i < address + count; i++) {
		holder[(i - address) / 8] |= (mb->coils[i] & 0x01) << ((i - address) % 8);
		if ((i - address) == 7) {
			byteCount++;
		}
	}
	byteCount++;
	LOG_DEBUG("Read coils response prepared: byteCount=%u", byteCount);
	return sendResponseRead(mb, data, byteCount, holder);
}

/**
 * @brief Reads discrete input states into an output buffer.
 * @param mb      Pointer to the ModbusInterface_t structure.
 * @param data		Buffer containing the Modbus request data, including address and count.
 * @return HAL_OK if the response was sent successfully, or an error status if sending failed.
 */
HAL_StatusTypeDef Modbus_ReadDiscreteInputs(ModbusInterface_t *mb, uint8_t *data) {
	uint16_t address = (data[2] << 8) | data[3];
	uint16_t count = (data[4] << 8) | data[5];
	if (address >= mb->shiftReg.totalSlots) {
		LOG_WARN("Read discrete inputs rejected: startAddress=%u out of range (slots=%u)", address, mb->shiftReg.totalSlots);
		return HAL_ERROR;
	}
	uint8_t holder[5] = {0}, byteCount = 0;
	LOG_DEBUG("Read discrete inputs request: startAddress=%u, count=%u", address, count);
	for (uint16_t i = address; i < mb->shiftReg.totalSlots && i < address + count; i++) {
		holder[(i - address) / 8] |= (mb->discreteInputs[i] & 0x01) << ((i - address) % 8);
		if ((i - address) == 7) {
			byteCount++;
		}
	}
	byteCount++;
	LOG_DEBUG("Read discrete inputs response prepared: count=%u", byteCount);
	return sendResponseRead(mb, data, byteCount, holder);
}

/**
 * @brief Reads input register values into an output buffer.
 * @details Input registers 0-2 contain: total slots, free slot count, and status register.
 * @param mb      Pointer to the ModbusInterface_t structure.
 * @param address 0-based starting input register address (max 2).
 * @param count   Number of registers to read.
 * @param output  Buffer to write the register values into.
 * @return Number of values written, or 0 if @p address is out of range.
 */
HAL_StatusTypeDef Modbus_ReadInputRegisters(ModbusInterface_t *mb, uint8_t *data) {
	uint16_t address = (data[2] << 8) | data[3];
	uint16_t count = (data[4] << 8) | data[5];
	if (count == 0 || address >= 3 || ((uint32_t)address + count) > 3) {
		LOG_WARN("Read input registers rejected: startAddress=%u count=%u out of range (max=2)", address, count);
		return HAL_ERROR;
	}
	LOG_DEBUG("Read input registers request: startAddress=%u, count=%u", address, count);
	uint8_t output[6] = {0};
	uint8_t byteCount = 0;
	for (uint16_t i = address; i < address + count; i++) {
		uint16_t value = mb->inputRegisters[i];
		output[(2 * i)] = (uint8_t)((value >> 8) & 0xFF);
		output[(2 * i) + 1] = (uint8_t)(value & 0xFF);
		byteCount += 2;
	}
	LOG_DEBUG("Read input registers response prepared: byteCount=%u", byteCount);
	return sendResponseRead(mb, data, byteCount, output);
}

/**
 * @brief Reads holding register values into an output buffer.
 * @details Holding registers 0..totalSlots-1 contain per-slot status flags;
 *          register at index totalSlots contains the LED mode.
 * @param mb      Pointer to the ModbusInterface_t structure.
 * @param address 0-based starting holding register address.
 * @param count   Number of registers to read.
 * @param output  Buffer to write the register values into.
 * @return Number of values written, or 0 if @p address is out of range.
 */
HAL_StatusTypeDef Modbus_ReadHoldingRegisters(ModbusInterface_t *mb, uint8_t *data) {
	uint16_t address = (data[2] << 8) | data[3];
	uint16_t count = (data[4] << 8) | data[5];
	if (count == 0 || address >= MODBUS_HOLDING_REGISTER_COUNT || ((uint32_t)address + count) > MODBUS_HOLDING_REGISTER_COUNT) {
		LOG_WARN("Read holding registers rejected: startAddress=%u count=%u out of range (max=%u)",
		         address, count, MODBUS_HOLDING_REGISTER_COUNT - 1U);
		return 0;
	}
	uint8_t output[MODBUS_HOLDING_REGISTER_COUNT * 2];
	uint8_t byteCount = 0;
	LOG_DEBUG("Read holding registers request: startAddress=%u, count=%u", address, count);
	for (uint16_t i = 0; i < count; i++) {
		uint16_t value = mb->holdingRegisters[address + i];
		output[(2 * i)] = (uint8_t)((value >> 8) & 0xFF);
		output[(2 * i) + 1] = (uint8_t)(value & 0xFF);
		byteCount += 2;
	}
	LOG_DEBUG("Read holding registers response prepared: byteCount=%u", byteCount);
	return sendResponseRead(mb, data, byteCount, output);
}

/**
 * @brief Writes a value to a single holding register.
 * @details If the target address is the LED mode register (index totalSlots),
 *          the LED mode is updated accordingly.
 * @param mb      Pointer to the ModbusInterface_t structure.
 * @param address 0-based holding register address.
 * @param value   Value to write.
 */
HAL_StatusTypeDef Modbus_WriteHoldingRegister(ModbusInterface_t *mb, uint8_t *data) {
	uint16_t address = (data[2] << 8) | data[3];
	uint16_t value = (data[4] << 8) | data[5];
	if (address > mb->shiftReg.totalSlots && address != 41) {
		LOG_WARN("Write single holding register ignored: address=%u out of range (max=%u)", address, mb->shiftReg.totalSlots);
		return HAL_ERROR;
	}
	LOG_DEBUG("Write single holding register: address=%u, value=0x%04X", address, value);

	mb->holdingRegisters[address] = value;

	if (address == 41) {
		mb->ledMode = value;
		LOG_INFO("LED mode updated through holding register: mode=%u", mb->ledMode);
	}
	return sendResponseWrite(mb, data);
}

HAL_StatusTypeDef Modbus_WriteMultipleHoldingRegisters(ModbusInterface_t *mb, uint8_t *data) {
	uint16_t address = (data[2] << 8) | data[3];
	uint16_t count = (data[4] << 8) | data[5];
	uint8_t byteCount = data[6];
	if (address >= mb->shiftReg.totalSlots || address + count > mb->shiftReg.totalSlots || count == 0) {
		LOG_WARN("Write multiple holding registers ignored: startAddress=%u out of range (slots=%u)", address, mb->shiftReg.totalSlots);
		return HAL_ERROR;
	}
	uint8_t *values = (uint8_t *)calloc(byteCount, sizeof(uint8_t));
	for (uint16_t i = 0; i < byteCount; i++) {
		values[i] = data[7 + i];
	}
	LOG_DEBUG("Write multiple holding registers: startAddress=%u, count=%u", address, count);
	for (uint16_t i = address; i < address + count; i++) {
		if (i > mb->shiftReg.totalSlots && i != 41) {
			LOG_WARN("(Write Multiple) Write single holding register ignored: address=%u out of range (max=%u)", address + i, mb->shiftReg.totalSlots);
			return HAL_ERROR;
		}
		LOG_DEBUG("(Write Multiple) Write single holding register: address=%u, value=0x%04X", address + i, values[i]);

		mb->holdingRegisters[i] = values[i - address];

		if (i == 41) {
			mb->ledMode = values[i - address];
			LOG_INFO("LED mode updated through holding register: mode=%u", mb->ledMode);
		}
	}
	LOG_DEBUG("Write multiple holding registers complete: startAddress=%u, count=%u", address, count);
	return sendResponseWriteMultiple(mb, data);
}

/**
 * @brief Writes a value to a single coil and updates the LED indicator.
 * @details Sets the coil state, updates the corresponding holding register flags
 *          (NEWOP or TAKEN), and updates the LED color for the slot. If the device
 *          is not in normal LED mode, resets it to normal mode first.
 * @param mb      Pointer to the ModbusInterface_t structure.
 * @param address 0-based coil address.
 * @param value   Non-zero to set the coil, zero to clear it.
 */
HAL_StatusTypeDef Modbus_WriteCoil(ModbusInterface_t *mb, uint8_t *data) {
	uint16_t address = (data[2] << 8) | data[3];
	uint8_t value = (data[4] == 0xFF); // Modbus spec: 0xFF00 to set, 0x0000 to reset
	if (address >= mb->shiftReg.totalSlots) {
		LOG_WARN("Write single coil ignored: address=%u out of range (slots=%u)", address, mb->shiftReg.totalSlots);
		return HAL_ERROR;
	}
	LOG_DEBUG("Write single coil: address=%u, valueRaw=0x%02X", address, value);
	mb->coils[address] = value;

	setOperationFlag(mb, address);

	if (mb->ledMode != LED_MODE_NORMAL) {
		mb->ledMode = LED_MODE_NORMAL;
		led_updateMode();
		LOG_INFO("LED mode forced to normal due to coil write");
	}

	argb_t colorHolder;
	colorHolder.brightness = LED_BRIGHTNESS_MEDIUM_HIGH;
	if (mb->holdingRegisters[address] & HOLDINGREG_SLOT_NEWOP_FLAG)
		colorHolder.color = led_blue;
	else if (mb->holdingRegisters[address] & HOLDINGREG_SLOT_TAKEN_FLAG)
		colorHolder.color = led_green;
	else
		colorHolder.color = led_black;
	led_set_colorWithBrightness(address, colorHolder);
	LOG_DEBUG("Write single coil complete: address=%u, coil=%u, holding=0x%04X", address, mb->coils[address], mb->holdingRegisters[address]);
	return sendResponseWrite(mb, data);
}

HAL_StatusTypeDef Modbus_WriteMultipleCoils(ModbusInterface_t *mb, uint8_t *data) {
	uint16_t address = (data[2] << 8) | data[3];
	uint16_t count = (data[4] << 8) | data[5];
	uint8_t byteCount = data[6];
	uint8_t *values = (uint8_t *)calloc(byteCount, sizeof(uint8_t));
	for (uint16_t i = 0; i < byteCount; i++) {
		values[i] = data[7 + i];
	}
	LOG_DEBUG("Write multiple coils: startAddress=%u, count=%u", address, count);
	for (uint16_t i = address; i < address + count; i++) {
		uint8_t coilValue = (values[i / 8] >> (i % 8)) & 0x01;
		if (i >= mb->shiftReg.totalSlots) {
			LOG_WARN("(Write Multiple) Write single coil ignored: address=%u out of range (slots=%u)", i, mb->shiftReg.totalSlots);
			return HAL_ERROR;
		}
		LOG_DEBUG("(Write Multiple) Write single coil: address=%u, valueRaw=0x%02X", i, coilValue);
		mb->coils[i] = coilValue;
	}
	LOG_DEBUG("Write multiple coils complete: startAddress=%u, count=%u", address, count);
	return sendResponseWriteMultiple(mb, data);
}

/**
 * @brief Updates all Modbus registers based on current hardware state.
 * @details Refreshes discrete inputs from slot sensors, updates input registers
 *          with slot count, free count, and status, and syncs holding registers
 *          with current slot states and LED mode.
 * @param mb Pointer to the ModbusInterface_t structure.
 * @note Call periodically when idle.
 */
void updateRegisters(ModbusInterface_t *mb) {
	for (uint8_t slot = 0; slot < mb->shiftReg.totalSlots; slot++) {
		mb->discreteInputs[slot] =
		  ShiftRegister_GetSlotState(&mb->shiftReg, slot) ? 1 : 0;
	}

	mb->inputRegisters[0] = mb->shiftReg.totalSlots;
	mb->inputRegisters[1] = ShiftRegister_GetFreeCount(&mb->shiftReg);
	mb->inputRegisters[2] = mb->statusRegister;

	for (uint8_t slot = 0; slot < mb->shiftReg.totalSlots; slot++) {
		mb->holdingRegisters[slot] =
		  ShiftRegister_GetSlotState(&mb->shiftReg, slot) ? 1 : 0;
	}
	led_updateMode();
}

/**
 * @brief Sets the operation flag in the holding register for a specific slot.
 * @details Compares the coil state against the discrete input to determine whether
 *          a new operation is pending (NEWOP flag) or the slot is occupied (TAKEN flag).
 *          Updates the LED color for the slot accordingly. Resets LED mode to normal
 *          if it was in a special mode.
 * @param mb      Pointer to the ModbusInterface_t structure.
 * @param address 0-based slot address.
 */
void setOperationFlag(ModbusInterface_t *mb, uint16_t address) {
	LOG_DEBUG("Updating operation flag for slot %d: coil=%d, discreteInput=%d", address, mb->coils[address], mb->discreteInputs[address]);
	if (mb->coils[address] != mb->discreteInputs[address]) {
		mb->holdingRegisters[address] |= HOLDINGREG_SLOT_NEWOP_FLAG;
	} else {
		mb->holdingRegisters[address] = 0;
		if (mb->coils[address])
			mb->holdingRegisters[address] = HOLDINGREG_SLOT_TAKEN_FLAG;
	}

	mb->ledMode = LED_MODE_NORMAL;
	led_updateMode();
}

/**
 * @brief Parses and dispatches a received Modbus packet.
 * @details Reads the full packet from the circular buffer based on function code,
 *          extracts address, count, and value fields, calls the appropriate
 *          read/write handler, and sends the response.
 * @param mb   Pointer to the ModbusInterface_t structure.
 * @param data Buffer containing the packet.
 */
void processReceivedPackage(ModbusInterface_t *mb, uint8_t *data) {
	// TODO: Refactor this function to reduce code duplication and improve clarity. Consider creating helper functions for parsing requests and sending responses.
	// TODO: Add error handling for invalid packet formats, unsupported function codes, and out-of-range addresses/counts. Ensure that the Modbus exception response is sent in these cases.
	uint8_t functionCode = data[1];

	LOG_DEBUG("Processing Modbus function: 0x%02X", functionCode);
	switch (functionCode) {
		case MODBUS_FUNCTION_READ_COILS: // expected values for slots
			LOG_DEBUG("Function READ_COILS detected, preparing to read request data");
			if (Modbus_ReadCoils(mb, data) != HAL_OK) {
				LOG_WARN("READ_COILS handler failed");
				return;
			}
			break;
		case MODBUS_FUNCTION_READ_DISCRETE_INPUTS: // actual slot states
			LOG_DEBUG("Function READ_DISCRETE_INPUTS detected, preparing to read request data");
			if (Modbus_ReadDiscreteInputs(mb, data) != HAL_OK) {
				LOG_WARN("READ_DISCRETE_INPUTS handler failed");
				return;
			}
			break;
		case MODBUS_FUNCTION_READ_HOLDING_REGISTERS: // slot status + LED mode
			LOG_DEBUG("Function READ_HOLDING_REGISTERS detected, preparing to read request data");
			if (Modbus_ReadHoldingRegisters(mb, data) != HAL_OK) {
				LOG_WARN("READ_HOLDING_REGISTERS handler failed");
				return;
			}
			break;
		case MODBUS_FUNCTION_WRITE_SINGLE_COIL:
			LOG_DEBUG("Function WRITE_SINGLE_COIL detected, preparing to read request data");
			if (Modbus_WriteCoil(mb, data) != HAL_OK) {
				LOG_WARN("WRITE_SINGLE_COIL handler failed");
				return;
			}
			break;
		case MODBUS_FUNCTION_WRITE_SINGLE_HOLDING_REGISTER:
			LOG_DEBUG("Function WRITE_SINGLE_HOLDING_REGISTER detected, preparing to read request data");
			if (Modbus_WriteHoldingRegister(mb, data) != HAL_OK) {
				LOG_WARN("WRITE_SINGLE_HOLDING_REGISTER handler failed");
				return;
			}
			break;
		case MODBUS_FUNCTION_WRITE_MULTIPLE_HOLDING_REGISTERS:
			LOG_DEBUG("Function WRITE_MULTIPLE_HOLDING_REGISTERS detected, preparing to read request data");
			if (Modbus_WriteMultipleHoldingRegisters(mb, data) != HAL_OK) {
				LOG_WARN("WRITE_MULTIPLE_HOLDING_REGISTERS handler failed");
				return;
			}
			break;
		case MODBUS_FUNCTION_WRITE_MULTIPLE_COILS:
			LOG_DEBUG("Function WRITE_MULTIPLE_COILS detected, preparing to read request data");
			if (Modbus_WriteMultipleCoils(mb, data) != HAL_OK) {
				LOG_WARN("WRITE_MULTIPLE_COILS handler failed");
				return;
			}
			break;
		default:
			LOG_DEBUG("Function READ_INPUT_REGISTERS detected, preparing to read request data");
			if (Modbus_ReadInputRegisters(mb, data) != HAL_OK) {
				LOG_WARN("READ_INPUT_REGISTERS handler failed");
				return;
			}
			break;
	}
}

uint8_t checkCrc(const uint8_t *frame, uint16_t length) {
	uint16_t frameCrc;
	uint16_t calculatedCrc;

	if (frame == NULL || length < 4) {
		return 0; // Invalid frame
	}

	frameCrc = (uint16_t)(frame[length - 2]) | ((uint16_t)frame[length - 1] << 8);
	calculatedCrc = crc16((uint8_t *)frame, (uint16_t)(length - 2));

	return (frameCrc == calculatedCrc) ? 1 : 0;
}

/**
 * @brief Sends a Modbus read response over UART.
 * @details Builds a response frame: slave ID, function code, byte count, data bytes,
 *          and a CRC-16 checksum, then transmits it via UART DMA.
 * @param mb           Pointer to the ModbusInterface_t structure.
 * @param requestData  Original request buffer (used to echo function code).
 * @param byteCount    Number of data bytes in @p responseData.
 * @param responseData Pointer to the data bytes to include in the response.
 */
HAL_StatusTypeDef sendResponseRead(ModbusInterface_t *mb, uint8_t *requestData, uint8_t byteCount, uint8_t *responseData) {
	uint8_t response[256] = {0};
	response[0] = mb->slaveId;
	response[1] = requestData[1];
	response[2] = byteCount;
	memcpy(&response[3], responseData, byteCount);
	appendCrc(response, (uint16_t)(5 + byteCount));
	LOG_DEBUG("Sending read response: slaveId=%u, function=0x%02X, byteCount=%u, crc=0x%04X", mb->slaveId, requestData[1], byteCount, response[5 + byteCount - 2] | (response[5 + byteCount - 1] << 8));
	return startTx(response, (uint16_t)(5 + byteCount), 1000);
}

/**
 * @brief Sends a Modbus write response (echo) over UART.
 * @details Builds an 8-byte response frame by echoing the slave ID, function code,
 *          and the 4 address/value bytes from the request, appended with a CRC-16,
 *          then transmits it via UART DMA.
 * @param mb          Pointer to the ModbusInterface_t structure.
 * @param requestData Original request buffer to echo back.
 */
HAL_StatusTypeDef sendResponseWrite(ModbusInterface_t *mb, uint8_t *requestData) {
	uint8_t response[256] = {0};
	response[0] = mb->slaveId;
	response[1] = requestData[1];
	response[2] = 2;
	response[3] = requestData[4];
	response[4] = requestData[5];
	appendCrc(response, 7);
	LOG_DEBUG("Sending write response: slaveId=%u, function=0x%02X, address=%u, crc=0x%04X", mb->slaveId, requestData[1], (uint16_t)((requestData[2] << 8) | requestData[3]), response[5] | (response[6] << 8));

	return startTx(response, 7, 1000);
}

HAL_StatusTypeDef sendResponseWriteMultiple(ModbusInterface_t *mb, uint8_t *requestData) {
	uint8_t response[256] = {0};
	response[0] = mb->slaveId;
	response[1] = requestData[1];
	memcpy(&response[2], &requestData[2], 4);
	appendCrc(response, 8);
	LOG_DEBUG("Sending write multiple response: slaveId=%u, function=0x%02X, address=%u, count=%u, crc=0x%04X",
	          mb->slaveId, requestData[1], (uint16_t)((requestData[2] << 8) | requestData[3]), (uint16_t)((requestData[4] << 8) | requestData[5]), response[6] | (response[7] << 8));

	return startTx(response, 8, 1000);
}

HAL_StatusTypeDef sendExceptionResponse(ModbusInterface_t *mb, uint8_t functionCode, uint8_t exceptionCode) {
	uint8_t response[256] = {0};
	response[0] = mb->slaveId;
	response[1] = functionCode | 0x80; // Set MSB to indicate exception
	response[2] = exceptionCode;
	appendCrc(response, 5);
	LOG_DEBUG("Sending exception response: slaveId=%u, function=0x%02X, exceptionCode=0x%02X, crc=0x%04X", mb->slaveId, functionCode, exceptionCode, response[3] | (response[4] << 8));
	return startTx(response, 5, 1000);
}

HAL_StatusTypeDef startTx(const uint8_t *data, uint16_t len, uint32_t timeout) {
	HAL_StatusTypeDef status;
	if (len == 0 || len > 256) {
		LOG_ERROR("Modbus TX rejected: invalid length=%u", len);
		return HAL_ERROR;
	}
	USART1_DE_GPIO_Port->BSRR = USART1_DE_Pin;
	if ((status = HAL_UART_Transmit(&huart1, data, len, timeout)) != HAL_OK) {
		USART1_DE_GPIO_Port->BRR = USART1_DE_Pin;
		LOG_ERROR("Failed to start UART TX, state=%u error=0x%08lX",
		          huart1.gState, (unsigned long)status);
		return status;
	}
	USART1_DE_GPIO_Port->BRR = USART1_DE_Pin;
	LOG_DEBUG("UART TX done: length=%u", len);
	return HAL_OK;
}

void appendCrc(uint8_t *frame, uint16_t length) {
	uint16_t crc = crc16(frame, (uint16_t)(length - 2));
	frame[length - 2] = crc & 0xFF;
	frame[length - 1] = (crc >> 8) & 0xFF;
}
