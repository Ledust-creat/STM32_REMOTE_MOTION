/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include "i2c.h"
#include "rtc.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "App_Monitor.h"
#include "BSP_UART.h"
#include "ML307C.h"
#include "ML307C_Config.h"
#include "RCWL0515.h"
#include "SHT30.h"
#include <string.h>
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

static uint8_t g_ml307c_raw_buffer[128];  /* 模块原始 AT 回传的主循环转发缓冲区。 */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  /* 临时振动台测试：只读取 PB6，并通过 USART1 输出雷达 OUT 电平变化。 */
  (void)BSP_UART_Write(BSP_UART_CHANNEL_DEBUG,
                       (const uint8_t *)"RCWL-0515 raw level test started.\r\n",
                       (uint16_t)strlen("RCWL-0515 raw level test started.\r\n"));

#if 0
  /* 原正式业务初始化；振动台测试完成后恢复本段。 */
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  MX_RTC_Init();
  RCWL0515_Init();
  (void)SHT30_Init();
  (void)ML307C_Init(HAL_GetTick());
  App_Monitor_Init();
#endif
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

#if 0
	  uint32_t now_tick_ms = HAL_GetTick();
	  uint16_t modem_read_size;

	  ML307C_Update(now_tick_ms);
	  modem_read_size = ML307C_ReadRaw(g_ml307c_raw_buffer, sizeof(g_ml307c_raw_buffer));
	  if (modem_read_size > 0U)
	  {
		  /* 先交给 ML307C 解析状态机；此步骤不能因关闭调试镜像而省略。 */
		  ML307C_OnReceive(g_ml307c_raw_buffer, modem_read_size, now_tick_ms);

#if (ML307C_DEBUG_ENABLED != 0U)
		  /* 仅开发阶段将同一批模块原始回传镜像到电脑 USART1。 */
		  (void)BSP_UART_Write(BSP_UART_CHANNEL_DEBUG, g_ml307c_raw_buffer, modem_read_size);
#endif
	  }

	  App_Monitor_Update(now_tick_ms, RCWL0515_IsMotionActive());
#endif

    /* 固定周期输出实时电平，便于观察振动过程中 OUT 的保持和回落。 */
    GPIO_PinState rcwl_current_level = HAL_GPIO_ReadPin(RCWL_0515_GPIO_Port,
                                                        RCWL_0515_Pin);
    if (rcwl_current_level == GPIO_PIN_SET)
    {
      (void)BSP_UART_Write(BSP_UART_CHANNEL_DEBUG,
                           (const uint8_t *)"RCWL-0515 OUT: HIGH (MOTION)\r\n",
                           (uint16_t)strlen("RCWL-0515 OUT: HIGH (MOTION)\r\n"));
    }
    else
    {
      (void)BSP_UART_Write(BSP_UART_CHANNEL_DEBUG,
                           (const uint8_t *)"RCWL-0515 OUT: LOW (NO MOTION)\r\n",
                           (uint16_t)strlen("RCWL-0515 OUT: LOW (NO MOTION)\r\n"));
    }

    HAL_Delay(200U);

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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

#if 0
/* 原正式业务中断回调；振动台测试期间不启用。 */
/*
*********************************************************************************************************
*   函 数 名: HAL_GPIO_EXTI_Callback
*   功能说明: RCWL-0515 的上升沿中断回调；记录设备事件并通知 APP 从 STOP 醒来后的处理。
*   形    参: GPIO_Pin - 触发中断的 GPIO 引脚掩码。
*   返 回 值: 无
*********************************************************************************************************
*/
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == RCWL_0515_Pin)
  {
    RCWL0515_OnRisingEdge(HAL_GetTick());
    App_Monitor_OnMotion();
  }
}

/*
*********************************************************************************************************
*   函 数 名: HAL_UART_RxCpltCallback
*   功能说明: USART2 接收完成回调；仅将 ML307C 单字节交给 UART BSP 环形缓冲并重启接收。
*   形    参: huart - 本次接收完成的 UART 句柄。
*   返 回 值: 无
*********************************************************************************************************
*/
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart2)
  {
    BSP_UART_OnModemRxComplete();
  }
}

/*
*********************************************************************************************************
*   函 数 名: HAL_RTC_AlarmAEventCallback
*   功能说明: RTC Alarm A HAL 回调；只向 APP 投递定时唤醒事件，不执行串口或传感器业务。
*   形    参: rtc_handle - RTC HAL 句柄。
*   返 回 值: 无
*********************************************************************************************************
*/
void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *rtc_handle)
{
  if (rtc_handle == &hrtc)
  {
    App_Monitor_OnRtcAlarm();
  }
}
#endif

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
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
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
