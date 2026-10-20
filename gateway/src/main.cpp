#include "protocol.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <termios.h>
#include <unistd.h>

namespace {

volatile std::sig_atomic_t g_running = 1;

void on_signal(int) {
    g_running = 0;
}

std::uint16_t read_u16_le(std::span<const std::uint8_t> data, std::size_t offset) {
    return static_cast<std::uint16_t>(data[offset]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[offset + 1]) << 8U);
}

std::int16_t read_i16_le(std::span<const std::uint8_t> data, std::size_t offset) {
    return static_cast<std::int16_t>(read_u16_le(data, offset));
}

std::uint32_t read_u32_le(std::span<const std::uint8_t> data, std::size_t offset) {
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8U) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16U) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24U);
}

float read_f32_le(std::span<const std::uint8_t> data, std::size_t offset) {
    const auto bits = read_u32_le(data, offset);
    float value{};
    static_assert(sizeof(value) == sizeof(bits));
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

speed_t baud_constant(int baud) {
    switch (baud) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        default: throw std::runtime_error("unsupported baud rate: " + std::to_string(baud));
    }
}

class Input {
public:
    Input(const std::string& path, int baud) {
        if (path == "-") {
            fd_ = STDIN_FILENO;
            owned_ = false;
            return;
        }

        fd_ = ::open(path.c_str(), O_RDONLY | O_NOCTTY);
        if (fd_ < 0) {
            throw std::runtime_error("cannot open " + path + ": " + std::strerror(errno));
        }
        owned_ = true;

        termios tty{};
        if (tcgetattr(fd_, &tty) != 0) {
            throw std::runtime_error("tcgetattr failed: " + std::string(std::strerror(errno)));
        }
        cfmakeraw(&tty);
        const auto speed = baud_constant(baud);
        cfsetispeed(&tty, speed);
        cfsetospeed(&tty, speed);
        tty.c_cflag |= CLOCAL | CREAD;
        tty.c_cflag &= static_cast<tcflag_t>(~(PARENB | CSTOPB | CRTSCTS));
        tty.c_cflag = static_cast<tcflag_t>((tty.c_cflag & ~CSIZE) | CS8);
        tty.c_cc[VMIN] = 1;
        tty.c_cc[VTIME] = 0;
        if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
            throw std::runtime_error("tcsetattr failed: " + std::string(std::strerror(errno)));
        }
    }

    ~Input() {
        if (owned_ && fd_ >= 0) {
            ::close(fd_);
        }
    }

    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;

    ssize_t read(std::span<std::uint8_t> buffer) const {
        return ::read(fd_, buffer.data(), buffer.size());
    }

private:
    int fd_{-1};
    bool owned_{false};
};

struct Options {
    std::string input{"-"};
    int baud{115200};
    std::uint64_t max_frames{0};
};

Options parse_options(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--input" && i + 1 < argc) {
            options.input = argv[++i];
        } else if (arg == "--baud" && i + 1 < argc) {
            options.baud = std::stoi(argv[++i]);
        } else if (arg == "--max-frames" && i + 1 < argc) {
            options.max_frames = std::stoull(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Usage: edge_collector [--input DEVICE|-] [--baud RATE] [--max-frames N]\n";
            std::exit(0);
        } else {
            throw std::runtime_error("invalid argument: " + std::string(arg));
        }
    }
    return options;
}

class SequenceTracker {
public:
    void observe(std::uint32_t sequence) {
        if (last_.has_value()) {
            const auto expected = static_cast<std::uint32_t>(*last_ + 1U);
            if (sequence != expected) {
                const auto gap = static_cast<std::uint32_t>(sequence - expected);
                missing_ += gap;
                std::cerr << "sequence gap: expected=" << expected << " received=" << sequence << '\n';
            }
        }
        last_ = sequence;
    }

    [[nodiscard]] std::uint64_t missing() const noexcept { return missing_; }

private:
    std::optional<std::uint32_t> last_;
    std::uint64_t missing_{};
};

