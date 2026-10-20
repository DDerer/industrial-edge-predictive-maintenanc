#ifndef EDGE_PROTOCOL_H
#define EDGE_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EDGE_PROTOCOL_VERSION 1u
#define EDGE_PROTOCOL_MAX_PAYLOAD 4096u
#define EDGE_PROTOCOL_FRAME_OVERHEAD 16u
#define EDGE_MSG_DEVICE_STATUS 0x10u
#define EDGE_MSG_VIBRATION_BATCH 0x11u

/* Return the encoded frame length, or zero if an argument/capacity is invalid. */
size_t edge_encode_status(uint8_t *out, size_t capacity, uint32_t sequence,
                          uint32_t timestamp_ms, float temperature_c,
                          float current_a, uint32_t rpm, uint32_t flags);

/* xyz contains sample_count interleaved X, Y, Z int16 samples in millig. */
size_t edge_encode_vibration(uint8_t *out, size_t capacity, uint32_t sequence,
                             uint32_t timestamp_ms, uint16_t sample_rate_hz,
                             const int16_t *xyz, uint16_t sample_count);

#ifdef __cplusplus
}
#endif

#endif
