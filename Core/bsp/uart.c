#include "interfaces/uart.h"
#include "qpc.h"
#include "stm32g4xx.h"
#include "uart_mpu_specific.h"
#include <assert.h>

Q_DEFINE_THIS_MODULE("uart")

/**************************************************************************************************\
* Private macros
\**************************************************************************************************/
#define UART_ERROR_HW      0x01U // General UART HW error flag
#define UART_ERROR_FULL_RX 0x02U // Full receive buffer flag

/**************************************************************************************************\
* Private Data
\**************************************************************************************************/

static UART_T *s_uart_handler_table[] = {NULL, NULL, NULL, NULL, NULL, NULL};
static_assert(
    sizeof(s_uart_handler_table) / sizeof(s_uart_handler_table[0]) == UART_MAX_SUPPORTED,
    "s_uart_handler_table bad length");

/**************************************************************************************************\
* Private functions
\**************************************************************************************************/

static uint32_t STM32_UART_GetWordLength(uint8_t n_data_bits);
static uint32_t STM32_UART_GetStopBits(uint8_t n_stop_bits);
static uint32_t STM32_UART_GetParity(UART_Parity_T parity);
static uint32_t STM32_UART_GetHwCtrl(UART_HwCtrl_T hwctrl);

/**************************************************************************************************\
* Public functions
\**************************************************************************************************/

void UART_Init(UART_T *p_uart, UART_Config_T *p_config_params)
{
    Q_ASSERT(p_uart != NULL);
    Q_ASSERT(p_config_params != NULL);

    UART_HandleTypeDef *p_stm_huart = STM32_UART_GetHandle(p_config_params->uart_id);
    Q_ASSERT(p_stm_huart != NULL);

    // copy config values into UART_T structure
    p_uart->_config = *p_config_params;

    // store UART handler for later
    s_uart_handler_table[p_config_params->uart_id] = p_uart;

    // Initialize STM32 structure
    p_stm_huart->Instance        = STM32_UART_GetInstance(p_config_params->uart_id);
    p_stm_huart->Init.BaudRate   = p_config_params->baud_rate_bps;
    p_stm_huart->Init.WordLength = STM32_UART_GetWordLength(p_config_params->n_data_bits);
    p_stm_huart->Init.StopBits   = STM32_UART_GetStopBits(p_config_params->n_stop_bits);
    p_stm_huart->Init.Parity     = STM32_UART_GetParity(p_config_params->parity);
    p_stm_huart->Init.HwFlowCtl  = STM32_UART_GetHwCtrl(p_config_params->hwctrl);

    // The rest of the STM32 available parameters are rarely needed
    //   so they are hardcoded here
    p_stm_huart->Init.Mode                   = UART_MODE_TX_RX;
    p_stm_huart->Init.OverSampling           = UART_OVERSAMPLING_16;
    p_stm_huart->Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
    p_stm_huart->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    int retval = HAL_UART_Init(p_stm_huart);
    Q_ASSERT(retval == HAL_OK);

    p_uart->_error_bitmap = 0U;
    p_uart->_rx_num_bytes = 0U;
    p_uart->_tx_num_bytes = 0U;
    p_uart->_rx_read_ptr  = p_uart->_config.rx_data_buffer;
    p_uart->_rx_write_ptr = p_uart->_rx_read_ptr;
    p_uart->_tx_read_ptr  = p_uart->_config.tx_data_buffer;
    p_uart->_tx_write_ptr = p_uart->_tx_read_ptr;

    // enable UART read data register not empty interrupt
    __HAL_UART_ENABLE_IT(p_stm_huart, UART_IT_RXNE);
}

void UART_RegisterDataReadyCB(UART_T *p_uart, UART_Data_Ready_Callback cb, void *cb_data)
{
    Q_ASSERT(p_uart != NULL);
    p_uart->_config.callback = cb;
    p_uart->_config.cb_data  = cb_data;
}

