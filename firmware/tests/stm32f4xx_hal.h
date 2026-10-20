#ifndef STM32F4XX_HAL_H
#define STM32F4XX_HAL_H

#include <stdint.h>

#define USART2 ((void *)0x40004400u)

typedef struct {
    void *Instance;
} UART_HandleTypeDef;

typedef enum {
    HAL_OK = 0,
    HAL_ERROR = 1
} HAL_StatusTypeDef;

#ifdef __cplusplus
extern "C" {
#endif

uint32_t HAL_GetTick(void);
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *uart,
                                        uint8_t *data, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif
