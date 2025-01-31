#include "bsp.h" // Board Support Package
#include "halt_if_debugging.h"
#include "interfaces/uart.h"
#include "main.h"
#include "pubsub_signals.h"
#include "qpc.h" // QP/C real-time embedded framework
#include "reset.h"
#include "stm32g4xx_hal.h"
#include <stdio.h>

Q_DEFINE_THIS_MODULE("bsp.c")

/**************************************************************************************************\
* Private macros
\**************************************************************************************************/

#define UART1_RX_BUFFER_LEN 128
#define UART1_TX_BUFFER_LEN 4096

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
static HAL_StatusTypeDef myHAL_DAC_Stop_DMA(DAC_HandleTypeDef *hdac, uint32_t Channel);

/**************************************************************************************************\
* Private memory declarations
\**************************************************************************************************/

const Serial_IO_T s_bsp_serial_io_uart = {
    .tx_func          = lpuart1_TransmitData,
    .rx_func          = lpuart1_ReceiveData,
    .register_cb_func = lpuart1_RegisterDataReadyCB,
};

extern UART_HandleTypeDef hlpuart1; // defined in main.c by cubeMX
extern TIM_HandleTypeDef htim1;     // defined in main.c by cubeMX
extern DAC_HandleTypeDef hdac1;     // defined in main.c by cubeMX
extern ADC_HandleTypeDef hadc1;     // defined in main.c by cubeMX
extern ADC_HandleTypeDef hadc2;     // defined in main.c by cubeMX

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
    BSP_LED_On();
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
    HAL_IncTick();
    QTIMEEVT_TICK(0U); // process time events for primary clock rate
    QK_ISR_EXIT();
}

//============================================================================
// BSP functions...

/**
 ***************************************************************************************************
 *
 * @brief   Millisecond Tick
 *
 **************************************************************************************************/
uint32_t BSP_Get_Milliseconds_Tick(void)
{
    return HAL_GetTick();
}

//............................................................................
void BSP_Init(void)
{
    UART_Config_T lpuart1_config;
    Configure_lpuart1(&lpuart1_config);
    UART_Init(&s_lpuart1, &lpuart1_config);
}
//............................................................................
void BSP_LED_On()
{
    HAL_GPIO_WritePin(FW_LED_GPIO_Port, FW_LED_Pin, 1);
}
//............................................................................
void BSP_LED_Off()
{
    HAL_GPIO_WritePin(FW_LED_GPIO_Port, FW_LED_Pin, 0);
}
//............................................................................
void BSP_debug_gpio_on()
{
    debug_gpio_state = true;
    HAL_GPIO_WritePin(DEBUG_GPIO_GPIO_Port, DEBUG_GPIO_Pin, debug_gpio_state);
}
//............................................................................
void BSP_debug_gpio_off()
{
    debug_gpio_state = false;
    HAL_GPIO_WritePin(DEBUG_GPIO_GPIO_Port, DEBUG_GPIO_Pin, debug_gpio_state);
}
//............................................................................
void BSP_debug_gpio_toggle()
{
    debug_gpio_state = !debug_gpio_state;
    HAL_GPIO_WritePin(DEBUG_GPIO_GPIO_Port, DEBUG_GPIO_Pin, debug_gpio_state);
}
//............................................................................
void BSP_terminate(int16_t result)
{
    Q_UNUSED_PAR(result);
}

/**
 ***************************************************************************************************
 * @brief   Functions for waveform generation/capture
 **************************************************************************************************/

void BSP_Set_DAC(uint16_t value)
{
    HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
    HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, value);
}

