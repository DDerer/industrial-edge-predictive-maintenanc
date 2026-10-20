#include "edge_protocol.h"
#include "protocol.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <span>

int main() {
    edge::StreamParser parser;
    std::array<std::uint8_t, EDGE_PROTOCOL_FRAME_OVERHEAD + 16> status{};
    const auto status_size = edge_encode_status(status.data(), status.size(), 7, 1234,
                                                 31.5f, 0.75f, 1480, 3);
    if (status_size != status.size() ||
        edge_encode_status(status.data(), status.size() - 1, 7, 1234,
                           31.5f, 0.75f, 1480, 3) != 0) {
        return 1;
    }
    auto frames = parser.feed(std::span(status.data(), status_size));
    if (frames.size() != 1 || frames[0].sequence != 7 ||
        frames[0].timestamp_ms != 1234 ||
        frames[0].type != edge::MessageType::DeviceStatus ||
        frames[0].payload.size() != 16 ||
        frames[0].payload[0] != 0 || frames[0].payload[1] != 0 ||
        frames[0].payload[2] != 0xfc || frames[0].payload[3] != 0x41) {
        return 2;
    }

    const std::array<std::int16_t, 6> xyz{-32768, 0, 32767, 100, -200, 300};
    std::array<std::uint8_t, EDGE_PROTOCOL_FRAME_OVERHEAD + 4 + 12> vibration{};
    const auto vibration_size = edge_encode_vibration(
        vibration.data(), vibration.size(), 8, 1266, 1000, xyz.data(), 2);
    if (vibration_size != vibration.size() ||
        edge_encode_vibration(vibration.data(), vibration.size() - 1,
                              8, 1266, 1000, xyz.data(), 2) != 0 ||
        edge_encode_vibration(vibration.data(), vibration.size(),
                              8, 1266, 0, xyz.data(), 2) != 0 ||
        edge_encode_vibration(vibration.data(), vibration.size(),
                              8, 1266, 1000, nullptr, 2) != 0) {
        return 3;
    }
    frames = parser.feed(std::span(vibration.data(), vibration_size));
    if (frames.size() != 1 || frames[0].sequence != 8 ||
        frames[0].type != edge::MessageType::VibrationBatch ||
        frames[0].payload.size() != 16 ||
        frames[0].payload[4] != 0 || frames[0].payload[5] != 0x80 ||
        frames[0].payload[8] != 0xff || frames[0].payload[9] != 0x7f ||
        parser.stats().crc_errors != 0) {
        return 4;
    }

    std::cout << "firmware encoder and gateway parser are compatible\n";
    return 0;
}
