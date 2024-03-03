/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "gpio.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stddef.h>
#include <string.h>

#include "hal_utils.h"

#include "ds1307rtc.h"
#include "max32664.h"
#include "ssd1306.h"
#include "strfmt.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// state
static MachineState state = MS_IDLE;

static uint32_t measureCount = 0;
static uint32_t cicleCount = 0;

static MachineData average;
static MachineData maximum;
static MachineData minimum;

static uint8_t oled_written = 0;
static strbuf msgBuf;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void putDate(strbuf *buffer, date_time_t dt);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static char buf[1] = {'\0'};
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
    /* USER CODE BEGIN 1 */
    static MAX32664_Handle pox;

    uint8_t flag_finger_on_p = 0;
    /* USER CODE END 1 */

    /* MCU Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    (void)HAL_Init();

    /* USER CODE BEGIN Init */
    char msgBufStr[200];
    msgBuf = mkbuf(msgBufStr);
    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_I2C1_Init();
    MX_TIM2_Init();
    MX_USART2_UART_Init();
    MX_TIM10_Init();
    MX_TIM3_Init();
    /* USER CODE BEGIN 2 */
    PRINT((const char *)"\r\nSystem init...");

    (void)HAL_TIM_Base_Start_IT(&htim3);
    (void)HAL_TIM_Base_Start_IT(&htim10);

    // devices creation
    GPIO_Line PC0 = {.port = GPIOC, .pin = GPIO_PIN_0};
    GPIO_Line PC1 = {.port = GPIOC, .pin = GPIO_PIN_1};
    MAX32664_Init(&pox, &hi2c1, &PC0, &PC1, 0x55);

    // devices init/start
    ssd1306_Init();
    (void)ds1307rtc_init();
    (void)MAX32664_Begin(&pox);

    date_time_t dt = {.year = 23, .month = 6, .date = 12, .hours = 8, .minutes = 21, .seconds = 0};
    (void)ds1307rtc_set_date_time(&dt);

    date_time_t test;
    (void)ds1307rtc_get_date_time(&test);
    str_clear(&msgBuf);
    put_str(&msgBuf, "\r\nDatetime: ");
    putDate(&msgBuf, test);
    put_end(&msgBuf);
    PRINT(msgBuf.buf);

    // Configuring just the BPM settings.
    uint8_t error = MAX32664_ConfigBpm(&pox, MODE_ONE);
    if (error == (uint8_t)SB_SUCCESS) {
        PRINT("\r\nSensor configured correctly");
    } else {
        str_clear(&msgBuf);
        put_str(&msgBuf, "\r\nError during configuration with status code ");
        put_uint8(&msgBuf, error);
        PRINT(msgBuf.buf);
    }

    PRINT("\r\nLoading sensor data...");
    // Data lags a bit behind the sensor, if you're finger is on the sensor when
    // it's being configured this delay will give some time for the data to catch
    // up.
    HAL_Delay(4000);
    PRINT("\r\nOk, sensor ready");
    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1) {
        switch (state) {
        case MS_WAIT: {
            bioData poxData = MAX32664_ReadBpm(&pox);
            flag_finger_on_p = (poxData.status == 3U) ? 0xFFU : 0x00U;
            if (flag_finger_on_p > 0x00U) {
                ssd1306_Fill(Black);
                ssd1306_SetCursor(0, 0);
                (void)ssd1306_WriteCString("Measuring", Font_7x10, White);
                ssd1306_UpdateScreen();
                PRINT("\r\nOk, measuring");
                state = MS_MEASURE;
            }
            break;
        }
        case MS_MEASURE: {
            bioData poxData = MAX32664_ReadBpm(&pox);
            if ((poxData.heartRate < MIN_MEASURABLE_HR) || (poxData.oxygen < MIN_MEASURABLE_OXY)) {
                break;
            }
            average.oxygen += poxData.oxygen;
            average.heartRate += poxData.heartRate;
            average.confidence += poxData.confidence;

            if (poxData.heartRate > maximum.heartRate) {
                maximum.heartRate = poxData.heartRate;
            }
            if ((uint32_t)(poxData.heartRate) < minimum.heartRate) {
                minimum.heartRate = poxData.heartRate;
            }

            if (poxData.oxygen > maximum.oxygen) {
                maximum.oxygen = poxData.oxygen;
            }
            if (poxData.oxygen < minimum.oxygen) {
                minimum.oxygen = poxData.oxygen;
            }

            if (poxData.confidence > maximum.confidence) {
                maximum.confidence = poxData.confidence;
            }
            if (poxData.confidence < minimum.confidence) {
                minimum.confidence = poxData.confidence;
            }

            measureCount += 1U;
            break;
        }
        default:
            break;
        }

        // pox sensor delay
        HAL_Delay(40);
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Configure the main internal regulator output voltage
     */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
        Error_Handler();
    }
}

