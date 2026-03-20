/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file.
 *                   This file contains the common defines of the application.
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
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#define LOG_LEVEL_VERBOSE
#include "Utilities/log.h"
#include <stdlib.h>
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void delay_250ns(uint32_t cycles);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_Pin GPIO_PIN_13
#define LED_GPIO_Port GPIOC
#define DIP_0_Pin GPIO_PIN_3
#define DIP_0_GPIO_Port GPIOA
#define DIP_1_Pin GPIO_PIN_4
#define DIP_1_GPIO_Port GPIOA
#define DIP_2_Pin GPIO_PIN_5
#define DIP_2_GPIO_Port GPIOA
#define DIP_3_Pin GPIO_PIN_6
#define DIP_3_GPIO_Port GPIOA
#define DIP_4_Pin GPIO_PIN_7
#define DIP_4_GPIO_Port GPIOA
#define DIP_5_Pin GPIO_PIN_0
#define DIP_5_GPIO_Port GPIOB
#define DIP_6_Pin GPIO_PIN_1
#define DIP_6_GPIO_Port GPIOB
#define DIP_7_Pin GPIO_PIN_2
#define DIP_7_GPIO_Port GPIOB
#define Button_Pin GPIO_PIN_10
#define Button_GPIO_Port GPIOB
#define SR_SDI_Pin GPIO_PIN_4
#define SR_SDI_GPIO_Port GPIOB
#define SR_CLK_Pin GPIO_PIN_5
#define SR_CLK_GPIO_Port GPIOB
#define SR_Latch_Pin GPIO_PIN_6
#define SR_Latch_GPIO_Port GPIOB
#define SR_SDO_Pin GPIO_PIN_7
#define SR_SDO_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
