/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    usart.h
 * @brief   This file contains all the function prototypes for
 *          the usart.c file
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#include "modbus/circularBuffer.h"
#include "modbus/modbus_interface.h"
/* USER CODE END Includes */

extern UART_HandleTypeDef huart1;

extern UART_HandleTypeDef huart2;

/* USER CODE BEGIN Private defines */
extern uint8_t MODBUS_RXData[256];

typedef struct {
	uint32_t rxEventCount;
	uint32_t rxByteCount;
	uint32_t rxErrorCount;
	uint32_t rxOvrErrorCount;
	uint32_t rxStartCount;
	uint32_t rxStartFailCount;
	uint16_t lastRxSize;
	uint8_t lastRxEventType;
	uint8_t lastRxStartStatus;
	uint8_t lastRxState;
	uint8_t lastTxState;
	uint32_t lastErrorCode;
} ModbusRxDiag_t;
/* USER CODE END Private defines */

void MX_USART1_UART_Init(void);
void MX_USART2_UART_Init(void);

/* USER CODE BEGIN Prototypes */
void printSplashScreen(void);
void Modbus_StartRx(void);
ModbusRxDiag_t Modbus_GetRxDiag(void);
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */

