#include "gpio.h"

void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pins : PA14 PA12 PA15 PA13
                           PA11 PA0 PA9
                           PA6 PA1 PA3 PA2
                           PA7 PA4 PA5 PA8 
                           *** careful, PA13 and PA14 are debug (swd) pins, code hangs when put in analog*/
  GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_15
                          |GPIO_PIN_11|GPIO_PIN_0|GPIO_PIN_9
                          |GPIO_PIN_6|GPIO_PIN_1|GPIO_PIN_3|GPIO_PIN_2
                          |GPIO_PIN_7|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB15 PB3 PB4 PB7
                           PB9 PB14 PB8 PB13
                           PB2 PB12
                           PB0 PB11 PB10 */
  GPIO_InitStruct.Pin = GPIO_PIN_15|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_7
                          |GPIO_PIN_9|GPIO_PIN_14|GPIO_PIN_8|GPIO_PIN_13
                          |GPIO_PIN_2|GPIO_PIN_12
                          |GPIO_PIN_0|GPIO_PIN_11|GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PC13 PC2 PC3 PC5
                           PC1 PC0 PC4 PC6 */
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_5
                          |GPIO_PIN_1|GPIO_PIN_0|GPIO_PIN_4|GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);


  // Green led pin — WritePin before Init pre-loads ODR so pin drives high the instant it switches to output
  HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_SET); /* LED_GREEN OFF */
  GPIO_InitStruct.Pin = LED_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);
}

void blink_led(uint8_t times, uint16_t delay_ms)
{
    for (uint8_t i = 0; i < times; i++) {
        HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_RESET); /* LED_GREEN ON */
        HAL_Delay(delay_ms);
        HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_SET); /* LED_GREEN OFF */
        HAL_Delay(delay_ms);
    }
}
