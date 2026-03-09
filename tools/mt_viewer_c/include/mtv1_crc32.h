#ifndef MTV1_CRC32_H
#define MTV1_CRC32_H

#include <stdint.h>
#include <stddef.h>

/* CRC32/IEEE: polynomial 0xEDB88320 (reflected)
   Returns raw CRC (no final XOR) — chain calls by passing result as seed.
   Final XOR 0xFFFFFFFF applied once by caller at the end. */
uint32_t mtv1_crc32_update(const void* data, size_t len, uint32_t seed);

/* Convenience: full CRC32 with correct framing (init + update + final XOR) */
uint32_t mtv1_crc32_full(const void* data, size_t len);

#endif /* MTV1_CRC32_H */
