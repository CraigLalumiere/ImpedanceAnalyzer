/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    stm32g4xx_it.c
 * @brief   Interrupt Service Routines.
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
#include "stm32g4xx_it.h"
#include "main.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "interfaces/uart.h"
#include "qpc.h"
#include "reset.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern DMA_HandleTypeDef hdma_adc1;
extern ADC_HandleTypeDef hadc1;
extern DMA_HandleTypeDef hdma_dac1_ch1;
extern UART_HandleTypeDef hlpuart1;
extern DMA_HandleTypeDef hdma_tim1_up;
extern TIM_HandleTypeDef htim1;
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M4 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
 * @brief This function handles Hard fault interrupt.
 */
void HardFault_Handler(void)
{
    /* USER CODE BEGIN HardFault_IRQn 0 */
    // https://interrupt.memfault.com/blog/cortex-m-hardfault-debug#relevant-status-registers
    uint32_t hfsr  = SCB->HFSR;
    uint32_t cfsr  = SCB->CFSR;
    uint32_t abfsr = 0U; // not used
    uint32_t mmfar = SCB->MMFAR;
    Reset_DoResetWithReason(RESET_REASON_HARD_FAULT, hfsr, cfsr, abfsr, mmfar);
    /* USER CODE END HardFault_IRQn 0 */
    while (1)
    {
        /* USER CODE BEGIN W1_HardFault_IRQn 0 */
        /* USER CODE END W1_HardFault_IRQn 0 */
    }
}

/**
 * @brief This function handles Memory management fault.
 */
void MemManage_Handler(void)
{
    /* USER CODE BEGIN MemoryManagement_IRQn 0 */
    // https://interrupt.memfault.com/blog/cortex-m-hardfault-debug#relevant-status-registers
    uint32_t hfsr  = SCB->HFSR;
    uint32_t cfsr  = SCB->CFSR;
    uint32_t abfsr = 0U; // not used
    uint32_t mmfar = SCB->MMFAR;
    Reset_DoResetWithReason(RESET_REASON_MEM_MANAGE_FAULT, hfsr, cfsr, abfsr, mmfar);
    /* USER CODE END MemoryManagement_IRQn 0 */
    while (1)
    {
        /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
        /* USER CODE END W1_MemoryManagement_IRQn 0 */
    }
}

/**
 * @brief This function handles Prefetch fault, memory access fault.
 */
void BusFault_Handler(void)
{
    /* USER CODE BEGIN BusFault_IRQn 0 */
    // https://interrupt.memfault.com/blog/cortex-m-hardfault-debug
    uint32_t hfsr  = SCB->HFSR;
    uint32_t cfsr  = SCB->CFSR;
    uint32_t abfsr = 0U; // not used
    uint32_t mmfar = SCB->MMFAR;
    Reset_DoResetWithReason(RESET_REASON_BUS_FAULT, hfsr, cfsr, abfsr, mmfar);
    /* USER CODE END BusFault_IRQn 0 */
    while (1)
    {
        /* USER CODE BEGIN W1_BusFault_IRQn 0 */
        /* USER CODE END W1_BusFault_IRQn 0 */
    }
}

/**
 * @brief This function handles Undefined instruction or illegal state.
 */
void UsageFault_Handler(void)
{
    /* USER CODE BEGIN UsageFault_IRQn 0 */
    // https://interrupt.memfault.com/blog/cortex-m-hardfault-debug
    uint32_t hfsr  = SCB->HFSR;
    uint32_t cfsr  = SCB->CFSR;
    uint32_t abfsr = 0U; // not used
    uint32_t mmfar = SCB->MMFAR;
    Reset_DoResetWithReason(RESET_REASON_USAGE_FAULT, hfsr, cfsr, abfsr, mmfar);
    /* USER CODE END UsageFault_IRQn 0 */
    while (1)
    {
        /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
        /* USER CODE END W1_UsageFault_IRQn 0 */
    }
}

/**
 * @brief This function handles System service call via SWI instruction.
 */
