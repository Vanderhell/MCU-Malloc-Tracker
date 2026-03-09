#include "mtv1_crc32.h"

/* CRC32 polynomial: reflected 0xEDB88320 (Ethernet, ZIP, PNG, etc.)
   This implementation mirrors the tracker's mt_crc32_ieee() exactly. */

uint32_t mtv1_crc32_update(const void* data, size_t len, uint32_t crc) {
    const uint8_t* bytes = (const uint8_t*)data;

    for (size_t i = 0; i < len; i++) {
        uint8_t byte = bytes[i];
        crc ^= byte;

        /* Process 8 bits */
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320u;
            else
                crc = crc >> 1;
        }
    }

    return crc;
}

uint32_t mtv1_crc32_full(const void* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    crc = mtv1_crc32_update(data, len, crc);
    crc ^= 0xFFFFFFFFu;
    return crc;
}
