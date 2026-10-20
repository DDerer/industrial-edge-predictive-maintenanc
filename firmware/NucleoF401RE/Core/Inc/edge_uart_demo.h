#ifndef EDGE_UART_DEMO_H
#define EDGE_UART_DEMO_H

#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Call once from the generated main loop after MX_USART2_UART_Init(). */
void edge_uart_demo_poll(UART_HandleTypeDef *uart);

/* Call from HAL_UART_TxCpltCallback for USART2. */
void edge_uart_demo_tx_complete(UART_HandleTypeDef *uart);

/* Call from HAL_UART_ErrorCallback for USART2. */
void edge_uart_demo_error(UART_HandleTypeDef *uart);

#ifdef __cplusplus
}
#endif

#endif
