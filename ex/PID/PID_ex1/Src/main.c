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
#include "adc.h"
#include "dac.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

// --> include all necessary headers for
// printf() redirection
#include "stdio.h"
// FreeRTOS related headers
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "pid.h"
#include <math.h>
#include <stdint.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

// PID gains
#define PID_KP   2.0f
#define PID_KI   8.0f
#define PID_KD   0.1f
#define PID_DT   0.02f   // control loop period: 50 Hz -> 0.02 s

// desired value range [0, 4095], selected by keys '0'..'9'
#define DV_STEP  4095/9
#define DV_MAX   4095

// DAC is 12-bit -> control signal range [0, 4095]
#define CS_MAX   4095.0f

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

// shared process values, protected by 'mutex'
uint16_t mv = 0;   // measured value
uint16_t dv = 0;   // desired value
uint16_t cs = 0;   // control signal

// sinus parameters
float a = 0.5;
float b = 0.1;
float c = 0;

// PID controller instance
PID_t pid;

// synchronization primitives
SemaphoreHandle_t mutex;     // guards mv, dv and cs
SemaphoreHandle_t adcReady;  // given by the ADC conversion-complete ISR
QueueHandle_t uartQueue;     // characters received over the serial port
uint8_t rxByte;              // single-byte buffer for HAL_UART_Receive_IT

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int _write(int file, char *ptr, int len) {
	HAL_UART_Transmit(&huart2, (uint8_t*) ptr, len, 50);
	return len;
}

int16_t convert_to_mV(int16_t value) {
	int16_t value_mV = 1000 * value * 3.3 / CS_MAX;
	return value_mV;
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
	if (hadc->Instance == ADC1) {
		BaseType_t hpw = pdFALSE;
		// tell the measure task that a fresh sample is ready
		xSemaphoreGiveFromISR(adcReady, &hpw);
		portYIELD_FROM_ISR(hpw);
	}
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance == USART2) {
		BaseType_t hpw = pdFALSE;
		// hand the received character to the user task
		xQueueSendToBackFromISR(uartQueue, &rxByte, &hpw);
		// re-arm reception for the next byte
		HAL_UART_Receive_IT(&huart2, &rxByte, 1);
		portYIELD_FROM_ISR(hpw);
	}
}

void measureTask(void *args) {
	TickType_t xLastWakeTime;

	xLastWakeTime = xTaskGetTickCount();

	for (;;) {
		// start a conversion and wait until the ISR signals it is done
		HAL_ADC_Start_IT(&hadc1);
		xSemaphoreTake(adcReady, portMAX_DELAY);
		uint16_t v = HAL_ADC_GetValue(&hadc1);

		xSemaphoreTake(mutex, portMAX_DELAY);
		mv = v;
		xSemaphoreGive(mutex);

		vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(20));
	}
}

void controlTask(void *args) {
	TickType_t xLastWakeTime;

	xLastWakeTime = xTaskGetTickCount();

	static float time = 0.0f;

	for (;;) {
		xSemaphoreTake(mutex, portMAX_DELAY);
		float dvLocal = dv;
		float mvLocal = mv;
		float a_local = a;
		float b_local = b;
		float c_local = c;
		xSemaphoreGive(mutex);

		time += PID_DT;
		float sinus_value_volts = a_local * sinf(b_local*time) + c_local;
		float sinus_value = sinus_value_volts * (CS_MAX/3.3f);

		xSemaphoreTake(mutex, portMAX_DELAY);
		dv = sinus_value;
		xSemaphoreGive(mutex);

		// PID output is already saturated to [0, CS_MAX]
		uint16_t csLocal = (uint16_t) PID_Update(&pid, sinus_value, mvLocal);

		HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, csLocal);

		xSemaphoreTake(mutex, portMAX_DELAY);
		cs = csLocal;
		xSemaphoreGive(mutex);

		vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(20));
	}
}

