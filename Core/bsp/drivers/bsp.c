#include "qpc.h" // QP/C real-time embedded framework
#include "bsp.h" // Board Support Package
#include "pubsub_signals.h"
#include "main.h"
#include "stm32g4xx_hal.h"
#include "uart.h"
#include "reset.h"
#include "halt_if_debugging.h"
#include <stdio.h>

/**************************************************************************************************\
* Private macros
\**************************************************************************************************/

#define UART1_RX_BUFFER_LEN 128
#define UART1_TX_BUFFER_LEN 1024

/**************************************************************************************************\
* Private type definitions
\**************************************************************************************************/

/**************************************************************************************************\
* Private prototypes
\**************************************************************************************************/

static uint16_t lpuart1_TransmitData(const uint8_t *data_ptr, const uint16_t data_len);
static uint16_t lpuart1_ReceiveData(uint8_t *data_ptr, const uint16_t max_data_len);
static void lpuart1_RegisterDataReadyCB(Serial_IO_Data_Ready_Callback cb, void *cb_data);
static void Configure_lpuart1(UART_Config_T *p_uart_config);

/**************************************************************************************************\
* Private memory declarations
\**************************************************************************************************/

const Serial_IO_T s_bsp_serial_io_uart = {
    .tx_func = lpuart1_TransmitData,
    .rx_func = lpuart1_ReceiveData,
    .register_cb_func = lpuart1_RegisterDataReadyCB,
};

// extern ADC_HandleTypeDef hadc2;  // defined in main.c by cubeMX
// extern TIM_HandleTypeDef htim15; // defined in main.c by cubeMX
extern UART_HandleTypeDef hlpuart1; // defined in main.c by cubeMX

static UART_T s_lpuart1;
static uint8_t s_rx_data_buffer[UART1_RX_BUFFER_LEN] = {0};
static uint8_t s_tx_data_buffer[UART1_TX_BUFFER_LEN] = {0};

static bool debug_gpio_state = false;

/**************************************************************************************************\
* Public functions
\**************************************************************************************************/

/**
 ***************************************************************************************************
 * @brief   QP Assert handler
 **************************************************************************************************/
Q_NORETURN Q_onError(char const *const module, int_t const loc)
{
    // NOTE: this implementation of the error handler is intended only
    // for debugging and MUST be changed for deployment of the application
    // (assuming that you ship your production code with assertions enabled).
    Q_UNUSED_PAR(module);
    Q_UNUSED_PAR(loc);
    QS_ASSERTION(module, loc, 10000U);

#ifndef NDEBUG
    BSP_ledOn();
#endif

    const uint32_t NOT_USED = 0;
    Reset_DoResetWithReasonWithStr(RESET_REASON_Q_ASSERT, module, loc, NOT_USED);
}

/**
 ***************************************************************************************************
 * @brief   Perform a reset of the microcontroller
 **************************************************************************************************/
__attribute__((noreturn)) void BSP_SystemReset(void)
{
    HALT_IF_DEBUGGING();
    NVIC_SystemReset();
}

/**
 ***************************************************************************************************
 * @brief   Read the microcontroller's RCC CSR Register.
 **************************************************************************************************/
uint32_t BSP_RCC_CSR_Read(void)
{
    return RCC->CSR;
}

/**
 ***************************************************************************************************
 * @brief   Clear reset flags indicated by the RCC CSR Register.
 **************************************************************************************************/
void BSP_RCC_CSR_ClearResetFlags(void)
{
    // From Ref Manual:
    //   RMVF: Remove reset flag -- This bit is set by software to clear the reset flags.
    RCC->CSR |= RCC_CSR_RMVF;
}

//............................................................................
void assert_failed(char const *const module, int_t const id); // prototype
void assert_failed(char const *const module, int_t const id)
{
    Q_onError(module, id);
}

//............................................................................
void SysTick_Handler(void); // prototype
void SysTick_Handler(void)
{
    QK_ISR_ENTRY();
    QTIMEEVT_TICK_X(0U, &l_SysTick_Handler); // time events at rate 0
    QK_ARM_ERRATUM_838869();
    QK_ISR_EXIT();
}

//============================================================================
// BSP functions...

//............................................................................
void BSP_Init(void)
{
    UART_Config_T lpuart1_config;
    Configure_lpuart1(&lpuart1_config);
    UART_Init(&s_lpuart1, &lpuart1_config);
}
//............................................................................
void BSP_ledOn()
{
    HAL_GPIO_WritePin(FW_LED_GPIO_Port, FW_LED_Pin, 1);
}
//............................................................................
void BSP_ledOff()
{
    HAL_GPIO_WritePin(FW_LED_GPIO_Port, FW_LED_Pin, 0);
}
//............................................................................
void BSP_terminate(int16_t result)
{
    Q_UNUSED_PAR(result);
}

