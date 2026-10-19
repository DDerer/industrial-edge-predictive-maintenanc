#!/usr/bin/env python3
"""Generate protocol-v1 frames that imitate an STM32 vibration node."""

import argparse
import math
import random
import struct
import sys
import time

MAGIC = b"\xAA\x55"
VERSION = 1
TYPE_STATUS = 0x10
TYPE_VIBRATION = 0x11


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def frame(message_type: int, sequence: int, timestamp_ms: int, payload: bytes) -> bytes:
    body = struct.pack("<BBHII", VERSION, message_type, len(payload), sequence, timestamp_ms)
    body += payload
    return MAGIC + body + struct.pack("<H", crc16_ccitt_false(body))


def status_payload(tick: int) -> bytes:
    temperature = 31.5 + 0.8 * math.sin(tick / 9.0)
    current = 0.72 + 0.05 * math.sin(tick / 4.0)
    rpm = 1480 + round(18 * math.sin(tick / 6.0))
    flags = 0
    return struct.pack("<ffII", temperature, current, rpm, flags)


def vibration_payload(tick: int, sample_rate: int = 1000, sample_count: int = 32) -> bytes:
    samples = bytearray(struct.pack("<HH", sample_rate, sample_count))
    frequency = 50.0
    for index in range(sample_count):
        phase = 2.0 * math.pi * frequency * (tick * sample_count + index) / sample_rate
        x = round(85 * math.sin(phase) + random.gauss(0, 7))
        y = round(48 * math.sin(phase + 0.7) + random.gauss(0, 7))
        z = round(1000 + 25 * math.sin(phase + 1.4) + random.gauss(0, 6))
        samples.extend(struct.pack("<hhh", x, y, z))
    return bytes(samples)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--frames", type=int, default=12, help="number of frames; 0 means forever")
    parser.add_argument("--delay", type=float, default=0.02, help="delay between frames in seconds")
    parser.add_argument("--corrupt-every", type=int, default=0, help="damage every Nth frame to test CRC")
    args = parser.parse_args()

    random.seed(401)
    sequence = 0
    started = time.monotonic()
    while args.frames == 0 or sequence < args.frames:
        timestamp_ms = int((time.monotonic() - started) * 1000) & 0xFFFFFFFF
        if sequence % 5 == 0:
            packet = frame(TYPE_STATUS, sequence, timestamp_ms, status_payload(sequence))
        else:
            packet = frame(TYPE_VIBRATION, sequence, timestamp_ms, vibration_payload(sequence))

        if args.corrupt_every > 0 and (sequence + 1) % args.corrupt_every == 0:
            damaged = bytearray(packet)
            damaged[-3] ^= 0x40
            packet = bytes(damaged)

        sys.stdout.buffer.write(packet)
        sys.stdout.buffer.flush()
        sequence += 1
        if args.delay > 0:
            time.sleep(args.delay)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

