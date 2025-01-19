#ifndef UART_H_
#define UART_H_

#include "stdbool.h"
#include "stdint.h"

typedef enum
{
    LPUART_ID_1,
    UART_ID_1,
    UART_ID_2,
    UART_ID_3,
    UART_ID_4,
    UART_ID_5,
    UART_MAX_SUPPORTED,
} UART_ID_T;

typedef enum
{
    UART_PARITY_VAL_NONE,
    UART_PARITY_VAL_EVEN,
    UART_PARITY_VAL_ODD,
} UART_Parity_T;

typedef enum
{
    UART_HWCTRL_NONE,
    UART_HWCTRL_RTS,
    UART_HWCTRL_CTS,
    UART_HWCTRL_RTS_CTS,
} UART_HwCtrl_T;

typedef enum
{
    UART_RTN_SUCCESS,
    UART_RTN_TX_BUF_FULL,
    UART_RTN_NO_RX_DATA,
} UART_Return_T;

typedef void (*UART_Data_Ready_Callback)(void *cb_data);

typedef struct
{
    UART_ID_T uart_id;
    uint32_t baud_rate_bps;
    uint8_t n_data_bits;
    uint8_t n_stop_bits;
    UART_Parity_T parity;
    UART_HwCtrl_T hwctrl;
    uint8_t *tx_data_buffer;
    uint16_t tx_data_buffer_len;
    uint8_t *rx_data_buffer;
    uint16_t rx_data_buffer_len;
    UART_Data_Ready_Callback callback;
    void *cb_data;
} UART_Config_T;

// Nothing in this struct should be modified by application code
typedef struct
{
    UART_Config_T _config;
    uint8_t _error_bitmap;
    uint16_t _rx_num_bytes;
    uint16_t _tx_num_bytes;
    uint8_t *_rx_read_ptr;
    uint8_t *_rx_write_ptr;
    uint8_t *_tx_read_ptr;
    uint8_t *_tx_write_ptr;
} UART_T;

void USART_IRQHandler(UART_ID_T id);

/**
 ***************************************************************************************************
 *
 * @brief   Initialize a UART channel
 *
 * @param   *p_uart             UART structure
 * @param   *p_config_params    Config parameter structure.  All values in the
 *                              structure must be specified
 * @retval  none
 *
 **************************************************************************************************/
void UART_Init(UART_T *p_uart, UART_Config_T *p_config_params);

/**
 ***************************************************************************************************
 *
 * @brief   Register a callback that will be called from the ISR context when new
 *          received data is ready
 *
 * @param   *p_uart     UART structure
 * @param   *cb         Callback function
 * @param   *cb_data    arbritrary chunk of data to pass to the cb function.
 *                      Useful, for example, if this code is used by C++ code, and the "this"
 *                        pointer can be passed to the callback
 *
 * @retval  none
 *
 **************************************************************************************************/
void UART_RegisterDataReadyCB(UART_T *p_uart, UART_Data_Ready_Callback cb, void *cb_data);

/**
 ***************************************************************************************************
 *
 * @brief   Non-blocking UART transmit of a block of data
 *
 * Data is first transferred into a transmit buffer, and a UART interrupt is configured to transfer
 * data out of this buffer. There must be enough space in the buffer to store the data to be
 * transmitted, or this function will return error.
 *
 * @param   *p_uart                 UART structure
 * @param   *p_data                 data to be transmitted
 * @param   data_len                length of data to be transmitted
 * @retval  UART_RTN_SUCCESS        Success
 * @retval  UART_RTN_TX_BUF_FULL    TX buffer is full
 *
 **************************************************************************************************/
UART_Return_T UART_TransmitData(UART_T *p_uart, const uint8_t *data_ptr, const uint16_t data_len);

/**
 ***************************************************************************************************
 *
 * @brief   Non-blocking UART receive.
 *
 * Received data first transferred into a buffer from a UART interrupt. This function checks if
 * data exists in the receive buffer, and if it does, stores the byte and returns UART_RTN_SUCCESS.
 * If the buffer is empty, returns UART_RTN_NO_RX_DATA.
 *
 * @param   *p_uart                 UART structure
 * @param   *p_data                 data to store received byte
 * @retval  UART_RTN_SUCCESS        Success
 * @retval  UART_RTN_NO_RX_DATA     RX buffer is empty (no data available)
 *
 **************************************************************************************************/
UART_Return_T UART_ReceiveByte(UART_T *p_uart, uint8_t *byte_ptr);

#endif /* UART_H_ */
