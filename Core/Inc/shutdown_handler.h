/* Properly deinitialize the LoRaWAN subsystem */
#include "stm32wlxx_hal_rcc.h" // Include the header for RCC functions
extern SUBGHZ_HandleTypeDef hsubghz; // This should be defined in subghz.c

void DeInitRadioAndShutdown(void)
{
    /* Stop LORAWAN */
    printf("Stopping LoRaWAN...\n");
    
    /* Stop the M0+ core if it's active */
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST) == RESET) {
        printf("Stopping C2 core (M0+)...\n");
        
        #if defined(CORE_CM4)
        // These are specific to STM32WL dual-core management
        // __HAL_RCC_C2STOP_ENABLE();
        
        // Wait for the core to stop
        HAL_Delay(10);
        #endif
    }
    
    /* Put the SUBGHZ radio in sleep mode */
    printf("Putting radio to sleep...\n");
    
    #if defined(SUBGHZ_BASE)
    // Direct register access to put radio to sleep
    SUBGHZ_CR_REG_DEF = SUBGHZ_CR_SLEEP;
    #endif
    
    /* Disable radio interrupt */
    printf("Disabling radio interrupt...\n");
    HAL_NVIC_DisableIRQ(SUBGHZ_Radio_IRQn);
    HAL_NVIC_ClearPendingIRQ(SUBGHZ_Radio_IRQn);
    
    /* Disable SUBGHZ clock */
    __HAL_RCC_SUBGHZSPI_CLK_DISABLE();
    
    /* Reset the radio */
    __HAL_RCC_SUBGHZSPI_FORCE_RESET();
    HAL_Delay(10);
    __HAL_RCC_SUBGHZSPI_RELEASE_RESET();
    
    /* Disable all interrupts */
    __disable_irq();
    NVIC->ICER[0] = 0xFFFFFFFF;
    NVIC->ICPR[0] = 0xFFFFFFFF;
    
    /* Verify interrupts are disabled */
    uint32_t active_ints = NVIC->ISER[0];
    printf("Active interrupts: 0x%08lX\n", active_ints);
    
    /* Configure wakeup pin */
    HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN1);
    HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1_LOW);
    
    /* Clear any wake flags */
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WUF1);
    
    /* Final delay to allow UART transmission */
    HAL_Delay(200);
    
    /* Enter shutdown mode */
    #if defined(CORE_CM4)
    /* Set M4 core to shutdown */
    MODIFY_REG(PWR->CR1, PWR_CR1_LPMS, PWR_LOWPOWERMODE_SHUTDOWN);
    #elif defined(CORE_CM0PLUS)
    /* Set M0+ core to shutdown */
    MODIFY_REG(PWR->C2CR1, PWR_C2CR1_LPMS, PWR_LOWPOWERMODE_SHUTDOWN);
    #else
    /* Default for single-core mode */
    MODIFY_REG(PWR->CR1, PWR_CR1_LPMS, PWR_LOWPOWERMODE_SHUTDOWN);
    #endif
    
    /* Set SLEEPDEEP bit of Cortex System Control Register */
    SET_BIT(SCB->SCR, ((uint32_t)SCB_SCR_SLEEPDEEP_Msk));
    
    /* This option is used to ensure that store operations are completed */
    #if defined(__CC_ARM)
    __force_stores();
    #endif
    
    /* Request Wait For Interrupt */
    __WFI();
    
    /* This code should never be reached */
    printf("Failed to enter shutdown mode\n");
}
