/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
extern I2C_HandleTypeDef hi2c1;

extern I2C_HandleTypeDef hi2c3;
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LCD_DC_Pin GPIO_PIN_10
#define LCD_DC_GPIO_Port GPIOI
#define LCD_BL_Pin GPIO_PIN_11
#define LCD_BL_GPIO_Port GPIOI
#define LED2_Pin GPIO_PIN_2
#define LED2_GPIO_Port GPIOA
#define LED1_Pin GPIO_PIN_4
#define LED1_GPIO_Port GPIOH
#define ENC_KEY_Pin GPIO_PIN_9
#define ENC_KEY_GPIO_Port GPIOH
#define ENC_SIG1_Pin GPIO_PIN_6
#define ENC_SIG1_GPIO_Port GPIOC
#define ENC_SIG2_Pin GPIO_PIN_7
#define ENC_SIG2_GPIO_Port GPIOC
#define CARD_DETECT_Pin GPIO_PIN_3
#define CARD_DETECT_GPIO_Port GPIOI
#define I_O_1_Pin GPIO_PIN_3
#define I_O_1_GPIO_Port GPIOD
#define I_O_2_Pin GPIO_PIN_4
#define I_O_2_GPIO_Port GPIOD
#define I_0_4_Pin GPIO_PIN_8
#define I_0_4_GPIO_Port GPIOB
#define I_0_3_Pin GPIO_PIN_9
#define I_0_3_GPIO_Port GPIOB
#define CTP_RST_Pin GPIO_PIN_4
#define CTP_RST_GPIO_Port GPIOI
#define CTP_INT_Pin GPIO_PIN_5
#define CTP_INT_GPIO_Port GPIOI
#define LCD_RST_Pin GPIO_PIN_6
#define LCD_RST_GPIO_Port GPIOI
#define LCD_CS_Pin GPIO_PIN_7
#define LCD_CS_GPIO_Port GPIOI

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
