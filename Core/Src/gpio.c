#include "gpio.h"
#include "stm32_timer.h"

/* LED_G is active-low on the MissMetal board (lit when PB1 is driven LOW) -
 * the opposite of TA's board. */
#define LED_ON_STATE    GPIO_PIN_RESET
#define LED_OFF_STATE   GPIO_PIN_SET

void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /*Configure GPIO pins : PA1 PA2 PA3
                            PA4 PA5 PA6 PA7
                            PA8 PA9 PA11 PA12
                            PA15
                            *** careful, PA13 and PA14 are debug (swd) pins, code hangs when put in analog*/
    GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3
                            |GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7
                            |GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_11|GPIO_PIN_12
                            |GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*Configure GPIO pins : PB0 PB2 PB3 PB4 PB5 PB7
                            PB8 PB9 PB11
                            PB12 PB13 PB14 PB15 */
    GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_7
                            |GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_11
                            |GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* Test-board detect - deliberately grounded on the test board. The
     * internal pull-up keeps the pin HIGH (no UART swap) on boards without
     * TA's external 100kOhm pull-up, where it would otherwise float and could
     * read LOW, swapping TX/RX and silencing the log. */
    GPIO_InitStruct.Pin = TB_DETECT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(TB_DETECT_GPIO_PORT, &GPIO_InitStruct);

    /* BMA400 INT1 (motion/no-motion) - PA0, EXTI0, rising edge */
    GPIO_InitStruct.Pin = BMA400_INT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(BMA400_INT_GPIO_PORT, &GPIO_InitStruct);
    HAL_NVIC_SetPriority(BMA400_INT_EXTI_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(BMA400_INT_EXTI_IRQn);

    /*Configure GPIO pins : PC0 PC1 PC2 PC3
                            PC4 PC5 PC6 PC13 (BMA400 INT2, unused) */
    GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3
                            |GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);


    // Pre-load ODR to "off" before switching to output so the LED never glitches on.
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, LED_OFF_STATE);
    GPIO_InitStruct.Pin = LED_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);
}

void gpio_blink_led(uint8_t times, uint16_t delay_ms)
{
    for (uint8_t i = 0; i < times; i++) {
        HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, LED_ON_STATE);
        HAL_Delay(delay_ms);
        HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, LED_OFF_STATE);
        HAL_Delay(delay_ms);
    }
}

void gpio_release_led(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = LED_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);
}

bool gpio_is_test_board(void)
{
    /* Read once and cache: gpio_release_test_board_detect() puts the pin in
     * analog mode, where it reads 0 - a later UART re-init would otherwise
     * see "test board" and swap TX/RX. */
    static int8_t s_is_test_board = -1;
    if (s_is_test_board < 0) {
        s_is_test_board = (HAL_GPIO_ReadPin(TB_DETECT_GPIO_PORT, TB_DETECT_PIN) == GPIO_PIN_RESET) ? 1 : 0;
    }
    return s_is_test_board == 1;
}

void gpio_release_test_board_detect(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = TB_DETECT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(TB_DETECT_GPIO_PORT, &GPIO_InitStruct);
}

/* Pulses per burst. Written from thread context (SyncModeConfiguration), read
 * in the timer callbacks (RTC IRQ); a single byte, so no tearing. */
static volatile uint8_t s_alive_blink_count = 1U;

#if GPIO_ALIVE_BLINK_PERIOD_MS > 0
/* The periodic timer starts a burst; the one-shot step timer then alternates
 * LED on (GPIO_ALIVE_BLINK_ON_MS) / off (GPIO_ALIVE_BLINK_GAP_MS) until the
 * burst's pulses are used up. Callbacks run from the RTC alarm IRQ and the
 * GPIO output holds its level in STOP2, so the MCU sleeps between edges. */
static UTIL_TIMER_Object_t s_alive_blink_timer;
static UTIL_TIMER_Object_t s_alive_blink_step_timer;
static uint8_t s_alive_blink_remaining;
static bool s_alive_blink_led_on;

static void AliveBlinkLedOn(void)
{
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, LED_ON_STATE);
    s_alive_blink_led_on = true;
    UTIL_TIMER_SetPeriod(&s_alive_blink_step_timer, GPIO_ALIVE_BLINK_ON_MS);
    UTIL_TIMER_Start(&s_alive_blink_step_timer);
}

static void OnAliveBlinkStepEvent(void *context)
{
    (void)context;
    if (s_alive_blink_led_on) {
        HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, LED_OFF_STATE);
        s_alive_blink_led_on = false;
        if (s_alive_blink_remaining > 0U) {
            s_alive_blink_remaining--;
        }
        if (s_alive_blink_remaining > 0U) {
            UTIL_TIMER_SetPeriod(&s_alive_blink_step_timer, GPIO_ALIVE_BLINK_GAP_MS);
            UTIL_TIMER_Start(&s_alive_blink_step_timer);
        }
    } else {
        AliveBlinkLedOn();
    }
}

static void OnAliveBlinkEvent(void *context)
{
    (void)context;
    if (s_alive_blink_remaining > 0U) {
        return;     /* previous burst still running (period shorter than a burst) */
    }
    s_alive_blink_remaining = s_alive_blink_count;
    if (s_alive_blink_remaining > 0U) {
        AliveBlinkLedOn();
    }
}
#endif

void gpio_alive_blink_start(void)
{
#if GPIO_ALIVE_BLINK_PERIOD_MS > 0
    UTIL_TIMER_Create(&s_alive_blink_step_timer, GPIO_ALIVE_BLINK_ON_MS, UTIL_TIMER_ONESHOT, OnAliveBlinkStepEvent, NULL);
    UTIL_TIMER_Create(&s_alive_blink_timer, GPIO_ALIVE_BLINK_PERIOD_MS, UTIL_TIMER_PERIODIC, OnAliveBlinkEvent, NULL);
    UTIL_TIMER_Start(&s_alive_blink_timer);
#endif
}

void gpio_alive_blink_set_count(uint8_t count)
{
    s_alive_blink_count = count;
}
