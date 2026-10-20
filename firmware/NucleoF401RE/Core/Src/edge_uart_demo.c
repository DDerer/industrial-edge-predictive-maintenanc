#include "edge_uart_demo.h"
#include "edge_protocol.h"

#include <stdint.h>

#define DEMO_SAMPLE_COUNT 32u
#define DEMO_FRAME_CAPACITY (EDGE_PROTOCOL_FRAME_OVERHEAD + 4u + 6u * DEMO_SAMPLE_COUNT)

static uint8_t tx_buffer[DEMO_FRAME_CAPACITY];
static volatile uint8_t tx_busy;
static uint32_t sequence;
static uint32_t next_status_ms;
static uint32_t next_vibration_ms;

static void make_samples(int16_t *xyz) {
    for (uint16_t i = 0; i < DEMO_SAMPLE_COUNT; ++i) {
        const int16_t triangle = i < 16u ? (int16_t)(i * 8u)
                                          : (int16_t)((32u - i) * 8u);
        xyz[3u * i] = triangle;
        xyz[3u * i + 1u] = (int16_t)(triangle / 2);
        xyz[3u * i + 2u] = (int16_t)(1000 + triangle / 4);
    }
}

void edge_uart_demo_poll(UART_HandleTypeDef *uart) {
    if (uart == NULL || tx_busy != 0u) {
        return;
    }

    const uint32_t now = HAL_GetTick();
    size_t length = 0u;
    if ((int32_t)(now - next_status_ms) >= 0) {
        length = edge_encode_status(tx_buffer, sizeof(tx_buffer), sequence, now,
                                    31.5f, 0.72f, 1480u, 0u);
        next_status_ms = now + 1000u;
    } else if ((int32_t)(now - next_vibration_ms) >= 0) {
        int16_t xyz[DEMO_SAMPLE_COUNT * 3u];
        make_samples(xyz);
        length = edge_encode_vibration(tx_buffer, sizeof(tx_buffer), sequence,
                                       now, 1000u, xyz, DEMO_SAMPLE_COUNT);
        next_vibration_ms = now + 32u;
    }

    if (length != 0u) {
        tx_busy = 1u;
        if (HAL_UART_Transmit_DMA(uart, tx_buffer, (uint16_t)length) == HAL_OK) {
            ++sequence;
        } else {
            tx_busy = 0u;
        }
    }
}

void edge_uart_demo_tx_complete(UART_HandleTypeDef *uart) {
    if (uart != NULL && uart->Instance == USART2) {
        tx_busy = 0u;
    }
}

void edge_uart_demo_error(UART_HandleTypeDef *uart) {
    if (uart != NULL && uart->Instance == USART2) {
        tx_busy = 0u;
    }
}
