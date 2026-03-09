/*
 * MCU Malloc Tracker - Phase 2 Self-Test
 *
 * Test scenario:
 * - 3 malloc calls
 * - 1 free call
 * - 1 realloc call
 * - 1 leak (intentional)
 *
 * Expected counters:
 * - total_allocs = 3
 * - total_frees = 1
 * - total_reallocs = 1
 * - alloc_count = 2 (3 - 1 freed = 2 active)
 * - current_used = sizeof(block2) + sizeof(block3_resized)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/mt_config.h"
#include "../../include/mt_api.h"

/* Override malloc/free/realloc for demo (debug build only) */
#define malloc(x)       mt_malloc((x), __FILE__, __LINE__)
#define free(p)         mt_free((p), __FILE__, __LINE__)
#define realloc(p, x)   mt_realloc((p), (x), __FILE__, __LINE__)

/* Simple output function for dumps */
static void simple_write(const char* s)
{
    printf("%s", s);
}

int main(void)
{
    printf("=== MCU Malloc Tracker - Phase 2 Self-Test ===\n\n");

    /* Initialize tracker */
    printf("[1] Initializing tracker...\n");
    mt_init();

    /* Scenario: 3 malloc */
    printf("[2] Allocating 3 blocks...\n");
    void* block1 = malloc(64);      /* malloc #1 */
    printf("    block1 = %p (64 bytes)\n", block1);

    void* block2 = malloc(128);     /* malloc #2 */
    printf("    block2 = %p (128 bytes)\n", block2);

    void* block3 = malloc(256);     /* malloc #3 */
    printf("    block3 = %p (256 bytes)\n", block3);

    /* Check stats after malloc */
    mt_heap_stats_t stats = mt_stats();
    printf("\nAfter 3 mallocs:\n");
    printf("  total_allocs = %u (expected 3)\n", stats.total_allocs);
    printf("  alloc_count = %u (expected 3)\n", stats.alloc_count);
    printf("  current_used = %u (expected 448 = 64+128+256)\n", stats.current_used);
    printf("  peak_used = %u (expected 448)\n", stats.peak_used);

    /* Free one block */
    printf("\n[3] Freeing block1...\n");
    free(block1);                   /* free #1 */
    printf("    freed block1\n");

    stats = mt_stats();
    printf("\nAfter 1 free:\n");
    printf("  total_frees = %u (expected 1)\n", stats.total_frees);
    printf("  alloc_count = %u (expected 2)\n", stats.alloc_count);
    printf("  current_used = %u (expected 384 = 128+256)\n", stats.current_used);
    printf("  table_tombstones = %u (expected 1)\n", stats.table_tombstones);

    /* Realloc one block (resize block3 from 256 to 512) */
    printf("\n[4] Reallocating block3 from 256 to 512 bytes...\n");
    void* block3_resized = realloc(block3, 512);  /* realloc #1 */
    printf("    block3 resized: %p -> %p\n", block3, block3_resized);

    stats = mt_stats();
    printf("\nAfter 1 realloc:\n");
    printf("  total_reallocs = %u (expected 1)\n", stats.total_reallocs);
    printf("  alloc_count = %u (expected 2)\n", stats.alloc_count);
    printf("  current_used = %u (expected 640 = 128+512)\n", stats.current_used);
    printf("  peak_used = %u (expected 768 = 64+128+512)\n", stats.peak_used);

    /* Block2 is still allocated (leak) */
    printf("\n[5] Block2 remains allocated (intentional leak for testing)...\n");

    /* Final stats */
    stats = mt_stats();
    printf("\nFinal stats:\n");
    printf("  total_allocs = %u (expected 3)\n", stats.total_allocs);
    printf("  total_frees = %u (expected 1)\n", stats.total_frees);
    printf("  total_reallocs = %u (expected 1)\n", stats.total_reallocs);
    printf("  alloc_count = %u (expected 2: block2 + block3_resized)\n", stats.alloc_count);
    printf("  current_used = %u bytes\n", stats.current_used);
    printf("  peak_used = %u bytes\n", stats.peak_used);
    printf("  table_tombstones = %u (expected 1)\n", stats.table_tombstones);

    /* Validate critical counters */
    printf("\n=== Validation ===\n");
    int pass = 1;

    if (stats.total_allocs != 3) {
        printf("❌ FAIL: total_allocs = %u (expected 3)\n", stats.total_allocs);
        pass = 0;
    } else {
        printf("✅ PASS: total_allocs = 3\n");
    }

    if (stats.total_frees != 1) {
        printf("❌ FAIL: total_frees = %u (expected 1)\n", stats.total_frees);
        pass = 0;
    } else {
        printf("✅ PASS: total_frees = 1\n");
    }

    if (stats.total_reallocs != 1) {
        printf("❌ FAIL: total_reallocs = %u (expected 1)\n", stats.total_reallocs);
        pass = 0;
    } else {
        printf("✅ PASS: total_reallocs = 1\n");
    }

    if (stats.alloc_count != 2) {
        printf("❌ FAIL: alloc_count = %u (expected 2)\n", stats.alloc_count);
        pass = 0;
    } else {
        printf("✅ PASS: alloc_count = 2\n");
    }

    if (stats.current_used != 640) {
        printf("❌ FAIL: current_used = %u (expected 640)\n", stats.current_used);
        pass = 0;
    } else {
        printf("✅ PASS: current_used = 640\n");
    }

    printf("\n%s\n", pass ? "=== ALL CHECKS PASSED ✅ ===" : "=== SOME CHECKS FAILED ❌ ===");

    return pass ? 0 : 1;
}