void BSP_Stop_ADC_DAC_DMA()
{
    HAL_StatusTypeDef retval;
    // Stop DMA
    retval = myHAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
    Q_ASSERT(retval == HAL_OK);

    retval = HAL_ADC_Stop_DMA(&hadc1);
    Q_ASSERT(retval == HAL_OK);

    retval = HAL_ADC_Stop_DMA(&hadc2);
    Q_ASSERT(retval == HAL_OK);
}
void BSP_Setup_ADC_DAC_DMA(
    int16_t adc1_dma_buffer[],
    int16_t adc2_dma_buffer[],
    uint16_t adc_data_width,
    uint16_t dac_dma_buffer[],
    uint16_t dac_data_width,
    uint16_t dac_total_clock_periods)
{
    HAL_StatusTypeDef retval;

    // Start ADC DMA
    retval = HAL_ADC_Start_DMA(&hadc1, (uint32_t *) adc1_dma_buffer, adc_data_width);
    Q_ASSERT(retval == HAL_OK);
    retval = HAL_ADC_Start_DMA(&hadc2, (uint32_t *) adc2_dma_buffer, adc_data_width);
    Q_ASSERT(retval == HAL_OK);

    // Start DAC DMA - it draws exactly one period at 3x the resolution as the ADC
    retval = HAL_DAC_Start_DMA(
        &hdac1, DAC_CHANNEL_1, (uint32_t *) dac_dma_buffer, dac_data_width, DAC_ALIGN_12B_R);
    Q_ASSERT(retval == HAL_OK);

    // setup CCR for TIM1 to generate exactly 10x periods
    // this is the number of DAC pulses to be generated, so it must be a multiple of 3 to keep the
    // ADC happy
    TIM1->CNT  = dac_total_clock_periods; // ensure timer doesn't start
    TIM1->CCR2 = dac_total_clock_periods; // number of pulses is equal to this number
}

void BSP_Start_Waveform_Timer()
{
    __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_CC2);
    TIM1->CNT = 0x00; // begin timer
}

/**
 ***************************************************************************************************
 * @brief   Functions for source impedance of waveform generator
 **************************************************************************************************/

