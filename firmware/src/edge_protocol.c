#include "edge_protocol.h"

#include <string.h>

_Static_assert(sizeof(float) == 4, "protocol requires 32-bit float");

static void write_u16(uint8_t *out, uint16_t value) {
    out[0] = (uint8_t)(value & 0xffu);
    out[1] = (uint8_t)(value >> 8u);
}

static void write_u32(uint8_t *out, uint32_t value) {
    for (unsigned i = 0; i < 4u; ++i) {
        out[i] = (uint8_t)(value >> (i * 8u));
    }
}

static void write_float(uint8_t *out, float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    write_u32(out, bits);
}

static uint16_t crc16(const uint8_t *data, size_t length) {
    uint16_t crc = 0xffffu;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8u);
        for (unsigned bit = 0; bit < 8u; ++bit) {
            crc = (crc & 0x8000u) != 0u
                      ? (uint16_t)((crc << 1u) ^ 0x1021u)
                      : (uint16_t)(crc << 1u);
        }
    }
    return crc;
}

static size_t begin_frame(uint8_t *out, size_t capacity, uint8_t type,
                          uint16_t payload_length, uint32_t sequence,
                          uint32_t timestamp_ms) {
    const size_t total = EDGE_PROTOCOL_FRAME_OVERHEAD + (size_t)payload_length;
    if (out == NULL || payload_length > EDGE_PROTOCOL_MAX_PAYLOAD || capacity < total) {
        return 0u;
    }
    out[0] = 0xaau;
    out[1] = 0x55u;
    out[2] = EDGE_PROTOCOL_VERSION;
    out[3] = type;
    write_u16(out + 4, payload_length);
    write_u32(out + 6, sequence);
    write_u32(out + 10, timestamp_ms);
    return total;
}

static void finish_frame(uint8_t *out, size_t total) {
    write_u16(out + total - 2u, crc16(out + 2u, total - 4u));
}

size_t edge_encode_status(uint8_t *out, size_t capacity, uint32_t sequence,
                          uint32_t timestamp_ms, float temperature_c,
                          float current_a, uint32_t rpm, uint32_t flags) {
    const size_t total = begin_frame(out, capacity, EDGE_MSG_DEVICE_STATUS, 16u,
                                     sequence, timestamp_ms);
    if (total == 0u) {
        return 0u;
    }
    write_float(out + 14u, temperature_c);
    write_float(out + 18u, current_a);
    write_u32(out + 22u, rpm);
    write_u32(out + 26u, flags);
    finish_frame(out, total);
    return total;
}

size_t edge_encode_vibration(uint8_t *out, size_t capacity, uint32_t sequence,
                             uint32_t timestamp_ms, uint16_t sample_rate_hz,
                             const int16_t *xyz, uint16_t sample_count) {
    if (sample_rate_hz == 0u || sample_count == 0u || xyz == NULL ||
        sample_count > (EDGE_PROTOCOL_MAX_PAYLOAD - 4u) / 6u) {
        return 0u;
    }
    const uint16_t payload_length = (uint16_t)(4u + (uint32_t)sample_count * 6u);
    const size_t total = begin_frame(out, capacity, EDGE_MSG_VIBRATION_BATCH,
                                     payload_length, sequence, timestamp_ms);
    if (total == 0u) {
        return 0u;
    }
    write_u16(out + 14u, sample_rate_hz);
    write_u16(out + 16u, sample_count);
    for (size_t i = 0; i < (size_t)sample_count * 3u; ++i) {
        write_u16(out + 18u + i * 2u, (uint16_t)xyz[i]);
    }
    finish_frame(out, total);
    return total;
}
