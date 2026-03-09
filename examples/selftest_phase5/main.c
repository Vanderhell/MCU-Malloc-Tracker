/*
 * MCU Malloc Tracker - Phase 5 Self-Test (Snapshot)
 *
 * Test scenario:
 * - 3 malloc calls
 * - 1 free call
 * - 1 realloc call
 * - 1 leak (intentional)
 * - Write binary snapshot
 * - Verify: magic, version, record count, ptr ordering, CRC
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/mt_config.h"
#include "../../include/mt_api.h"

#define malloc(x)       mt_malloc((x), __FILE__, __LINE__)
#define free(p)         mt_free((p), __FILE__, __LINE__)
#define realloc(p, x)   mt_realloc((p), (x), __FILE__, __LINE__)

/* Snapshot buffer */
static uint8_t snapshot_buf[2048];

int main(void)
{
    printf("=== MCU Malloc Tracker - Phase 5 Self-Test (Snapshot) ===\n\n");

    /* Initialize */
    printf("[1] Initializing tracker...\n");
    mt_init();

    /* Allocate 3 blocks */
    printf("[2] Allocating 3 blocks...\n");
    void* block1 = malloc(64);
    void* block2 = malloc(128);
    void* block3 = malloc(256);
    printf("    block1=%p (64B)\n", block1);
    printf("    block2=%p (128B)\n", block2);
    printf("    block3=%p (256B)\n", block3);

    /* Free block1 */
    printf("[3] Freeing block1...\n");
    free(block1);

    /* Realloc block3 */
    printf("[4] Reallocating block3 (256->512)...\n");
    void* block3_resized = realloc(block3, 512);
    printf("    block3_resized=%p\n", block3_resized);

    /* block2 is leak */
    printf("[5] block2 remains (leak test)...\n");

    /* Write snapshot */
    printf("[6] Writing binary snapshot...\n");
    size_t snap_size = mt_snapshot_write(snapshot_buf, sizeof(snapshot_buf));
    printf("    Snapshot size: %zu bytes\n", snap_size);

    /* Save to file for decoder testing */
    FILE* snap_file = fopen("phase5_snapshot.bin", "wb");
    if (snap_file) {
        fwrite(snapshot_buf, 1, snap_size, snap_file);
        fclose(snap_file);
        printf("    Saved to: phase5_snapshot.bin\n");
    }

    /* Verify snapshot */
    printf("\n[7] Verifying snapshot:\n");

    if (snap_size < 40) {
        printf("❌ FAIL: Snapshot too small (< 40 bytes)\n");
        return 1;
    }

    /* Parse header */
    uint8_t* p = snapshot_buf;

    char magic[4];
    memcpy(magic, p + 0, 4);
    uint16_t version = p[4] | (p[5] << 8);
    uint16_t flags = p[6] | (p[7] << 8);
    uint32_t record_count = p[8] | (p[9] << 8) | (p[10] << 16) | (p[11] << 24);
    uint32_t current_used = p[12] | (p[13] << 8) | (p[14] << 16) | (p[15] << 24);
    uint32_t crc32_in_header = p[32] | (p[33] << 8) | (p[34] << 16) | (p[35] << 24);

    printf("  magic: %.4s (expected MTS1)\n", magic);
    printf("  version: %u (expected 1)\n", version);
    printf("  flags: 0x%04x\n", flags);
    printf("  record_count: %u (expected 2: block2 + block3_resized)\n", record_count);
    printf("  current_used: %u bytes\n", current_used);
    printf("  crc32: 0x%08x\n", crc32_in_header);

    int pass = 1;

    if (magic[0] != 'M' || magic[1] != 'T' || magic[2] != 'S' || magic[3] != '1') {
        printf("❌ FAIL: Magic mismatch\n");
        pass = 0;
    } else {
        printf("✅ PASS: Magic = MTS1\n");
    }

    if (version != 1) {
        printf("❌ FAIL: Version != 1\n");
        pass = 0;
    } else {
        printf("✅ PASS: Version = 1\n");
    }

    if (record_count != 2) {
        printf("❌ FAIL: Record count = %u (expected 2)\n", record_count);
        pass = 0;
    } else {
        printf("✅ PASS: Record count = 2\n");
    }

    if (current_used != 640) {
        printf("❌ FAIL: current_used = %u (expected 640)\n", current_used);
        pass = 0;
    } else {
        printf("✅ PASS: current_used = 640\n");
    }

    /* Check CRC flag */
    if (flags & 0x0008) {
        printf("✅ PASS: CRC flag set\n");
    } else {
        printf("❌ FAIL: CRC flag not set\n");
        pass = 0;
    }

    /* Verify record ptr ordering */
    printf("\n[8] Verifying record ptr ordering:\n");
    if (record_count >= 2) {
        uint8_t* rec0 = snapshot_buf + 40 + 0;  /* First record */
        uint8_t* rec1 = snapshot_buf + 40 + 24; /* Second record */

        uint64_t ptr0 = (uint64_t)rec0[0] |
                        ((uint64_t)rec0[1] << 8) |
                        ((uint64_t)rec0[2] << 16) |
                        ((uint64_t)rec0[3] << 24) |
                        ((uint64_t)rec0[4] << 32) |
                        ((uint64_t)rec0[5] << 40) |
                        ((uint64_t)rec0[6] << 48) |
                        ((uint64_t)rec0[7] << 56);

        uint64_t ptr1 = (uint64_t)rec1[0] |
                        ((uint64_t)rec1[1] << 8) |
                        ((uint64_t)rec1[2] << 16) |
                        ((uint64_t)rec1[3] << 24) |
                        ((uint64_t)rec1[4] << 32) |
                        ((uint64_t)rec1[5] << 40) |
                        ((uint64_t)rec1[6] << 48) |
                        ((uint64_t)rec1[7] << 56);

        printf("  Record 0 ptr: 0x%016llx\n", (unsigned long long)ptr0);
        printf("  Record 1 ptr: 0x%016llx\n", (unsigned long long)ptr1);

        if (ptr0 < ptr1) {
            printf("✅ PASS: Records ordered by ptr (ASC)\n");
        } else {
            printf("❌ FAIL: Records not ordered (ptr0 >= ptr1)\n");
            pass = 0;
        }
    }

    /* Dump hex of first 64 bytes */
    printf("\n[9] Snapshot hex (first 64 bytes):\n");
    for (int i = 0; i < 64 && i < (int)snap_size; i++) {
        if (i % 16 == 0) printf("  ");
        printf("%02x ", snapshot_buf[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");

    printf("%s\n", pass ? "=== ALL CHECKS PASSED ✅ ===" : "=== SOME CHECKS FAILED ❌ ===");

    return pass ? 0 : 1;
}
