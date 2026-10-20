#include "protocol.hpp"

#include <algorithm>
#include <array>
#include <iterator>

namespace edge {
namespace {

std::uint16_t read_u16_le(const std::uint8_t* data) {
    return static_cast<std::uint16_t>(data[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) << 8U);
}

std::uint32_t read_u32_le(const std::uint8_t* data) {
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8U) |
           (static_cast<std::uint32_t>(data[2]) << 16U) |
           (static_cast<std::uint32_t>(data[3]) << 24U);
}

}  // namespace

std::uint16_t crc16_ccitt_false(std::span<const std::uint8_t> data) {
    std::uint16_t crc = 0xFFFF;
    for (const auto byte : data) {
        crc ^= static_cast<std::uint16_t>(static_cast<std::uint16_t>(byte) << 8U);
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000U) != 0U
                      ? static_cast<std::uint16_t>((crc << 1U) ^ 0x1021U)
                      : static_cast<std::uint16_t>(crc << 1U);
        }
    }
    return crc;
}

std::vector<Frame> StreamParser::feed(std::span<const std::uint8_t> bytes) {
    buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
    std::vector<Frame> frames;
    constexpr std::array<std::uint8_t, 2> magic_bytes{kMagic0, kMagic1};

    while (true) {
        const auto magic = std::search(buffer_.begin(), buffer_.end(),
                                       magic_bytes.begin(), magic_bytes.end());
        if (magic == buffer_.end()) {
            const bool keep_prefix = !buffer_.empty() && buffer_.back() == kMagic0;
            const auto keep_count = keep_prefix ? std::size_t{1} : std::size_t{0};
            stats_.discarded_bytes += buffer_.size() - keep_count;
            if (keep_prefix) {
                buffer_.assign(1, kMagic0);
            } else {
                buffer_.clear();
            }
            break;
        }
        if (magic != buffer_.begin()) {
            const auto discarded = static_cast<std::size_t>(std::distance(buffer_.begin(), magic));
            stats_.discarded_bytes += discarded;
            buffer_.erase(buffer_.begin(), magic);
        }

        if (buffer_.size() < kFixedHeaderSize) {
            break;
        }

        const auto payload_size = static_cast<std::size_t>(read_u16_le(buffer_.data() + 4));
        if (payload_size > kMaxPayloadSize || buffer_[2] != kProtocolVersion) {
            ++stats_.format_errors;
            ++stats_.discarded_bytes;
            buffer_.erase(buffer_.begin());
            continue;
        }

        const auto frame_size = kFixedHeaderSize + payload_size + kCrcSize;
        if (buffer_.size() < frame_size) {
            break;
        }

        const auto received_crc = read_u16_le(buffer_.data() + frame_size - kCrcSize);
        const auto calculated_crc = crc16_ccitt_false(
            std::span<const std::uint8_t>(buffer_.data() + 2, frame_size - 2 - kCrcSize));
        if (received_crc != calculated_crc) {
            ++stats_.crc_errors;
            ++stats_.discarded_bytes;
            buffer_.erase(buffer_.begin());
            continue;
        }

        Frame frame;
        frame.version = buffer_[2];
        frame.type = static_cast<MessageType>(buffer_[3]);
        frame.sequence = read_u32_le(buffer_.data() + 6);
        frame.timestamp_ms = read_u32_le(buffer_.data() + 10);
        frame.payload.assign(buffer_.begin() + static_cast<std::ptrdiff_t>(kFixedHeaderSize),
                             buffer_.begin() + static_cast<std::ptrdiff_t>(kFixedHeaderSize + payload_size));
        frames.push_back(std::move(frame));
        ++stats_.valid_frames;
        buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(frame_size));
    }

    return frames;
}

}  // namespace edge

