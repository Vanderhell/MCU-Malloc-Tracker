/*
 * MCU Malloc Tracker - CRC32/IEEE Implementation
 *
 * IEEE CRC32 (polynomial 0xEDB88320) for snapshot integrity.
 * Portable, no lookup table, no malloc.
 */

#include <stddef.h>
#include <stdint.h>
#include "../include/mt_crc32_ieee.h"

/**
 * mt_crc32_ieee_update(data, n, seed)
 * Process n bytes with existing CRC seed.
 * Returns raw CRC without final XOR — enables chaining.
 */
uint32_t mt_crc32_ieee_update(const void* data, size_t n, uint32_t seed)
{
    uint32_t crc = seed;
    const uint8_t* p = (const uint8_t*)data;

    for (size_t i = 0; i < n; i++) {
        uint8_t byte = p[i];
        crc ^= byte;

        for (int bit = 0; bit < 8; bit++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320u;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;  /* Return raw CRC (no final XOR) */
}

/**
 * mt_crc32_ieee_full(data, n)
 * Compute CRC32 of data with init/update/final XOR.
 */
uint32_t mt_crc32_ieee_full(const void* data, size_t n)
{
    uint32_t crc = 0xFFFFFFFFu;
    crc = mt_crc32_ieee_update(data, n, crc);
    crc ^= 0xFFFFFFFFu;
    return crc;
}