void SVC_Handler(void)
{
    /* USER CODE BEGIN SVCall_IRQn 0 */

    /* USER CODE END SVCall_IRQn 0 */
    /* USER CODE BEGIN SVCall_IRQn 1 */

    /* USER CODE END SVCall_IRQn 1 */
}

/**
 * @brief This function handles Debug monitor.
 */
void DebugMon_Handler(void)
{
    /* USER CODE BEGIN DebugMonitor_IRQn 0 */

    /* USER CODE END DebugMonitor_IRQn 0 */
    /* USER CODE BEGIN DebugMonitor_IRQn 1 */

    /* USER CODE END DebugMonitor_IRQn 1 */
}

/******************************************************************************/
/* STM32G4xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32g4xx.s).                    */
/******************************************************************************/

/**
 * @brief This function handles DMA1 channel1 global interrupt.
 */
void DMA1_Channel1_IRQHandler(void)
{
    /* USER CODE BEGIN DMA1_Channel1_IRQn 0 */

    /* USER CODE END DMA1_Channel1_IRQn 0 */
    HAL_DMA_IRQHandler(&hdma_dac1_ch1);
    /* USER CODE BEGIN DMA1_Channel1_IRQn 1 */
    QK_ARM_ERRATUM_838869();
    /* USER CODE END DMA1_Channel1_IRQn 1 */
}

/**
 * @brief This function handles DMA1 channel2 global interrupt.
 */
void DMA1_Channel2_IRQHandler(void)
{
    /* USER CODE BEGIN DMA1_Channel2_IRQn 0 */

    /* USER CODE END DMA1_Channel2_IRQn 0 */
    HAL_DMA_IRQHandler(&hdma_adc1);
    /* USER CODE BEGIN DMA1_Channel2_IRQn 1 */
    QK_ARM_ERRATUM_838869();
    /* USER CODE END DMA1_Channel2_IRQn 1 */
}

/**
 * @brief This function handles DMA1 channel3 global interrupt.
 */
void DMA1_Channel3_IRQHandler(void)
{
    /* USER CODE BEGIN DMA1_Channel3_IRQn 0 */

    /* USER CODE END DMA1_Channel3_IRQn 0 */
    HAL_DMA_IRQHandler(&hdma_tim1_up);
    /* USER CODE BEGIN DMA1_Channel3_IRQn 1 */
    QK_ARM_ERRATUM_838869();
    /* USER CODE END DMA1_Channel3_IRQn 1 */
}

/**
 * @brief This function handles ADC1 and ADC2 global interrupt.
 */
void ADC1_2_IRQHandler(void)
{
    /* USER CODE BEGIN ADC1_2_IRQn 0 */

    /* USER CODE END ADC1_2_IRQn 0 */
    HAL_ADC_IRQHandler(&hadc1);
    /* USER CODE BEGIN ADC1_2_IRQn 1 */
    QK_ARM_ERRATUM_838869();
    /* USER CODE END ADC1_2_IRQn 1 */
}

/**
 * @brief This function handles TIM1 capture compare interrupt.
 */
void TIM1_CC_IRQHandler(void)
{
    /* USER CODE BEGIN TIM1_CC_IRQn 0 */
    QK_ISR_ENTRY();
    /* USER CODE END TIM1_CC_IRQn 0 */
    HAL_TIM_IRQHandler(&htim1);
    /* USER CODE BEGIN TIM1_CC_IRQn 1 */
    QK_ISR_EXIT();
    // QK_ARM_ERRATUM_838869();
    /* USER CODE END TIM1_CC_IRQn 1 */
}

/**
 * @brief This function handles LPUART1 global interrupt.
 */
void LPUART1_IRQHandler(void)
{
    /* USER CODE BEGIN LPUART1_IRQn 0 */

    QK_ISR_ENTRY();
    USART_IRQHandler(LPUART_ID_1);
    QK_ISR_EXIT();
    return;
    /* USER CODE END LPUART1_IRQn 0 */
    HAL_UART_IRQHandler(&hlpuart1);
    /* USER CODE BEGIN LPUART1_IRQn 1 */

    /* USER CODE END LPUART1_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
