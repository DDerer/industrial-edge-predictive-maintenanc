#include "edge_uart_demo.h"
#include "protocol.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace {
std::uint32_t tick;
std::vector<std::uint8_t> sent;
}

extern "C" std::uint32_t HAL_GetTick(void) { return tick; }

extern "C" HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *,
                                                     std::uint8_t *data,
                                                     std::uint16_t length) {
    sent.assign(data, data + length);
    return HAL_OK;
}

int main() {
    UART_HandleTypeDef uart{USART2};
    edge::StreamParser parser;

    edge_uart_demo_poll(&uart);
    auto frames = parser.feed(std::span(sent));
    if (frames.size() != 1 || frames[0].sequence != 0 ||
        frames[0].type != edge::MessageType::DeviceStatus) {
        return 1;
    }

    sent.clear();
    edge_uart_demo_poll(&uart);
    if (!sent.empty()) {
        return 2;
    }

    edge_uart_demo_tx_complete(&uart);
    edge_uart_demo_poll(&uart);
    frames = parser.feed(std::span(sent));
    if (frames.size() != 1 || frames[0].sequence != 1 ||
        frames[0].type != edge::MessageType::VibrationBatch ||
        frames[0].payload.size() != 196) {
        return 3;
    }

    return 0;
}
