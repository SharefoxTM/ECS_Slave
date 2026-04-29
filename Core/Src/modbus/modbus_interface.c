#include "modbus/modbus_interface.h"
#include "led/led_interface.h"
#include "modbus/modbus_crc.h"

void setOperationFlag(ModbusInterface_t *mb, uint16_t address);
void processReceivedPackage(ModbusInterface_t *mb, uint8_t *data);
void sendResponseRead(ModbusInterface_t *mb, uint8_t *requestData, uint8_t byteCount, uint8_t *responseData);
void sendResponseWrite(ModbusInterface_t *mb, uint8_t *requestData);

uint8_t MODBUS_DMA_RXData[256];

/**
 * @brief Reads the Modbus configuration from hardware.
 * @details Reads the slave ID from the DIP switch and maps it to the range 1-16.
 *          Slot count is initialized to 0 and determined later by shift register detection.
 * @return ModbusConfig_t Populated configuration structure.
 */
ModbusConfig_t Modbus_ReadConfig(void) {
	ModbusConfig_t config = {0};

	config.slaveId = DipSwitch_GetSlaveId();

	// Slave ID range 1-16, map dipswitch 0-15 to 1-16
	config.slaveId = (config.slaveId == 0) ? 1 : (config.slaveId + 1);
	if (config.slaveId > 16)
		config.slaveId = 16;

	config.numSlots = 0;

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
	uint8_t pdata[256] = {0};
	hcbuf_modbus = cbuf_init(pdata, 256);
	if (hcbuf_modbus == NULL) {
		return mb;
	}

	mb.slaveId = slaveId;
	mb.shiftReg = ShiftRegister_Init();

	mb.coils = (uint8_t *)calloc(mb.shiftReg.totalSlots, sizeof(uint8_t));
	if (mb.coils == NULL) {
		return mb;
	}
	mb.discreteInputs =
	  (uint8_t *)calloc(mb.shiftReg.totalSlots, sizeof(uint8_t));
	if (mb.discreteInputs == NULL) {
		free(mb.coils);
		return mb;
	}
	mb.holdingRegisters =
	  (uint16_t *)calloc((mb.shiftReg.totalSlots + 1), sizeof(uint16_t));
	if (mb.holdingRegisters == NULL) {
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
	HAL_UARTEx_ReceiveToIdle_DMA(&huart1, MODBUS_DMA_RXData, 256);
	return mb;
}

/**
 * @brief Updates all Modbus registers based on current hardware state.
 * @details Refreshes discrete inputs from slot sensors, updates input registers
 *          with slot count, free count, and status, and syncs holding registers
 *          with current slot states and LED mode.
 * @param mb Pointer to the ModbusInterface_t structure.
 * @note Call periodically when idle.
 */
void Modbus_UpdateRegisters(ModbusInterface_t *mb) {
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

	mb->holdingRegisters[mb->shiftReg.totalSlots] = mb->ledMode;
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
	Modbus_UpdateRegisters(mb);
	mb->statusRegister &= ~STATUS_BIT_SCANNING;
}

/**
 * @brief Processes all pending data in the Modbus receive circular buffer.
 * @details Peeks at the first three bytes to identify the incoming packet, then
 *          dispatches to processReceivedPackage(). Flushes the buffer if a read
 *          error is encountered.
 * @param mb Pointer to the ModbusInterface_t structure.
 */
void Modbus_ProcessReceivedData(ModbusInterface_t *mb) {
	uint8_t data[256];
	uint32_t length;
	while (!cbuf_empty(hcbuf_modbus)) {
		for (int i = 0; i < 3; i++) {
			CB_Status_t err = cbuf_peek(hcbuf_modbus, data[i], i);
			if (err != CB_OK) {
				LOG_ERROR("Circular buffer get error: %d\r\nFlushing buffer to remove corrupted data", err);
				cbuf_flush(hcbuf_modbus); // Flush buffer to remove corrupted data
				return;
			}
		}
		processReceivedPackage(mb, data);
	}
}

/**
 * @brief Reads coil states into a packed byte array.
 * @details Packs up to @p count coil values starting at @p address (1-based)
 *          into @p output, with 8 coils per byte LSB-first.
 * @param mb      Pointer to the ModbusInterface_t structure.
 * @param address 1-based starting coil address.
 * @param count   Number of coils to read.
 * @param output  Buffer to write the packed coil bytes into.
 * @return Number of bytes written to @p output.
 */
uint8_t Modbus_ReadCoils(ModbusInterface_t *mb, uint16_t address, uint16_t count, uint8_t *output) {
	uint64_t coilState = 0;
	uint8_t byteCount = 0;
	address--; // Modbus addresses are 1-based, convert to 0-based index
	for (uint16_t i = address; i < mb->shiftReg.totalSlots && i < address + count; i++) {
		output[byteCount] |= mb->coils[i] ? (1 << (i - address)) : 0;
		if ((i - address) == 7) {
			byteCount++;
		}
	}
	return byteCount + 1;
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
void Modbus_WriteCoil(ModbusInterface_t *mb, uint16_t address, uint8_t value) {
	if (address >= mb->shiftReg.totalSlots) {
		return;
	}

	mb->coils[address] =
	  value ? 1 : 0; // Make sure all non-zero is 1 and zero is 0

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
	}

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

/**
 * @brief Reads discrete input states into an output buffer.
 * @param mb      Pointer to the ModbusInterface_t structure.
 * @param address 0-based starting discrete input address.
 * @param count   Number of discrete inputs to read.
 * @param output  Buffer to write the discrete input values into (one byte per input).
 * @return Number of values written, or 0 if @p address is out of range.
 */
uint8_t Modbus_ReadDiscreteInputs(ModbusInterface_t *mb, uint16_t address, uint16_t count, uint8_t *output) {
	if (address >= mb->shiftReg.totalSlots) {
		return 0;
	}
	for (uint8_t i = 0; i < count; i++) {
		output[i] = mb->discreteInputs[address + i];
	}
	return count;
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
uint16_t Modbus_ReadInputRegisters(ModbusInterface_t *mb, uint16_t address, uint16_t count, uint8_t *output) {
	if (address > 2) {
		return 0;
	}
	for (uint8_t i = 0; i < count; i++) {
		output[i] = mb->inputRegisters[address + i];
	}
	return count;
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
uint16_t Modbus_ReadHoldingRegisters(ModbusInterface_t *mb, uint16_t address, uint16_t count, uint8_t *output) {
	if (address > mb->shiftReg.totalSlots) {
		return 0;
	}
	for (uint8_t i = 0; i < count; i++) {
		output[i] = mb->holdingRegisters[address + i];
	}
	return count;
}

/**
 * @brief Writes a value to a single holding register.
 * @details If the target address is the LED mode register (index totalSlots),
 *          the LED mode is updated accordingly.
 * @param mb      Pointer to the ModbusInterface_t structure.
 * @param address 0-based holding register address.
 * @param value   Value to write.
 */
void Modbus_WriteHoldingRegister(ModbusInterface_t *mb, uint16_t address,
                                 uint16_t value) {
	if (address > mb->shiftReg.totalSlots) {
		return;
	}

	mb->holdingRegisters[address] = value;

	if (address == mb->shiftReg.totalSlots) {
		mb->ledMode = value;
	}
}

/**
 * @brief Validates a Modbus function code.
 * @param functionCode The function code byte from the received packet.
 * @return 0 if the function code is supported, 1 if it is unsupported.
 */
uint8_t Modbus_ValidateCode(uint8_t functionCode) {
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
			return 1; // Invalid function code
	}
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
	argb_t led_setting = {.brightness = LED_BRIGHTNESS_MEDIUM_HIGH};
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
			led_setting.color = led_blue;
		else if (mb->holdingRegisters[address] & HOLDINGREG_SLOT_TAKEN_FLAG)
			led_setting.color = led_green;
		else
			led_setting.color = led_black;
		led_set_color(address, led_setting);
	}
}

/**
 * @brief Parses and dispatches a received Modbus packet.
 * @details Reads the full packet from the circular buffer based on function code,
 *          extracts address, count, and value fields, calls the appropriate
 *          read/write handler, and sends the response.
 * @param mb   Pointer to the ModbusInterface_t structure.
 * @param data Buffer containing at least the first 3 bytes of the packet
 *             (slave ID, function code, length/data byte).
 */
void processReceivedPackage(ModbusInterface_t *mb, uint8_t *data) {
	uint8_t functionCode = data[1], byteCount;
	uint8_t pData[256] = {0};
	uint16_t address;
	uint16_t count;
	uint16_t value;
	switch (functionCode) {
		case MODBUS_FUNCTION_READ_COILS:
			cbuf_get(hcbuf_modbus, pData, 8);
			address = (pData[2] << 8) | pData[3];
			count = (pData[4] << 8) | pData[5];
			uint8_t output[((count) / 8) + 1] = {0};
			uint8_t byteCount = Modbus_ReadCoils(mb, address, count, output);
			sendResponseRead(mb, data, byteCount, output);
			break;
		case MODBUS_FUNCTION_READ_DISCRETE_INPUTS:
			cbuf_get(hcbuf_modbus, pData, 8);
			address = (pData[2] << 8) | pData[3];
			count = (pData[4] << 8) | pData[5];
			uint8_t output[((count) / 8) + 1] = {0};
			uint8_t byteCount = Modbus_ReadDiscreteInputs(mb, address, count, output);
			sendResponseRead(mb, data, byteCount, output);
			break;
		case MODBUS_FUNCTION_READ_HOLDING_REGISTERS:
			cbuf_get(hcbuf_modbus, data, data[2]);
			address = (data[2] << 8) | data[3];
			count = (data[4] << 8) | data[5];
			uint16_t output[count] = {0};
			uint8_t byteCount = Modbus_ReadHoldingRegisters(mb, address, count, (uint8_t *)output);
			sendResponseRead(mb, data, byteCount, (uint8_t *)output);
			break;
		case MODBUS_FUNCTION_WRITE_SINGLE_COIL:
			cbuf_get(hcbuf_modbus, data, 8);
			address = (data[2] << 8) | data[3];
			value = (data[6] << 8) | data[7];
			Modbus_WriteCoil(mb, address, value);
			sendResponseWrite(mb, data);
			break;
		case MODBUS_FUNCTION_WRITE_SINGLE_HOLDING_REGISTER:
			cbuf_get(hcbuf_modbus, data, 8);
			address = (data[2] << 8) | data[3];
			value = (data[6] << 8) | data[7];
			Modbus_WriteHoldingRegister(mb, address, value);
			sendResponseWrite(mb, data);
			break;
		case MODBUS_FUNCTION_WRITE_MULTIPLE_HOLDING_REGISTERS:
			cbuf_get(hcbuf_modbus, data, data[6] + 7);
			address = (data[2] << 8) | data[3];
			count = (data[4] << 8) | data[5];
			uint8_t *values = &data[6];
			Modbus_WriteMultipleHoldingRegisters(mb, address, count, values);
			sendResponseWrite(mb, data);
			break;
		case MODBUS_FUNCTION_WRITE_MULTIPLE_COILS:
			cbuf_get(hcbuf_modbus, data, data[6] + 7);
			address = (data[2] << 8) | data[3];
			count = (data[4] << 8) | data[5];
			uint8_t *values = &data[6];
			Modbus_WriteMultipleCoils(mb, address, count, values);
			sendResponseWrite(mb, data);
			break;
		default:
			// Not possible due to prior validation
			break;
	}
}

/**
 * @brief Sends a Modbus read response over UART.
 * @details Builds a response frame: slave ID, function code, byte count, data bytes,
 *          and a CRC-16 checksum, then transmits it blocking via UART.
 * @param mb           Pointer to the ModbusInterface_t structure.
 * @param requestData  Original request buffer (used to echo function code).
 * @param byteCount    Number of data bytes in @p responseData.
 * @param responseData Pointer to the data bytes to include in the response.
 */
void sendResponseRead(ModbusInterface_t *mb, uint8_t *requestData, uint8_t byteCount, uint8_t *responseData) {
	uint8_t response[256] = {0};
	response[0] = mb->slaveId;
	response[1] = requestData[1];
	response[2] = byteCount;
	memcpy(&response[3], responseData, byteCount);
	uint16_t crc = crc16(response, 3 + byteCount);
	response[3 + byteCount] = crc & 0xFF;
	response[4 + byteCount] = (crc >> 8) & 0xFF;

	HAL_UART_Transmit(&huart1, response, 5 + byteCount, HAL_MAX_DELAY);
}

/**
 * @brief Sends a Modbus write response (echo) over UART.
 * @details Builds an 8-byte response frame by echoing the slave ID, function code,
 *          and the 4 address/value bytes from the request, appended with a CRC-16,
 *          then transmits it blocking via UART.
 * @param mb          Pointer to the ModbusInterface_t structure.
 * @param requestData Original request buffer to echo back.
 */
void sendResponseWrite(ModbusInterface_t *mb, uint8_t *requestData) {
	uint8_t response[256] = {0};
	response[0] = mb->slaveId;
	response[1] = requestData[1];
	memcpy(&response[2], &requestData[2], 4);
	uint16_t crc = crc16(response, 6);
	response[6] = crc & 0xFF;
	response[7] = (crc >> 8) & 0xFF;

	HAL_UART_Transmit(&huart1, response, 8, HAL_MAX_DELAY);
}