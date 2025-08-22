#include "main.h"

// prototypes

void SetupOutputGPIO(void);
void SetupInputGPIO(void);
void turnOffLeds(void);

//constants
const uint16_t PINS[] = {
	GPIO_PIN_0,
	GPIO_PIN_1,
	GPIO_PIN_2,
	GPIO_PIN_3,
	GPIO_PIN_4,
	GPIO_PIN_5,
	GPIO_PIN_6,
	GPIO_PIN_7,
};
const uint16_t INPUT_PINS[] = {
	GPIO_PIN_8,
	GPIO_PIN_9,
};
#define PIN_COUNT (sizeof(PINS) / sizeof(PINS[0]))
#define COUNTING_BITS 8

// functions
int main(void) {

	HAL_Init();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	SetupOutputGPIO();
	SetupInputGPIO();
	int currentNumber = 0;
	 while(1){

		 turnOffLeds();

		 if(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_8)){
			 currentNumber++;
		 }
		 if(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_9)){
			 currentNumber--;
		 }

		 HAL_GPIO_WritePin(GPIOC, currentNumber, 1);
		 HAL_Delay(300);

	 }
}


void SetupOutputGPIO(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                          GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

void SetupInputGPIO(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

void turnOffLeds(void) {
	for(int i = 0; i < PIN_COUNT; i++) {
		HAL_GPIO_WritePin(GPIOC, PINS[i], 0);
	}

}
