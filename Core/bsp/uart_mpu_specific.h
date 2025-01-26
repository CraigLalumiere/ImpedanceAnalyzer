#ifndef UART_MPU_SPECIFIC_H_
#define UART_MPU_SPECIFIC_H_

#include "interfaces/uart.h"
#include "stm32g4xx.h"

UART_HandleTypeDef *STM32_UART_GetHandle(UART_ID_T uart_id);
USART_TypeDef *STM32_UART_GetInstance(UART_ID_T uart_id);
IRQn_Type STM32_UART_GetIrqType(UART_ID_T uart_id);

#endif // UART_MPU_SPECIFIC_H_