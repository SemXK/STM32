#include "main.h"
#include "common.h"
#include "stdio.h"
#include "string.h"
#include "stdint.h"
#include "ESP8266_STM32.h"

#define USER_DEBUG 1

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
void
SystemClock_Config(void);
static void
MX_USART1_UART_Init(void);
static void
MX_USART2_UART_Init(void);

int main(void) {
	HAL_Init();
	SystemClock_Config();
	MX_USART1_UART_Init();   // ESP8266
	MX_USART2_UART_Init();   // Tera Term

	//Connection check with Teraterm
	const char msg[] = "\r\nSTM32 starting ESP8266...\r\n";
	HAL_UART_Transmit(&huart2, (uint8_t*) msg, strlen(msg), 100);

	HAL_Delay(2000);

	/*
	 * Initialize ESP8266
	 */
	if (ESP_Init() != ESP8266_OK) {
		const char error[] = "ERROR: ESP8266 initialization failed\r\n";

		HAL_UART_Transmit(&huart2, (uint8_t*) error, strlen(error),
		HAL_MAX_DELAY);

		// Send a response back to the ESP8266
		const char response[] = "OK\r\n";

		HAL_UART_Transmit(&huart1, (uint8_t*) response, strlen(response), 1000);

		Error_Handler();
	}

	/*
	 * Connect to WiFi and get IP
	 */

	char ipBuffer[20] = { 0 };
	if (ESP_ConnectWiFi("WINDTRE - 665A90", "MagliettaRossa123!", ipBuffer,
			sizeof(ipBuffer)) != ESP8266_OK) {
		const char error[] = "ERROR: WiFi connection failed\r\n";
		HAL_UART_Transmit(&huart2, (uint8_t*) error, strlen(error),
		HAL_MAX_DELAY);
		Error_Handler();
	}

	// Set ESP as listening Server
	const char muxSettings[] = "AT+CIPMUX=1\r\n";
	const char serverSettings[] = "AT+CIPSERVER=1,80\r\n";

	if (ESP_SendCommand(muxSettings, "OK", 100) != ESP8266_OK) {
		const char muxEr[] = "ERROR: Mux initialization failed\r\n";

		HAL_UART_Transmit(&huart2, (uint8_t*) muxEr, strlen(muxEr),
		HAL_MAX_DELAY);
		Error_Handler();
	}
	if (USER_DEBUG) {
		HAL_UART_Transmit(&huart2, (uint8_t*) "MUX READY\r\n",
				strlen("MUX READY\r\n"), 1000);
	}

	if (ESP_SendCommand(serverSettings, "OK", 100) != ESP8266_OK) {
		const char tcpErr[] = "ERROR: TCP Server initialization failed\r\n";
		HAL_UART_Transmit(&huart2, (uint8_t*) tcpErr, strlen(tcpErr),
		HAL_MAX_DELAY);
		Error_Handler();
	}
	if (USER_DEBUG) {
		HAL_UART_Transmit(&huart2, (uint8_t*) "TCP READY\r\n",
				strlen("TCP READY\r\n"), 1000);
	}
	// Give back the ESP's IP

	char output[64];
	snprintf(output, sizeof(output), "ESP8266 IP: %s\r\n", ipBuffer);
	HAL_UART_Transmit(&huart2, (uint8_t*) output, strlen(output), 1000);

	char rxBuffer[512];
	uint16_t rxLen = 0;

	while (1)
	{
	    uint8_t byte;

	    // Stand-by mode (nothind arrived)
	    if (HAL_UART_Receive(&huart1, &byte, 1, 100) != HAL_OK) continue;

	    // Reset rx buffer if it overflows
	    if (rxLen >= sizeof(rxBuffer) - 1) rxLen = 0;

	    // Add the recieved byte to the buffer
	    rxBuffer[rxLen++] = byte;
	    rxBuffer[rxLen] = '\0';

	    // Add the row containing connectionId and lenght of data
	    char *ipd = strstr(rxBuffer, "+IPD,");
	    if (ipd == NULL)
	    {
	        if (rxLen > 4)
	        {
	            memmove(rxBuffer, rxBuffer + rxLen - 4, 4);
	            rxLen = 4;
	            rxBuffer[rxLen] = '\0';
	        }
	        continue;
	    }

	    unsigned int connectionId, payloadLen;
	    int headerLen;

	    if (sscanf(
				ipd,
				"+IPD,%u,%u:%n",
				 &connectionId,
				 &payloadLen,
				 &headerLen
			 ) != 2)
	        continue;

	    char *colon = strchr(ipd, ':');

	    // Header is missing (possible ping)
	    if (colon == NULL) continue;

	    size_t headerBytes = (size_t)(colon + 1 - ipd);
	    size_t offset = (size_t)(ipd - rxBuffer);
	    size_t received = rxLen - offset - headerBytes;

	    //Stand by (payload is either missing or overflowing or 5+ devices connected)
	    if (payloadLen == 0 || payloadLen > 400 || connectionId > 4) {
	        rxLen = 0;
	        continue;
	    }


	    if (received < payloadLen) continue;
	    char *request = colon + 1;

	    // Print the incoming HTTP request
	    HAL_UART_Transmit(&huart2, (uint8_t *)request, payloadLen, 1000);

	    // Respond to GET requests only
	    if (payloadLen >= 4 && strncmp(request, "GET ", 4) == 0)
	    {
	    	ESP_SendHttpResponse((uint8_t)connectionId);
	    }

	    // Remove the processed +IPD header and payload from memory
	    size_t consumed = offset + headerBytes + payloadLen;
	    memmove(rxBuffer, rxBuffer + consumed, rxLen - consumed);
	    rxLen -= consumed;
	    rxBuffer[rxLen] = '\0';
	}
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };
	RCC_PeriphCLKInitTypeDef PeriphClkInit = { 0 };

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL12;
	RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
		Error_Handler();
	}
	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
	PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK1;
	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void) {

	/* USER CODE BEGIN USART1_Init 0 */

	/* USER CODE END USART1_Init 0 */

	/* USER CODE BEGIN USART1_Init 1 */

	/* USER CODE END USART1_Init 1 */
	huart1.Instance = USART1;
	huart1.Init.BaudRate = 9600;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;
	huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART1_Init 2 */

	/* USER CODE END USART1_Init 2 */

}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void) {

	/* USER CODE BEGIN USART2_Init 0 */

	/* USER CODE END USART2_Init 0 */

	/* USER CODE BEGIN USART2_Init 1 */

	/* USER CODE END USART2_Init 1 */
	huart2.Instance = USART2;
	huart2.Init.BaudRate = 9600;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.OverSampling = UART_OVERSAMPLING_16;
	huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart2) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART2_Init 2 */

	/* USER CODE END USART2_Init 2 */

}

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
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
