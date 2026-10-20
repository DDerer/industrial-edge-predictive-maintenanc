#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace edge {

constexpr std::uint8_t kMagic0 = 0xAA;
constexpr std::uint8_t kMagic1 = 0x55;
constexpr std::uint8_t kProtocolVersion = 1;
constexpr std::size_t kFixedHeaderSize = 14;
constexpr std::size_t kCrcSize = 2;
constexpr std::size_t kMaxPayloadSize = 4096;

enum class MessageType : std::uint8_t {
    DeviceStatus = 0x10,
    VibrationBatch = 0x11,
    ControlCommand = 0x20,
    CommandResponse = 0x21,
};

struct Frame {
    std::uint8_t version{};
    MessageType type{};
    std::uint32_t sequence{};
    std::uint32_t timestamp_ms{};
    std::vector<std::uint8_t> payload;
};

struct ParserStats {
    std::uint64_t valid_frames{};
    std::uint64_t crc_errors{};
    std::uint64_t format_errors{};
    std::uint64_t discarded_bytes{};
};

std::uint16_t crc16_ccitt_false(std::span<const std::uint8_t> data);

class StreamParser {
public:
    std::vector<Frame> feed(std::span<const std::uint8_t> bytes);
    [[nodiscard]] const ParserStats& stats() const noexcept { return stats_; }

private:
    std::vector<std::uint8_t> buffer_;
    ParserStats stats_;
};

}  // namespace edge

