#include "main.h"
#include "math.h"
#include <stdbool.h>
#include "common.h"

/**
 * Set a pin from port C to output push-pull mode
 */
void enablePin(GPIO_TypeDef  *GPIOx, uint16_t pin) {

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);

}