void print_status(const edge::Frame& frame) {
    if (frame.payload.size() != 16) {
        std::cerr << "invalid status payload length: " << frame.payload.size() << '\n';
        return;
    }
    const auto payload = std::span<const std::uint8_t>(frame.payload);
    std::cout << "STATUS seq=" << frame.sequence
              << " time_ms=" << frame.timestamp_ms
              << " temperature_c=" << read_f32_le(payload, 0)
              << " current_a=" << read_f32_le(payload, 4)
              << " rpm=" << read_u32_le(payload, 8)
              << " flags=0x" << std::hex << read_u32_le(payload, 12) << std::dec << '\n';
}

void print_vibration(const edge::Frame& frame) {
    if (frame.payload.size() < 4) {
        std::cerr << "invalid vibration payload length\n";
        return;
    }
    const auto payload = std::span<const std::uint8_t>(frame.payload);
    const auto sample_rate = read_u16_le(payload, 0);
    const auto sample_count = read_u16_le(payload, 2);
    const auto expected_size = 4U + static_cast<std::size_t>(sample_count) * 6U;
    if (payload.size() != expected_size || sample_count == 0) {
        std::cerr << "invalid vibration sample count\n";
        return;
    }

    double sum_square = 0.0;
    std::int16_t peak = 0;
    for (std::size_t i = 0; i < sample_count; ++i) {
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const auto value = read_i16_le(payload, 4U + i * 6U + axis * 2U);
            const auto magnitude = static_cast<std::int16_t>(std::abs(static_cast<int>(value)));
            peak = std::max(peak, magnitude);
            const auto normalized = static_cast<double>(value) / 1000.0;
            sum_square += normalized * normalized;
        }
    }
    const auto rms = std::sqrt(sum_square / static_cast<double>(sample_count * 3U));
    std::cout << "VIBRATION seq=" << frame.sequence
              << " time_ms=" << frame.timestamp_ms
              << " sample_rate_hz=" << sample_rate
              << " samples=" << sample_count
              << " rms_g=" << rms
              << " peak_mg=" << peak << '\n';
}

void process_frame(const edge::Frame& frame) {
    switch (frame.type) {
        case edge::MessageType::DeviceStatus:
            print_status(frame);
            break;
        case edge::MessageType::VibrationBatch:
            print_vibration(frame);
            break;
        default:
            std::cout << "UNKNOWN seq=" << frame.sequence
                      << " type=0x" << std::hex << static_cast<int>(frame.type) << std::dec
                      << " payload_bytes=" << frame.payload.size() << '\n';
            break;
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parse_options(argc, argv);
        std::signal(SIGINT, on_signal);
        std::signal(SIGTERM, on_signal);

        Input input(options.input, options.baud);
        edge::StreamParser parser;
        SequenceTracker sequences;
        std::array<std::uint8_t, 512> buffer{};
        std::uint64_t processed = 0;

        while (g_running != 0) {
            const auto count = input.read(buffer);
            if (count == 0) {
                break;
            }
            if (count < 0) {
                if (errno == EINTR) {
                    continue;
                }
                throw std::runtime_error("read failed: " + std::string(std::strerror(errno)));
            }

            const auto frames = parser.feed(std::span<const std::uint8_t>(
                buffer.data(), static_cast<std::size_t>(count)));
            for (const auto& frame : frames) {
                sequences.observe(frame.sequence);
                process_frame(frame);
                ++processed;
                if (options.max_frames != 0 && processed >= options.max_frames) {
                    g_running = 0;
                    break;
                }
            }
        }

        const auto& stats = parser.stats();
        std::cerr << "collector stats: valid=" << stats.valid_frames
                  << " crc_errors=" << stats.crc_errors
                  << " format_errors=" << stats.format_errors
                  << " discarded_bytes=" << stats.discarded_bytes
                  << " missing_sequences=" << sequences.missing() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}

