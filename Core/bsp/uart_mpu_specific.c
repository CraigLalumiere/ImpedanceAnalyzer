#include "uart_mpu_specific.h"
#include "qpc.h"
#include "stm32g474xx.h"

Q_DEFINE_THIS_MODULE("uart_mpu_specific")

extern UART_HandleTypeDef hlpuart1;
extern void USART_IRQHandler(UART_ID_T id);

// void USART1_IRQHandler(void)
// void LPUART1_IRQHandler(void)
// {
//     QK_ISR_ENTRY();
//     USART_IRQHandler(UART_ID_1);
//     QK_ISR_EXIT();
// }

UART_HandleTypeDef *STM32_UART_GetHandle(UART_ID_T uart_id)
{
    if (uart_id == LPUART_ID_1)
    {
        return &hlpuart1;
    }
    else
    {
        return 0;
    }
}

USART_TypeDef *STM32_UART_GetInstance(UART_ID_T uart_id)
{
    switch (uart_id)
    {
        case UART_ID_1:
            return USART1;
        case UART_ID_2:
            return USART2;
        case UART_ID_3:
            return USART3;
        case UART_ID_4:
            return UART4;
        case UART_ID_5:
            return UART5;
            break;
        case LPUART_ID_1:
            return LPUART1;
        default:
            Q_ASSERT(false);
            break;
    }
}

IRQn_Type STM32_UART_GetIrqType(UART_ID_T uart_id)
{
    switch (uart_id)
    {
        case UART_ID_1:
            return USART1_IRQn;
        case UART_ID_2:
            return USART2_IRQn;
        case UART_ID_3:
            return USART3_IRQn;
        case UART_ID_4:
            return UART4_IRQn;
        case UART_ID_5:
            return UART5_IRQn;
        case LPUART_ID_1:
            return LPUART1_IRQn;
        default:
            Q_ASSERT(false);
            break;
    }
}
