/**
 * @file modbus_crc.h
 * @brief CRC16 calculation for Modbus
 * @details This file contains the function prototypes for the CRC16 calculation for Modbus.
 */
#ifndef MODBUS_CRC_H
#define MODBUS_CRC_H

#include "main.h"

uint16_t crc16(uint8_t* buffer, uint16_t buffer_length);

#endif /* MODBUS_CRC_H */