UART_Return_T UART_TransmitData(UART_T *p_uart, const uint8_t *data_ptr, const uint16_t data_len)
{
    UART_Return_T retval    = UART_RTN_SUCCESS;
    uint8_t *data_bytes_ptr = (uint8_t *) data_ptr;
    uint16_t sent_chars     = 0U;

    IRQn_Type irq_type = STM32_UART_GetIrqType(p_uart->_config.uart_id);

    UART_HandleTypeDef *p_stm_huart = STM32_UART_GetHandle(p_uart->_config.uart_id);
    Q_ASSERT(p_stm_huart != NULL);

    // Can the output buffer hold the data?
    if (p_uart->_tx_num_bytes + data_len <= p_uart->_config.tx_data_buffer_len)
    {
        // While there are characters to send and the output buffer is not full
        while (sent_chars < data_len)
        {
            HAL_NVIC_DisableIRQ(irq_type);
            p_uart->_tx_num_bytes++;
            *(p_uart->_tx_write_ptr++) = *data_bytes_ptr++;

            // Is the pointer at the end of the transmit buffer?
            if (p_uart->_tx_write_ptr >=
                (p_uart->_config.tx_data_buffer + p_uart->_config.tx_data_buffer_len))
            {
                p_uart->_tx_write_ptr = p_uart->_config.tx_data_buffer;
            }

            sent_chars++;

            // Is the Transmit Data Register Empty interrupt disabled?
            if ((p_stm_huart->Instance->CR1 & USART_CR1_TXEIE) == 0U)
            {
                // Enable the Transmit Data Register Empty interrupt
                ATOMIC_SET_BIT(p_stm_huart->Instance->CR1, USART_CR1_TXEIE);
            }

            HAL_NVIC_EnableIRQ(irq_type);
        }
    }
    else
    {
        retval = UART_RTN_TX_BUF_FULL;
    }

    return retval;
}

UART_Return_T UART_ReceiveByte(UART_T *p_uart, uint8_t *byte_ptr)
{
    UART_Return_T retval = UART_RTN_SUCCESS;

    IRQn_Type irq_type = STM32_UART_GetIrqType(p_uart->_config.uart_id);

    // Are there bytes in the receive buffer?
    if (p_uart->_rx_num_bytes > 0)
    {
        HAL_NVIC_DisableIRQ(irq_type);
        p_uart->_rx_num_bytes--;
        *byte_ptr = *(p_uart->_rx_read_ptr++);

        // Is the pointer outside the receive buffer?
        if (p_uart->_rx_read_ptr >=
            (p_uart->_config.rx_data_buffer + p_uart->_config.rx_data_buffer_len))
        {
            p_uart->_rx_read_ptr = p_uart->_config.rx_data_buffer;
        }
        HAL_NVIC_EnableIRQ(irq_type);
    }
    else
    {
        retval = UART_RTN_NO_RX_DATA;
    }

    return retval;
}

