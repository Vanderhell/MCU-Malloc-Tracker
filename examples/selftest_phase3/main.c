/*
 * MCU Malloc Tracker - Phase 3 Self-Test
 *
 * Test heap statistics with fragmentation.
 * Tests 2 modes:
 * - MT_PLATFORM_HEAP_WALK=0: Fragmentation N/A (default)
 * - MT_PLATFORM_HEAP_WALK=1: Fragmentation with stub implementation (optional)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/mt_config.h"
#include "../../include/mt_api.h"

/* Override malloc/free/realloc for demo */
#define malloc(x)       mt_malloc((x), __FILE__, __LINE__)
#define free(p)         mt_free((p), __FILE__, __LINE__)
#define realloc(p, x)   mt_realloc((p), (x), __FILE__, __LINE__)

int main(void)
{
    printf("=== MCU Malloc Tracker - Phase 3 Self-Test (Fragmentation) ===\n\n");

    /* Initialize tracker */
    printf("[1] Initializing tracker...\n");
    mt_init();

    /* Allocate some blocks */
    printf("[2] Allocating test blocks...\n");
    void* block1 = malloc(64);
    void* block2 = malloc(128);
    void* block3 = malloc(256);
    printf("    Allocated 3 blocks (64 + 128 + 256 = 448 bytes)\n");

    /* Get stats with fragmentation */
    printf("\n[3] Getting heap statistics (with fragmentation analysis)...\n");
    mt_heap_stats_t stats = mt_stats();

    printf("\nCore Statistics:\n");
    printf("  current_used      = %u bytes\n", stats.current_used);
    printf("  peak_used         = %u bytes\n", stats.peak_used);
    printf("  total_allocs      = %u\n", stats.total_allocs);
    printf("  total_frees       = %u\n", stats.total_frees);
    printf("  total_reallocs    = %u\n", stats.total_reallocs);
    printf("  alloc_count       = %u\n", stats.alloc_count);

    printf("\nTable Metrics:\n");
    printf("  table_used        = %u slots\n", stats.table_used);
    printf("  table_tombstones  = %u slots\n", stats.table_tombstones);
    printf("  table_drops       = %u\n", stats.table_drops);

    printf("\nFragmentation (Phase 3):\n");
    printf("  total_free        = %u bytes\n", stats.total_free);
    printf("  largest_free      = %u bytes\n", stats.largest_free);
    printf("  frag_permille     = %u (0..1000 scale)\n", stats.frag_permille);
    printf("  frag_health       = %u ", stats.frag_health);

    /* Print health name */
    switch (stats.frag_health) {
        case 0: printf("(OK)"); break;
        case 1: printf("(WARN)"); break;
        case 2: printf("(CRITICAL)"); break;
        case 3: printf("(N/A)"); break;
        default: printf("(UNKNOWN)"); break;
    }
    printf("\n");

    printf("\nStatus Flags:\n");
    printf("  flags             = 0x%x\n", stats.flags);
    printf("    FRAG_NA         = %d\n", (stats.flags & 1) ? 1 : 0);
    printf("    DROPS           = %d\n", (stats.flags & 2) ? 1 : 0);
    printf("    OVERFLOW        = %d\n", (stats.flags & 4) ? 1 : 0);

    /* Verify critical invariants */
    printf("\n=== Validation ===\n");
    int pass = 1;

    if (stats.total_allocs != 3) {
        printf("❌ FAIL: total_allocs = %u (expected 3)\n", stats.total_allocs);
        pass = 0;
    } else {
        printf("✅ PASS: total_allocs = 3\n");
    }

    if (stats.alloc_count != 3) {
        printf("❌ FAIL: alloc_count = %u (expected 3)\n", stats.alloc_count);
        pass = 0;
    } else {
        printf("✅ PASS: alloc_count = 3\n");
    }

    if (stats.current_used != 448) {
        printf("❌ FAIL: current_used = %u (expected 448)\n", stats.current_used);
        pass = 0;
    } else {
        printf("✅ PASS: current_used = 448\n");
    }

    /* Check fragmentation mode */
#if MT_PLATFORM_HEAP_WALK == 0
    printf("\n[Fragmentation Mode] N/A (MT_PLATFORM_HEAP_WALK=0)\n");
    if (stats.frag_health != 3) {
        printf("❌ FAIL: frag_health = %u (expected 3=NA)\n", stats.frag_health);
        pass = 0;
    } else {
        printf("✅ PASS: frag_health = N/A (3)\n");
    }

    if (!(stats.flags & 1)) {
        printf("❌ FAIL: FRAG_NA flag not set\n");
        pass = 0;
    } else {
        printf("✅ PASS: FRAG_NA flag is set\n");
    }

    if (stats.total_free != 0 || stats.largest_free != 0 || stats.frag_permille != 0) {
        printf("❌ FAIL: Fragmentation metrics should be 0 in N/A mode\n");
        printf("     total_free=%u, largest_free=%u, frag_permille=%u\n",
               stats.total_free, stats.largest_free, stats.frag_permille);
        pass = 0;
    } else {
        printf("✅ PASS: All fragmentation metrics are 0 (N/A)\n");
    }
#else
    printf("\n[Fragmentation Mode] Real (MT_PLATFORM_HEAP_WALK=1)\n");
    if (stats.frag_health == 3) {
        printf("⚠️  NOTE: frag_health = N/A (platform hooks not provided)\n");
    } else {
        printf("✅ Platform heap walk enabled\n");
    }
#endif

    /* Test with free to change metrics */
    printf("\n[4] Freeing block1 and checking stats...\n");
    free(block1);
    stats = mt_stats();

    if (stats.alloc_count != 2) {
        printf("❌ FAIL: alloc_count after free = %u (expected 2)\n", stats.alloc_count);
        pass = 0;
    } else {
        printf("✅ PASS: alloc_count after free = 2\n");
    }

    if (stats.current_used != 384) {
        printf("❌ FAIL: current_used after free = %u (expected 384=128+256)\n", stats.current_used);
        pass = 0;
    } else {
        printf("✅ PASS: current_used = 384\n");
    }

    printf("\n%s\n", pass ? "=== ALL CHECKS PASSED ✅ ===" : "=== SOME CHECKS FAILED ❌ ===");

    /* Cleanup */
    free(block2);
    free(block3);

    return pass ? 0 : 1;
}