//============================================================================
// QF callbacks...
void QF_onStartup(void)
{
    // set up the SysTick timer to fire at BSP_TICKS_PER_SEC rate
    SysTick_Config(SystemCoreClock / BSP_TICKS_PER_SEC);

    // assign all priority bits for preemption-prio. and none to sub-prio.
    NVIC_SetPriorityGrouping(0U);

    // set priorities of ALL ISRs used in the system, see NOTE1
    // NVIC_SetPriority(EXTI0_IRQn, QF_AWARE_ISR_CMSIS_PRI + 0U);
    // NVIC_SetPriority(TIM4_IRQn, QF_AWARE_ISR_CMSIS_PRI + 0U);
    NVIC_SetPriority(TIM1_BRK_TIM15_IRQn, QF_AWARE_ISR_CMSIS_PRI + 0U); // tach input capture
    NVIC_SetPriority(I2C2_EV_IRQn, QF_AWARE_ISR_CMSIS_PRI + 1U);        // I2C for pressure and OLED
    NVIC_SetPriority(I2C2_ER_IRQn, QF_AWARE_ISR_CMSIS_PRI + 1U);        // I2C for pressure and OLED
    NVIC_SetPriority(USART2_IRQn, QF_AWARE_ISR_CMSIS_PRI + 2U);
    NVIC_SetPriority(SysTick_IRQn, QF_AWARE_ISR_CMSIS_PRI + 12U);
    // ...

    // enable IRQs...
    // NVIC_EnableIRQ(EXTI0_IRQn);
    // HAL_NVIC_EnableIRQ(TIM1_BRK_TIM15_IRQn); // enaled by AO

#ifdef Q_SPY
    NVIC_EnableIRQ(USART2_IRQn); // UART2 interrupt used for QS-RX
#endif
}
//............................................................................
void QF_onCleanup(void)
{
}
//............................................................................
void QK_onIdle(void)
{ // called with interrupts DISABLED, see NOTE01
}


/*****************************************************************************
 * NOTE1:
 * The QF_AWARE_ISR_CMSIS_PRI constant from the QF port specifies the highest
 * ISR priority that is disabled by the QF framework. The value is suitable
 * for the NVIC_SetPriority() CMSIS function.
 *
 * Only ISRs prioritized at or below the QF_AWARE_ISR_CMSIS_PRI level (i.e.,
 * with the numerical values of priorities equal or higher than
 * QF_AWARE_ISR_CMSIS_PRI) are allowed to call the QK_ISR_ENTRY/QK_ISR_ENTRY
 * macros or any other QF services. These ISRs are "QF-aware".
 *
 * Conversely, any ISRs prioritized above the QF_AWARE_ISR_CMSIS_PRI priority
 * level (i.e., with the numerical values of priorities less than
 * QF_AWARE_ISR_CMSIS_PRI) are never disabled and are not aware of the kernel.
 * Such "QF-unaware" ISRs cannot call any QF services. In particular they
 * can NOT call the macros QK_ISR_ENTRY/QK_ISR_ENTRY. The only mechanism
 * by which a "QF-unaware" ISR can communicate with the QF framework is by
 * triggering a "QF-aware" ISR, which can post/publish events.
 *
 */

/**
 ***************************************************************************************************
 * @brief   Functions for CLI via UART
 **************************************************************************************************/

const Serial_IO_T *BSP_Get_Serial_IO_Interface_UART()
{
    return &s_bsp_serial_io_uart;
}

/**************************************************************************************************\
* Private functions
\**************************************************************************************************/

/**
 ***************************************************************************************************
 *  @brief   Functions for LPUART
 **************************************************************************************************/

static void Configure_lpuart1(UART_Config_T *p_uart_config)
{
    p_uart_config->uart_id = LPUART_ID_1;
    p_uart_config->baud_rate_bps = 115200;
    p_uart_config->n_data_bits = 8;
    p_uart_config->n_stop_bits = 1;
    p_uart_config->parity = UART_PARITY_VAL_NONE;
    p_uart_config->hwctrl = UART_HWCTRL_NONE;
    p_uart_config->tx_data_buffer = s_tx_data_buffer;
    p_uart_config->tx_data_buffer_len = UART1_TX_BUFFER_LEN;
    p_uart_config->rx_data_buffer = s_rx_data_buffer;
    p_uart_config->rx_data_buffer_len = UART1_RX_BUFFER_LEN;
    p_uart_config->callback = NULL;
    p_uart_config->cb_data = NULL;
}

static uint16_t lpuart1_TransmitData(const uint8_t *data_ptr, const uint16_t data_len)
{
    UART_Return_T retval = UART_TransmitData(&s_lpuart1, data_ptr, data_len);
    return (retval == UART_RTN_SUCCESS) ? data_len : 0;
}

static uint16_t lpuart1_ReceiveData(uint8_t *data_ptr, const uint16_t max_data_len)
{
    if (max_data_len == 0)
    {
        return 0;
    }

    UART_Return_T retval = UART_ReceiveByte(&s_lpuart1, data_ptr);
    return (retval == UART_RTN_SUCCESS) ? 1 : 0;
}

static void lpuart1_RegisterDataReadyCB(Serial_IO_Data_Ready_Callback cb, void *cb_data)
{
    UART_RegisterDataReadyCB(&s_lpuart1, cb, cb_data);
}