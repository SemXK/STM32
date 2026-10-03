#include "main.h"
#include "math.h"
#include "common.h"
#include <stdio.h>

UART_HandleTypeDef huart2;

void SystemClock_Config(void);

static void MX_USART2_UART_Init(void);

int main(void)
{

	HAL_Init();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
	MX_USART2_UART_Init();

    int count = 0;
    GPIO_PinState downButtonPressed = 0;
	GPIO_PinState upButtonPressed = 0;
    uint16_t outputPins[] = {
    		GPIO_PIN_0,
    		GPIO_PIN_1,
    		GPIO_PIN_2,
    		GPIO_PIN_3,
    		GPIO_PIN_4,
    		GPIO_PIN_5,
    		GPIO_PIN_6,
    		GPIO_PIN_7,
    };


    int bits = sizeof(outputPins) / sizeof(outputPins[0]);

    for(int i = 0; i < bits; i++) {
    	enablePin(GPIOC, outputPins[i]);
    }

    // Inputs
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;

    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);


    while (1) {
    	// Check buttons states
        downButtonPressed = HAL_GPIO_ReadPin(
        	GPIOA,
			GPIO_PIN_0
        );
        upButtonPressed = HAL_GPIO_ReadPin(
        	GPIOA,
			GPIO_PIN_1
        );


        if(downButtonPressed == GPIO_PIN_RESET) {
        	count = fmax(count - 1, 0);
        	printf("Conteggio corrente %d\r\n", count);
        }

        if(upButtonPressed == GPIO_PIN_RESET) {
        	count = fmin(count + 1, pow(2, bits) - 1);
        	printf("Conteggio corrente %d\r\n", count);
        }

    	// Pin Countout display
        for (int i = 0; i < bits; i++)
        {
            HAL_GPIO_WritePin(
                GPIOC,
                outputPins[i],
                (count & (1 << i)) ? GPIO_PIN_SET : GPIO_PIN_RESET
            );
        }

       HAL_Delay(100);
    }
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void) {

  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }


}


int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}
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


}
#endif /* USE_FULL_ASSERT */
