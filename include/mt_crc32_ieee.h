/*
 * MCU Malloc Tracker - CRC32/IEEE Interface
 *
 * IEEE CRC32 (polynomial 0xEDB88320) for snapshot integrity.
 * No malloc, no FPU, deterministic.
 */

#ifndef MT_CRC32_IEEE_H
#define MT_CRC32_IEEE_H

#include <stddef.h>
#include <stdint.h>

/**
 * mt_crc32_ieee_update(data, n, seed)
 * Process n bytes with existing CRC seed.
 * Returns raw CRC (no final XOR) — enables chaining multiple calls.
 * Final XOR applied once after all data processed.
 *
 * Contract: seed=0xFFFFFFFF → process bytes → final XOR ^0xFFFFFFFF once
 */
uint32_t mt_crc32_ieee_update(const void* data, size_t n, uint32_t seed);

/**
 * mt_crc32_ieee_full(data, n)
 * Compute CRC32 of data with init/update/final XOR.
 * Convenience function for single buffer.
 */
uint32_t mt_crc32_ieee_full(const void* data, size_t n);

#endif /* MT_CRC32_IEEE_H */
