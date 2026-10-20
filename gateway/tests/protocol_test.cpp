#include "protocol.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

namespace {

void append_u16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
    }
}

std::vector<std::uint8_t> make_frame(std::uint32_t sequence) {
    std::vector<std::uint8_t> frame{edge::kMagic0, edge::kMagic1,
                                    edge::kProtocolVersion,
                                    static_cast<std::uint8_t>(edge::MessageType::DeviceStatus)};
    append_u16(frame, 3);
    append_u32(frame, sequence);
    append_u32(frame, 1234);
    frame.insert(frame.end(), {0x10, 0x20, 0x30});
    const auto crc = edge::crc16_ccitt_false(
        std::span<const std::uint8_t>(frame.data() + 2, frame.size() - 2));
    append_u16(frame, crc);
    return frame;
}

}  // namespace

int main() {
    edge::StreamParser parser;
    const std::vector<std::uint8_t> noise{0x00, 0x7F, edge::kMagic0};
    auto frames = parser.feed(noise);
    assert(frames.empty());

    const auto valid = make_frame(42);
    for (std::size_t i = 1; i < valid.size(); ++i) {
        frames = parser.feed(std::span<const std::uint8_t>(&valid[i], 1));
        if (i + 1 < valid.size()) {
            assert(frames.empty());
        }
    }
    assert(frames.size() == 1);
    assert(frames.front().sequence == 42);
    assert(frames.front().timestamp_ms == 1234);
    assert(frames.front().payload == std::vector<std::uint8_t>({0x10, 0x20, 0x30}));

    auto corrupted = make_frame(43);
    corrupted[14] ^= 0x01;
    frames = parser.feed(corrupted);
    assert(frames.empty());

    const auto recovered = make_frame(44);
    frames = parser.feed(recovered);
    assert(frames.size() == 1);
    assert(frames.front().sequence == 44);
    assert(parser.stats().valid_frames == 2);
    assert(parser.stats().crc_errors == 1);
    assert(parser.stats().discarded_bytes >= 3);

    std::cout << "protocol stream parser tests passed\n";
    return 0;
}