// Globally available because it must be called by actual IRQs defined in mpu-specific code
void USART_IRQHandler(UART_ID_T id)
{
    UART_T *p_uart = s_uart_handler_table[id];
    if (p_uart == NULL)
    {
        return;
    }

    UART_HandleTypeDef *p_stm_huart = STM32_UART_GetHandle(p_uart->_config.uart_id);
    Q_ASSERT(p_stm_huart != NULL);

    uint32_t isr_reg = p_stm_huart->Instance->ISR;
    uint32_t cr1_reg = p_stm_huart->Instance->CR1;
    uint32_t cr3_reg = p_stm_huart->Instance->CR3;
    uint32_t errorflags;
    uint8_t uart_byte;

    errorflags =
        (isr_reg &
         (uint32_t) (USART_ISR_PE | USART_ISR_FE | USART_ISR_ORE | USART_ISR_NE | USART_ISR_RTOF));

    // If no error occured
    if (errorflags == 0U)
    {
        // has data been received (RXNE) and is receive interrupt (RXNEIE) enabled?
        if ((isr_reg & USART_ISR_RXNE) && (cr1_reg & USART_CR1_RXNEIE))
        {
            // read received byte from RDR register
            // RXNE flag is be cleared by reading register
            uart_byte = (uint8_t) p_stm_huart->Instance->RDR;

            // Is the number of bytes in the receive buffer lower than the size of the buffer?
            if (p_uart->_rx_num_bytes < p_uart->_config.rx_data_buffer_len)
            {
                // Increase number of bytes in the receive buffer
                p_uart->_rx_num_bytes++;

                // Save received char to the receive buffer
                *(p_uart->_rx_write_ptr++) = uart_byte;

                // Is the pointer out of the receive buffer?
                if (p_uart->_rx_write_ptr >=
                    (p_uart->_config.rx_data_buffer + p_uart->_config.rx_data_buffer_len))
                {
                    // Set pointer to the first item of the receive buffer
                    p_uart->_rx_write_ptr = p_uart->_config.rx_data_buffer;
                }
            }
            else
            {
                // set RX buffer overflow flag
                p_uart->_error_bitmap |= UART_ERROR_FULL_RX;
            }

            // Call the user-supplied callback function if data is available
            if (p_uart->_rx_num_bytes > 0 && p_uart->_config.callback != NULL)
            {
                p_uart->_config.callback(p_uart->_config.cb_data);
            }
        }

        // is the transmit data register (TXE) empty flag set and the
        // transmit data register empty interrupt (TXEIE) enabled?
        if ((isr_reg & USART_ISR_TXE) && (cr1_reg & USART_CR1_TXEIE))
        {
            // Are there bytes remaining in the transmit buffer?
            if (p_uart->_tx_num_bytes > 0)
            {
                // Decrease number of bytes in the transmit buffer
                p_uart->_tx_num_bytes--;

                // Send next byte to transmit register
                p_stm_huart->Instance->TDR = *(p_uart->_tx_read_ptr++);

                // Is the pointer out of the transmit buffer?
                if (p_uart->_tx_read_ptr >=
                    (p_uart->_config.tx_data_buffer + p_uart->_config.tx_data_buffer_len))
                {
                    // Set pointer to the first item of the transmit buffer
                    p_uart->_tx_read_ptr = p_uart->_config.tx_data_buffer;
                }
            }
            else
            {
                // Disable the UART transmit data register empty interrupt
                ATOMIC_CLEAR_BIT(p_stm_huart->Instance->CR1, USART_CR1_TXEIE);
            }
        }
    }
    else
    {
        // set general HW error occured flag
        p_uart->_error_bitmap |= UART_ERROR_HW;

        // UART parity error interrupt occurred
        if ((isr_reg & USART_ISR_PE) && (cr1_reg & USART_CR1_PEIE))
        {
            __HAL_UART_CLEAR_FLAG(p_stm_huart, UART_CLEAR_PEF);
            // SEGGER_RTT_WriteString(0, "UART 2 - Parity Error\r\n");
        }

        // UART frame error interrupt occurred
        if ((isr_reg & USART_ISR_FE)) // && (cr3_reg & USART_CR3_EIE))
        {
            __HAL_UART_CLEAR_FLAG(p_stm_huart, UART_CLEAR_FEF);
            // SEGGER_RTT_WriteString(0, "UART 2 - Frame Error\r\n");
        }

        // UART Over-Run interrupt occurred
        if ((isr_reg & USART_ISR_ORE) &&
            ((cr1_reg & USART_CR1_RXNEIE) || (cr3_reg & USART_CR3_EIE)))
        {
            __HAL_UART_CLEAR_FLAG(p_stm_huart, UART_CLEAR_OREF);
            // SEGGER_RTT_WriteString(0, "UART 2 - Over-Run Error\r\n");
        }

        // UART noise error interrupt occurred
        if ((isr_reg & USART_ISR_NE) && (cr3_reg & USART_CR3_EIE))
        {
            __HAL_UART_CLEAR_FLAG(p_stm_huart, UART_CLEAR_NEF);
            // SEGGER_RTT_WriteString(0, "UART 2 - Noise Error\r\n");
        }

        // UART Receiver Timeout interrupt occurred
        if ((isr_reg & USART_ISR_RTOF) && (cr1_reg & USART_CR1_RTOIE))
        {
            __HAL_UART_CLEAR_FLAG(p_stm_huart, UART_CLEAR_RTOF);
            // SEGGER_RTT_WriteString(0, "UART 2 - Receiver Timeout Error\r\n");
        }

        // has data been received (RXNE) and is receive interrupt (RXNEIE) enabled?
        if ((isr_reg & USART_ISR_RXNE) && (cr1_reg & USART_CR1_RXNEIE))
        {
            // read received byte from RDR register
            // RXNE flag is be cleared by reading register
            // the received byte is thrown away
            uart_byte = (uint8_t) p_stm_huart->Instance->RDR;
        }
    }
}

static uint32_t STM32_UART_GetWordLength(uint8_t n_data_bits)
{
    switch (n_data_bits)
    {
        case 7:
            return UART_WORDLENGTH_7B;
        case 8:
            return UART_WORDLENGTH_8B;
        case 9:
            return UART_WORDLENGTH_9B;
        default:
            Q_ASSERT(false);
    }
}

static uint32_t STM32_UART_GetStopBits(uint8_t n_stop_bits)
{
    switch (n_stop_bits)
    {
        case 1:
            return UART_STOPBITS_1;
        case 2:
            return UART_STOPBITS_2;
        default:
            Q_ASSERT(false);
    }
}

static uint32_t STM32_UART_GetParity(UART_Parity_T parity)
{
    switch (parity)
    {
        case UART_PARITY_VAL_NONE:
            return UART_PARITY_NONE;
        case UART_PARITY_VAL_EVEN:
            return UART_PARITY_EVEN;
        case UART_PARITY_VAL_ODD:
            return UART_PARITY_ODD;
        default:
            Q_ASSERT(false);
    }
}

static uint32_t STM32_UART_GetHwCtrl(UART_HwCtrl_T hwctrl)
{
    switch (hwctrl)
    {
        case UART_HWCTRL_NONE:
            return UART_HWCONTROL_NONE;
        case UART_HWCTRL_RTS:
            return UART_HWCONTROL_RTS;
        case UART_HWCTRL_CTS:
            return UART_HWCONTROL_CTS;
        case UART_HWCTRL_RTS_CTS:
            return UART_HWCONTROL_RTS_CTS;
        default:
            Q_ASSERT(false);
    }
}
