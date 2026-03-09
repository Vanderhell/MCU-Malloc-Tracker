/*
 * CRC32 Verification Test
 *
 * Reads golden test vector and computes CRC using C implementation.
 * Verifies it matches Python-computed expected value.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* CRC32 Implementation (matches mt_snapshot.c) */
static uint32_t mt_crc32_ieee(const void* data, size_t n, uint32_t seed)
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

int main(void)
{
    printf("=== CRC32 Verification Test ===\n");
    printf("Verifies C and Python CRC32 implementations match on golden vector\n\n");

    /* Read golden snapshot (40-byte header + 24-byte record) */
    FILE* f = fopen("../../tools/_vectors/phase5_crc_snapshot.bin", "rb");
    if (!f) {
        printf("ERROR: Cannot open phase5_crc_snapshot.bin\n");
        return 1;
    }

    uint8_t snapshot[64];
    size_t bytes_read = fread(snapshot, 1, sizeof(snapshot), f);
    fclose(f);

    if (bytes_read != 64) {
        printf("ERROR: Expected 64 bytes, got %zu\n", bytes_read);
        return 1;
    }

    printf("[1] Read golden snapshot (64 bytes: 40B header + 24B record)\n");

    /* Extract CRC from snapshot header (bytes 32-35) */
    uint32_t embedded_crc = snapshot[32] |
                            (snapshot[33] << 8) |
                            (snapshot[34] << 16) |
                            (snapshot[35] << 24);
    printf("    Embedded CRC in snapshot: 0x%08x\n", embedded_crc);

    /* Compute CRC using C implementation (strict contract) */
    /* Header bytes are 0-31 (CRC field must be zeroed for computation) */
    /* Record bytes are 40-63 (24 bytes) */
    uint32_t crc = 0xFFFFFFFFu;
    crc = mt_crc32_ieee(snapshot, 32, crc);      /* Process header (bytes 0-31) */
    crc = mt_crc32_ieee(snapshot + 40, 24, crc); /* Process record (bytes 40-63) */
    crc ^= 0xFFFFFFFFu;  /* Final XOR applied once after all data */

    printf("\n[2] Computed CRC using C mt_crc32_ieee: 0x%08x\n", crc);

    /* Verify against embedded CRC */
    if (crc == embedded_crc) {
        printf("\n✅ PASS: C CRC matches embedded CRC!\n");
        printf("   Both compute: 0x%08x\n", crc);
        return 0;
    } else {
        printf("\n❌ FAIL: CRC mismatch!\n");
        printf("   C computed: 0x%08x\n", crc);
        printf("   Embedded:   0x%08x\n", embedded_crc);
        return 1;
    }
}