void BSP_Set_Source_Impedance(Source_Impedance_T impedance)
{
    switch (impedance)
    {
        case IMPEDANCE_100: {
            PC_COM_print("Set impedance to 100 ohm");
            HAL_GPIO_WritePin(GS0_GPIO_Port, GS0_Pin, 0);
            HAL_GPIO_WritePin(GS1_GPIO_Port, GS1_Pin, 0);
            HAL_GPIO_WritePin(GS2_GPIO_Port, GS2_Pin, 0);
            break;
        }
        case IMPEDANCE_330: {
            PC_COM_print("Set impedance to 330 ohm");
            HAL_GPIO_WritePin(GS0_GPIO_Port, GS0_Pin, 1);
            HAL_GPIO_WritePin(GS1_GPIO_Port, GS1_Pin, 0);
            HAL_GPIO_WritePin(GS2_GPIO_Port, GS2_Pin, 0);
            break;
        }
        case IMPEDANCE_1k: {
            PC_COM_print("Set impedance to 1k ohm");
            HAL_GPIO_WritePin(GS0_GPIO_Port, GS0_Pin, 0);
            HAL_GPIO_WritePin(GS1_GPIO_Port, GS1_Pin, 1);
            HAL_GPIO_WritePin(GS2_GPIO_Port, GS2_Pin, 0);
            break;
        }
        case IMPEDANCE_3k3: {
            PC_COM_print("Set impedance to 3.3k ohm");
            HAL_GPIO_WritePin(GS0_GPIO_Port, GS0_Pin, 1);
            HAL_GPIO_WritePin(GS1_GPIO_Port, GS1_Pin, 1);
            HAL_GPIO_WritePin(GS2_GPIO_Port, GS2_Pin, 0);
            break;
        }
        case IMPEDANCE_10k: {
            PC_COM_print("Set impedance to 10k ohm");
            HAL_GPIO_WritePin(GS0_GPIO_Port, GS0_Pin, 0);
            HAL_GPIO_WritePin(GS1_GPIO_Port, GS1_Pin, 0);
            HAL_GPIO_WritePin(GS2_GPIO_Port, GS2_Pin, 1);
            break;
        }
        case IMPEDANCE_33k: {
            PC_COM_print("Set impedance to 33k ohm");
            HAL_GPIO_WritePin(GS0_GPIO_Port, GS0_Pin, 1);
            HAL_GPIO_WritePin(GS1_GPIO_Port, GS1_Pin, 0);
            HAL_GPIO_WritePin(GS2_GPIO_Port, GS2_Pin, 1);
            break;
        }
        case IMPEDANCE_100k: {
            PC_COM_print("Set impedance to 100k ohm");
            HAL_GPIO_WritePin(GS0_GPIO_Port, GS0_Pin, 0);
            HAL_GPIO_WritePin(GS1_GPIO_Port, GS1_Pin, 1);
            HAL_GPIO_WritePin(GS2_GPIO_Port, GS2_Pin, 1);
            break;
        }
        case IMPEDANCE_330k: {
            PC_COM_print("Set impedance to 330k ohm");
            HAL_GPIO_WritePin(GS0_GPIO_Port, GS0_Pin, 1);
            HAL_GPIO_WritePin(GS1_GPIO_Port, GS1_Pin, 1);
            HAL_GPIO_WritePin(GS2_GPIO_Port, GS2_Pin, 1);
            break;
        }
    }
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
 * @brief   clone of the HAL_DAC_Stop_DMA function, except it doesn't disable the peripheral, thus
 *the ADC output maintains its last analog value
 **************************************************************************************************/

static HAL_StatusTypeDef myHAL_DAC_Stop_DMA(DAC_HandleTypeDef *hdac, uint32_t Channel)
{
    /* Check the DAC peripheral handle */
    if (hdac == NULL)
    {
        return HAL_ERROR;
    }

    /* Check the parameters */
    assert_param(IS_DAC_CHANNEL(hdac->Instance, Channel));

    /* Disable the selected DAC channel DMA request */
    hdac->Instance->CR &= ~(DAC_CR_DMAEN1 << (Channel & 0x10UL));

    /* Disable the Peripheral */
    // __HAL_DAC_DISABLE(hdac, Channel);

    /* Disable the DMA channel */

    /* Channel1 is used */
    if (Channel == DAC_CHANNEL_1)
    {
        /* Disable the DMA channel */
        (void) HAL_DMA_Abort(hdac->DMA_Handle1);

        /* Disable the DAC DMA underrun interrupt */
        __HAL_DAC_DISABLE_IT(hdac, DAC_IT_DMAUDR1);
    }

    else /* Channel2 is used for */
    {
        /* Disable the DMA channel */
        (void) HAL_DMA_Abort(hdac->DMA_Handle2);

        /* Disable the DAC DMA underrun interrupt */
        __HAL_DAC_DISABLE_IT(hdac, DAC_IT_DMAUDR2);
    }

    /* Change DAC state */
    hdac->State = HAL_DAC_STATE_READY;

    /* Return function status */
    return HAL_OK;
}

/**
 ***************************************************************************************************
 *  @brief   Functions for LPUART
 **************************************************************************************************/

static void Configure_lpuart1(UART_Config_T *p_uart_config)
{
    p_uart_config->uart_id            = LPUART_ID_1;
    p_uart_config->baud_rate_bps      = 500000;
    p_uart_config->n_data_bits        = 8;
    p_uart_config->n_stop_bits        = 1;
    p_uart_config->parity             = UART_PARITY_VAL_NONE;
    p_uart_config->hwctrl             = UART_HWCTRL_NONE;
    p_uart_config->tx_data_buffer     = s_tx_data_buffer;
    p_uart_config->tx_data_buffer_len = UART1_TX_BUFFER_LEN;
    p_uart_config->rx_data_buffer     = s_rx_data_buffer;
    p_uart_config->rx_data_buffer_len = UART1_RX_BUFFER_LEN;
    p_uart_config->callback           = NULL;
    p_uart_config->cb_data            = NULL;
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