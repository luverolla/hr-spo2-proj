/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file.
 *                   This file contains the common defines of the application.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2023 STMicroelectronics.
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
#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
typedef enum MachineState {
    MS_IDLE = 0x00U,
    MS_WAIT,
    MS_MEASURE,
    MS_END,
    MS_ERROR,
    MS_EXERCISE
} MachineState;

typedef struct MachineData {
    uint32_t heartRate;
    uint32_t oxygen;
    uint32_t confidence;
} MachineData;
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
#define MAX_MEASURE_TIME 30U // seconds
#define EXERCISE_TIME 20U    // seconds
#define TIME_RATIO 100U      // counts per second (timer here has resolution 10ms)
#define PAUSE_TIME 5U
#define OPT_MEASURES 100U

#define PRINT(str) (void)HAL_UART_Transmit(&huart2, (const uint8_t *)str, strlen(str), HAL_MAX_DELAY)

/**
 * @brief minimum observable values for oxygenation
 *
 * Values under this threshold are considered as errors
 * The value 0x000A corresponds to 10% oxygenation (LSB = 1%)
 */
#define MIN_MEASURABLE_OXY 0x000AU

/**
 * @brief minimum observable values for heart rate
 *
 * Values under this threshold are considered as errors
 * The value 0x0064 corresponds to 10 bpm (LSB = 0.1 bpm)
 */
#define MIN_MEASURABLE_HR 0x0064U

/**
 * @brief Low oxygenation threshold
 *
 * 0x5E corresponds to 94% oxygenation (LSB = 1%)
 */
#define LOW_OXY_THRES 0x5EU

/**
 * @brief High heart rate threshold
 *
 * 0x02EE corresponds to 75 bpm (LSB = 0.1 bpm)
 */
#define HIGH_HR_THRES 0x02EEU
#define MIN_UNCERT_THRES 0.1 // 1 = 100%
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
#define __EXPIRED(t, timeout) ((t) == ((timeout) * TIME_RATIO))
/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BUTTON_Pin GPIO_PIN_13
#define BUTTON_GPIO_Port GPIOC
#define BUTTON_EXTI_IRQn EXTI15_10_IRQn
#define LED_Pin GPIO_PIN_5
#define LED_GPIO_Port GPIOA
#define ERROR_Pin GPIO_PIN_7
#define ERROR_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
