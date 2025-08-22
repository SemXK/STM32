#include "main.h"

// prototypes

void SetupOutputGPIO(void);
void turnOffLeds(void);

//constants
const uint16_t PINS[]= {
	GPIO_PIN_0,
	GPIO_PIN_1,
	GPIO_PIN_2,
	GPIO_PIN_3,
	GPIO_PIN_4,
	GPIO_PIN_5,
	GPIO_PIN_6,
	GPIO_PIN_7,
};
#define PIN_COUNT (sizeof(PINS) / sizeof(PINS[0]))
#define COUNTING_BITS 8

// functions
int main(void) {

	HAL_Init();
	SetupGPIO();

	 while(1){

		 for(int i=0; i < 2^COUNTING_BITS; i++){
			 turnOffLeds();
			 HAL_GPIO_WritePin(GPIOC, i, 1);
			 HAL_Delay(100);
		 }
	 }
}


void SetupOutputGPIO(void) {
    // Enable GPIOC clock
    __HAL_RCC_GPIOC_CLK_ENABLE();

    // Configure just one pin for testing
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                          GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

void turnOffLeds(void) {
	for(int i = 0; i < PIN_COUNT; i++) {
		HAL_GPIO_WritePin(GPIOC, PINS[i], 0);
	}

}
