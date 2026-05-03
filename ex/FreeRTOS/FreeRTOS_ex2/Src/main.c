/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "FreeRTOSConfig.h"
#include "adc.h"
#include "dma.h"
#include "stm32l4xx_hal.h"
#include "stm32l4xx_hal_adc.h"
#include "stm32l4xx_hal_tim.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
// --> include all necessary headers for
// printf() redirection
#include "stdio.h"
#include <stdint.h>

// FreeRTOS related headers
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
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

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

int _write(int file, char *ptr, int len) {
	HAL_UART_Transmit(&huart2, (uint8_t *) ptr, len, 50);
	return len;
}

enum QueueStatus {
	QueueOK, QueueWriteProblem, QueueEmpty, QueueCantRead
};

enum QueueMessages {
	QueueMsgNoData, QueueMsgNewData, QueueMsgNewDataChange,
};

uint16_t measurement;
uint8_t queueError = QueueOK;
SemaphoreHandle_t mutex;
QueueHandle_t queue;

void measureTask(void *args) {
	TickType_t xLastWakeTime;
  uint16_t measurement_local = 0;
	BaseType_t xStatus;

	xLastWakeTime = xTaskGetTickCount();

	for (;;) {
    measurement_local = HAL_ADC_GetValue(&hadc1);
    xStatus = xQueueSend(queue, &measurement_local, portMAX_DELAY);
    if(xStatus == pdPASS) {
      xSemaphoreTake(mutex, portMAX_DELAY);
      queueError = QueueOK;
      xSemaphoreGive(mutex);
    }
    else {
      xSemaphoreTake(mutex, portMAX_DELAY);
      queueError = QueueWriteProblem;
      xSemaphoreGive(mutex);
    }
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(300));
	}
}

void commTask(void *args) {
	TickType_t xLastWakeTime;
	uint16_t measurement_local = 0;
	uint16_t flag_local;
	uint16_t histeresis = 500;
	BaseType_t queue_size;
	BaseType_t xStatus;

	xLastWakeTime = xTaskGetTickCount();

	for (;;) {
    queue_size = uxQueueMessagesWaiting(queue);
    if(queue_size > 12) {
      histeresis = 100;
    }
    else if (queue_size == 0) {
      xSemaphoreTake(mutex, portMAX_DELAY);
      queueError = QueueEmpty;
      xSemaphoreGive(mutex);
    }
    else if (queue_size < 4) {
      histeresis = 500;
    }

    xSemaphoreTake(mutex, portMAX_DELAY);
    flag_local = queueError;
    xSemaphoreGive(mutex);

    xStatus = xQueueReceive(queue, &measurement_local, portMAX_DELAY);
    if(xStatus == pdPASS && flag_local == QueueOK) {
      xSemaphoreTake(mutex, portMAX_DELAY);
      queueError = QueueOK;
      xSemaphoreGive(mutex);
    }
    else {
      xSemaphoreTake(mutex, portMAX_DELAY);
      queueError = QueueCantRead;
      xSemaphoreGive(mutex);
    }


    printf("Measured value: %4u, time: %7lu, queue size %2lu, error %u\r\n",
      measurement_local, HAL_GetTick(), queue_size, flag_local);

    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(histeresis));
	}
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_TIM6_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */

	// --> start TIM1 to generate PWM signal on TIMER3 connector
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
	// --> start TIM6 in interrupt
  HAL_TIM_Base_Start_IT(&htim6);
	// --> start ADC1 in DMA mode
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *) &measurement, 1);
	// --> create a mutex
  mutex = xSemaphoreCreateMutex();
	// --> create a queue
  queue = xQueueCreate(15, sizeof(uint16_t));
	// --> create all necessary tasks
	printf("Starting!\r\n");
  xTaskCreate(measureTask, "measure", configMINIMAL_STACK_SIZE,
			NULL, tskIDLE_PRIORITY + 1, NULL);
	xTaskCreate(commTask, "comm", configMINIMAL_STACK_SIZE * 4,
			NULL, tskIDLE_PRIORITY + 2, NULL);

	// --> start FreeRTOS scheduler
  vTaskStartScheduler();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
    uint16_t local_measurement = HAL_ADC_GetValue(&hadc1);
    printf("Measured value: %4u, time: %7lu\r\n", local_measurement, HAL_GetTick());
    HAL_Delay(1000);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2|RCC_PERIPHCLK_ADC;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  PeriphClkInit.AdcClockSelection = RCC_ADCCLKSOURCE_PLLSAI1;
  PeriphClkInit.PLLSAI1.PLLSAI1Source = RCC_PLLSOURCE_HSI;
  PeriphClkInit.PLLSAI1.PLLSAI1M = 1;
  PeriphClkInit.PLLSAI1.PLLSAI1N = 8;
  PeriphClkInit.PLLSAI1.PLLSAI1P = RCC_PLLP_DIV7;
  PeriphClkInit.PLLSAI1.PLLSAI1Q = RCC_PLLQ_DIV2;
  PeriphClkInit.PLLSAI1.PLLSAI1R = RCC_PLLR_DIV2;
  PeriphClkInit.PLLSAI1.PLLSAI1ClockOut = RCC_PLLSAI1_ADC1CLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
	 tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
