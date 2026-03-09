/*
 * MCU Malloc Tracker - Phase 4 Self-Test (Hotspots)
 *
 * Test scenario:
 * - Allocate from hotspot A (10 allocations)
 * - Allocate from hotspot B (5 allocations)
 * - Dump hotspots with deterministic ordering
 * - Verify: hotspots detected, ordered correctly (allocs DESC)
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

/* Buffer to collect dump output */
static char dump_buffer[2048];
static size_t dump_offset = 0;

static void collect_output(const char* s)
{
    if (!s) return;
    while (*s && dump_offset < sizeof(dump_buffer) - 1) {
        dump_buffer[dump_offset++] = *s++;
    }
    dump_buffer[dump_offset] = '\0';
}

/* Helper to allocate from a specific "source" */
static void* alloc_from_a(size_t size)
{
    return malloc(size);  /* Line: different from alloc_from_b */
}

static void* alloc_from_b(size_t size)
{
    /* Adding some no-op lines to make this visibly different in code */
    ;
    ;
    return malloc(size);  /* Line: different from alloc_from_a */
}

int main(void)
{
    printf("=== MCU Malloc Tracker - Phase 4 Self-Test (Hotspots) ===\n\n");

    /* Initialize tracker */
    printf("[1] Initializing tracker...\n");
    mt_init();

    /* Allocate from hotspot A (10x from same line) */
    printf("[2] Allocating 10x from hotspot A...\n");
    void* ptrs_a[10];
    for (int i = 0; i < 10; i++) {
        ptrs_a[i] = alloc_from_a(32);
    }
    printf("    Allocated 10 × 32 bytes from alloc_from_a()\n");

    /* Allocate from hotspot B (5x from same line) */
    printf("[3] Allocating 5x from hotspot B...\n");
    void* ptrs_b[5];
    for (int i = 0; i < 5; i++) {
        ptrs_b[i] = alloc_from_b(64);
    }
    printf("    Allocated 5 × 64 bytes from alloc_from_b()\n");

    /* Get stats to show basic info */
    printf("\n[4] Basic stats:\n");
    mt_heap_stats_t stats = mt_stats();
    printf("    current_used = %u bytes\n", stats.current_used);
    printf("    alloc_count = %u\n", stats.alloc_count);

    /* Dump hotspots */
    printf("\n[5] Dumping hotspots:\n");
    dump_offset = 0;
    memset(dump_buffer, 0, sizeof(dump_buffer));
    mt_dump_hotspots(collect_output);

    printf("%s", dump_buffer);

    /* Verify hotspot count */
    printf("\n[6] Verification:\n");

#if MT_ENABLE_HOTSPOTS
    printf("✅ Hotspots enabled (MT_ENABLE_HOTSPOTS=1)\n");

    /* Count hotspots in dump output */
    int hotspot_count = 0;
    int first_allocs = 0, second_allocs = 0;

    const char* ptr = dump_buffer;
    while ((ptr = strstr(ptr, "allocs=")) != NULL) {
        int allocs_val = 0;
        sscanf(ptr, "allocs=%d", &allocs_val);

        if (hotspot_count == 0) {
            first_allocs = allocs_val;
        } else if (hotspot_count == 1) {
            second_allocs = allocs_val;
        }
        hotspot_count++;
        ptr++;  /* Move past this match */
    }

    printf("Found %d hotspots in dump\n", hotspot_count);

    if (hotspot_count >= 2) {
        printf("✅ PASS: Found at least 2 hotspots\n");
    } else {
        printf("❌ FAIL: Expected >= 2 hotspots, got %d\n", hotspot_count);
    }

    if (first_allocs == 10) {
        printf("✅ PASS: First hotspot has 10 allocs\n");
    } else {
        printf("❌ FAIL: First hotspot expected 10, got %d\n", first_allocs);
    }

    if (second_allocs == 5) {
        printf("✅ PASS: Second hotspot has 5 allocs\n");
    } else {
        printf("❌ FAIL: Second hotspot expected 5, got %d\n", second_allocs);
    }

    if (first_allocs >= second_allocs) {
        printf("✅ PASS: Hotspots ordered by allocs DESC (10 >= 5)\n");
    } else {
        printf("❌ FAIL: Hotspots not ordered correctly\n");
    }
#else
    printf("⚠️  Hotspots disabled (MT_ENABLE_HOTSPOTS=0)\n");
#endif

    /* Free allocations */
    printf("\n[7] Cleaning up...\n");
    for (int i = 0; i < 10; i++) free(ptrs_a[i]);
    for (int i = 0; i < 5; i++) free(ptrs_b[i]);

    printf("\n=== TEST COMPLETE ===\n");

    return 0;
}