/* USER CODE BEGIN 4 */
static void putDate(strbuf *buffer, date_time_t dt) {
    put_uint8(buffer, dt.date);
    put_char(buffer, '/');
    put_uint8(buffer, dt.month);
    put_char(buffer, '/');
    put_uint16(buffer, dt.year);
    put_char(buffer, ' ');
    put_uint8(buffer, dt.hours);
    put_char(buffer, ':');
    put_uint8(buffer, dt.minutes);
    put_char(buffer, ':');
    put_uint8(buffer, dt.seconds);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim == &htim10) {
        static uint32_t timeCount = 0;
        static uint8_t led_dir = 0;
        static uint32_t led_pulse = 0;

        if (state == MS_MEASURE) {
            if (__EXPIRED(timeCount, MAX_MEASURE_TIME)) {
                timeCount = 0U;
                state = MS_END;
                date_time_t curr = {0};
                (void)ds1307rtc_get_date_time(&curr);
                str_clear(&msgBuf);
                put_str(&msgBuf, "\r\nReport [");
                putDate(&msgBuf, curr);
                put_str(&msgBuf, "]");
                put_end(&msgBuf);
                PRINT(msgBuf.buf);

                str_clear(&msgBuf);
                put_str(&msgBuf, "\r\nobtained ");
                if (measureCount < OPT_MEASURES) {
                    put_uint32(&msgBuf, measureCount);
                    put_str(&msgBuf, "/");
                    put_uint32(&msgBuf, OPT_MEASURES);
                    put_str(&msgBuf, " good samples -> discard");
                } else {
                    put_str(&msgBuf, "\r\nobtained ");
                    put_uint32(&msgBuf, measureCount);
                    put_str(&msgBuf, "/");
                    put_uint32(&msgBuf, OPT_MEASURES);
                    put_str(&msgBuf, " good samples -> accept");
                }
                put_end(&msgBuf);
                PRINT(msgBuf.buf);

                if (measureCount < OPT_MEASURES) {
                    state = MS_ERROR;
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);
                    ssd1306_Fill(Black);
                    ssd1306_SetCursor(0, 0);
                    (void)ssd1306_WriteCString("Invalid measure", Font_7x10, White);
                    ssd1306_SetCursor(0, 15);
                    (void)ssd1306_WriteCString("Repeat", Font_7x10, White);
                    ssd1306_SetCursor(0, 0);
                    ssd1306_UpdateScreen();
                } else {
                    average.oxygen /= measureCount;
                    average.heartRate /= measureCount;
                    average.confidence /= measureCount;

                    str_clear(&msgBuf);
                    put_str(&msgBuf, "\r\nHr: ");
                    put_uint32(&msgBuf, average.heartRate);
                    put_str(&msgBuf, ", Ox: ");
                    put_uint32(&msgBuf, average.oxygen);
                    put_str(&msgBuf, ", Conf: ");
                    put_uint32(&msgBuf, average.confidence);
                    put_end(&msgBuf);
                    PRINT(msgBuf.buf);

                    // uncertainty computation
                    MachineData unc;
                    unc.heartRate = (maximum.heartRate - minimum.heartRate) / 2U;
                    unc.oxygen = (maximum.oxygen - minimum.oxygen) / 2U;
                    unc.heartRate = unc.heartRate / average.heartRate;
                    unc.oxygen = unc.oxygen / average.oxygen;

                    if ((unc.heartRate <= MIN_UNCERT_THRES) || (unc.oxygen <= MIN_UNCERT_THRES)) {
                        state = MS_ERROR;
                        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);
                        ssd1306_Fill(Black);
                        ssd1306_SetCursor(0, 0);
                        (void)ssd1306_WriteCString("Invalid measure", Font_7x10, White);
                        ssd1306_SetCursor(0, 15);
                        (void)ssd1306_WriteCString("Repeat", Font_7x10, White);
                        ssd1306_SetCursor(0, 0);
                        ssd1306_UpdateScreen();
                    } else if (average.heartRate > HIGH_HR_THRES) {
                        state = MS_EXERCISE;
                        PRINT("\r\nBreath exercise mode");
                        ssd1306_Fill(Black);
                        ssd1306_SetCursor(0, 0);
                        (void)ssd1306_WriteCString("Exercise mode", Font_7x10, White);
                        ssd1306_UpdateScreen();
                        (void)HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
                    } else {
                        // write pox data
                        char tmpStr[30];
                        strbuf tmp = mkbuf(tmpStr);

                        ssd1306_Fill(Black);
                        ssd1306_SetCursor(0, 0);

                        str_clear(&tmp);
                        put_str(&tmp, "Hr: ");
                        put_uint32(&tmp, average.heartRate);
                        put_str(&tmp, " bpm");
                        put_end(&tmp);
                        (void)ssd1306_WriteString(tmp.buf, Font_7x10, White);
                        ssd1306_SetCursor(0, 15);

                        str_clear(&tmp);
                        put_str(&tmp, "Ox: ");
                        put_uint32(&tmp, average.oxygen);
                        put_str(&tmp, " perc");
                        put_end(&tmp);
                        (void)ssd1306_WriteString(tmp.buf, Font_7x10, White);
                        ssd1306_SetCursor(0, 30);

                        str_clear(&tmp);
                        put_str(&tmp, "Cf: ");
                        put_uint32(&tmp, average.confidence);
                        put_str(&tmp, " perc");
                        put_end(&tmp);
                        (void)ssd1306_WriteString(tmp.buf, Font_7x10, White);
                        ssd1306_SetCursor(0, 0);
                        ssd1306_UpdateScreen();
                        state = MS_END;
                    }
                }
            }

            else {
                timeCount++;
            }
        }

        else if (state == MS_EXERCISE) {
            if (__EXPIRED(timeCount, EXERCISE_TIME)) {
                timeCount = 0;
                (void)HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_2);
                state = MS_WAIT;
            } else {
                if (led_dir == 0U) {
                    led_pulse += 4U;
                } else {
                    led_pulse -= 4U;
                }

                if ((led_pulse == 0U) || (led_pulse >= 999U)) {
                    led_dir = (led_dir == 0U) ? 1U : 0U;
                }

                __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, led_pulse);
                timeCount += 1U;
            }
        }

        else if ((state == MS_ERROR) || (state == MS_END)) {
            if (__EXPIRED(timeCount, PAUSE_TIME)) {
                timeCount = 0;
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
                ssd1306_Fill(Black);
                ssd1306_SetCursor(0, 0);
                (void)ssd1306_WriteCString("Put finger", Font_7x10, White);
                ssd1306_SetCursor(0, 15);
                (void)ssd1306_WriteCString("on sensors", Font_7x10, White);
                ssd1306_UpdateScreen();
                state = MS_WAIT;
            }

            timeCount++;
        } else {
            // do nothing
        }
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GPIO_PIN_13) {
        if (state == MS_IDLE) {
            USART_PRINT("\r\nDevice is on");
            state = MS_WAIT;
            ssd1306_Fill(Black);
            (void)ssd1306_WriteCString("Put finger", Font_7x10, White);
            ssd1306_SetCursor(0, 15);
            (void)ssd1306_WriteCString("on sensors", Font_7x10, White);
            ssd1306_UpdateScreen();
        }
    }
}
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1) {
    }
    /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line) {
    /* USER CODE BEGIN 6 */
    (void)file;
    (void)line;
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