void commTask(void *args) {
	TickType_t xLastWakeTime;

	xLastWakeTime = xTaskGetTickCount();

	for (;;) {
		xSemaphoreTake(mutex, portMAX_DELAY);
		uint16_t mvLocal = mv;
		uint16_t dvLocal = dv;
		uint16_t csLocal = cs;
		xSemaphoreGive(mutex);
		int16_t errorLocal = dvLocal - mvLocal;

		int16_t mvLocal_mV = convert_to_mV(mvLocal);
		int16_t dvLocal_mV = convert_to_mV(dvLocal);
		int16_t csLocal_mV = convert_to_mV(csLocal);
		int16_t errorLocal_mV = convert_to_mV(errorLocal);


		printf("%4d;%4d;%4d;%4d\r\n", mvLocal_mV, dvLocal_mV, errorLocal_mV, csLocal_mV);

		vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(200));
	}
}

void userTask(void *args) {
	TickType_t xLastWakeTime;
	uint8_t character;

	xLastWakeTime = xTaskGetTickCount();

	for (;;) {
		while (xQueueReceive(uartQueue, &character, 0) == pdTRUE) {			
			if (character >= '0' && character <= '2') {
				float a_local;
				switch (character) {
					case '0':
						a_local = 0.5;
						break;
					case '1':
						a_local = 1.0;
						break;
					case '2':
						a_local = 2.0;
						break;
				}
				xSemaphoreTake(mutex, portMAX_DELAY);
				a = a_local;
				xSemaphoreGive(mutex);
			}
			if (character >= 'a' && character <= 'c') {
				float b_local;
				switch (character) {
					case 'a':
						b_local = 0.1;
						break;
					case 'b':
						b_local = 0.5;
						break;
					case 'c':
						b_local = 0.8;
						break;
				}
				xSemaphoreTake(mutex, portMAX_DELAY);
				b = b_local;
				xSemaphoreGive(mutex);
			}
			if (character >= 'x' && character <= 'z') {
				float c_local;
				switch (character) {
					case 'x':
						c_local = 0.0;
						break;
					case 'y':
						c_local = 1.0;
						break;
					case 'z':
						c_local = 1.65;
						break;
				}
				xSemaphoreTake(mutex, portMAX_DELAY);
				c = c_local;
				xSemaphoreGive(mutex);
			}
		}

		vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
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
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_TIM6_Init();
  MX_DAC1_Init();
  /* USER CODE BEGIN 2 */

	// Interrupts that call FreeRTOS ...FromISR() APIs must run at a priority
	// >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (5). Re-apply it here so a
	// CubeMX regeneration cannot silently reset the priorities (set in
	// adc.c / usart.c) back to 0 and hang the scheduler.
	HAL_NVIC_SetPriority(ADC1_2_IRQn, 5, 0);
	HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);

	// calibrate the ADC for accurate single-ended measurements
	HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);

	// create all necessary synchronization mechanisms
	mutex = xSemaphoreCreateMutex();
	configASSERT(mutex != NULL);
	adcReady = xSemaphoreCreateBinary();
	configASSERT(adcReady != NULL);
	uartQueue = xQueueCreate(8, sizeof(uint8_t));
	configASSERT(uartQueue != NULL);

	// initialize the PID controller (output limited to the DAC range)
	PID_Init(&pid, PID_KP, PID_KI, PID_KD, PID_DT, 0.0f, CS_MAX);

	// start the DAC output channel
	HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);

	// enable UART receive in interrupt mode (one byte at a time)
	HAL_UART_Receive_IT(&huart2, &rxByte, 1);

	printf("Starting!\r\n");

	// create all necessary tasks
	xTaskCreate(measureTask, "measure", configMINIMAL_STACK_SIZE,
			NULL, tskIDLE_PRIORITY + 3, NULL);
	xTaskCreate(controlTask, "control", configMINIMAL_STACK_SIZE,
			NULL, tskIDLE_PRIORITY + 3, NULL);
	xTaskCreate(userTask, "user", configMINIMAL_STACK_SIZE,
			NULL, tskIDLE_PRIORITY + 2, NULL);
	xTaskCreate(commTask, "comm", configMINIMAL_STACK_SIZE * 4,
			NULL, tskIDLE_PRIORITY + 1, NULL);

	// start FreeRTOS scheduler
	vTaskStartScheduler();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
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
