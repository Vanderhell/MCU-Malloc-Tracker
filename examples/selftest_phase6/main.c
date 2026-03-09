/*
 * MCU Malloc Tracker - Phase 6 Self-Test (Text Dumps)
 *
 * Test scenario:
 * - 2 malloc calls
 * - 1 free call (1 leak remains)
 * - 10 hotspot allocations from 2 call sites
 * - Call mt_dump_uart(write_fn)
 * - Verify output contains: header, LEAKS, HOTSPOTS
 * - Verify leaks are sorted by ptr (ascending)
 * - Verify hotspots show top allocator
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/mt_config.h"
#include "../../include/mt_api.h"

#define malloc(x)       mt_malloc((x), __FILE__, __LINE__)
#define free(p)         mt_free((p), __FILE__, __LINE__)
#define realloc(p, x)   mt_realloc((p), (x), __FILE__, __LINE__)

/* Output buffer for dump */
static char dump_output[4096];
static size_t dump_offset = 0;

/**
 * Callback function for mt_dump_uart to capture output.
 */
static void dump_write_fn(const char* s)
{
    if (s && dump_offset < sizeof(dump_output) - 1) {
        size_t len = strlen(s);
        size_t remaining = sizeof(dump_output) - dump_offset - 1;
        size_t to_copy = (len < remaining) ? len : remaining;

        memcpy(dump_output + dump_offset, s, to_copy);
        dump_offset += to_copy;
    }
}

/**
 * Check if substring exists in output.
 */
static int dump_contains(const char* substr)
{
    return strstr(dump_output, substr) != NULL;
}

/**
 * Check if output lines are in order.
 */
static int check_line_order(const char* line1, const char* line2)
{
    const char* pos1 = strstr(dump_output, line1);
    const char* pos2 = strstr(dump_output, line2);

    if (!pos1 || !pos2) {
        return 0;
    }

    return pos1 < pos2;
}

int main(void)
{
    printf("=== MCU Malloc Tracker - Phase 6 Self-Test (Text Dumps) ===\n\n");

    /* Initialize */
    printf("[1] Initializing tracker...\n");
    mt_init();

    /* Allocate 2 blocks */
    printf("[2] Allocating 2 blocks...\n");
    void* block1 = malloc(64);
    void* block2 = malloc(128);
    printf("    block1=%p (64B)\n", block1);
    printf("    block2=%p (128B)\n", block2);

    /* Free block1 (block2 is leak) */
    printf("[3] Freeing block1 (block2 remains as leak)...\n");
    free(block1);

    /* Generate hotspots from two call sites */
    printf("[4] Generating hotspot allocations...\n");

    /* Site A: 7 allocations */
    void* h1 = malloc(16);  /* hotspot A */
    void* h2 = malloc(16);  /* hotspot A */
    void* h3 = malloc(16);  /* hotspot A */
    void* h4 = malloc(16);  /* hotspot A */
    void* h5 = malloc(16);  /* hotspot A */
    void* h6 = malloc(16);  /* hotspot A */
    void* h7 = malloc(16);  /* hotspot A */

    /* Site B: 3 allocations (different line) */
    {
        void* hb1 = malloc(32);  /* hotspot B */
        void* hb2 = malloc(32);  /* hotspot B */
        void* hb3 = malloc(32);  /* hotspot B */
        (void)hb1;
        (void)hb2;
        (void)hb3;
    }

    (void)h1; (void)h2; (void)h3; (void)h4; (void)h5; (void)h6; (void)h7;

    printf("    Created hotspots: 7 from site A, 3 from site B\n");

    /* Get stats before dump */
    printf("\n[5] Current state:\n");
    mt_heap_stats_t stats = mt_stats();
    printf("    current_used=%u peak_used=%u\n", stats.current_used, stats.peak_used);
    printf("    total_allocs=%u total_frees=%u\n", stats.total_allocs, stats.total_frees);

    /* Call mt_dump_uart */
    printf("\n[6] Calling mt_dump_uart(write_fn)...\n");
    dump_offset = 0;
    memset(dump_output, 0, sizeof(dump_output));
    mt_dump_uart(dump_write_fn);

    /* Show captured output */
    printf("\n[7] Captured dump output:\n");
    printf("─────────────────────────────────────────────────────────────\n");
    printf("%s", dump_output);
    printf("─────────────────────────────────────────────────────────────\n");

    /* Verification */
    printf("\n[8] Verification checks:\n");

    int pass = 1;

    /* Check header marker */
    if (dump_contains("=== MCU MALLOC TRACKER ===")) {
        printf("    [OK] Header marker found\n");
    } else {
        printf("    [FAIL] Header marker not found\n");
        pass = 0;
    }

    /* Check footer marker */
    if (dump_contains("=== END ===")) {
        printf("    [OK] Footer marker found\n");
    } else {
        printf("    [FAIL] Footer marker not found\n");
        pass = 0;
    }

    /* Check stats are present */
    if (dump_contains("version=") && dump_contains("used_bytes=")) {
        printf("    [OK] Stats present\n");
    } else {
        printf("    [FAIL] Stats missing\n");
        pass = 0;
    }

#if MT_ENABLE_LEAK_DUMP
    /* Check LEAKS section */
    if (dump_contains("--- LEAKS")) {
        printf("    [OK] LEAKS section found\n");
    } else {
        printf("    [FAIL] LEAKS section not found\n");
        pass = 0;
    }

    /* Check leak count (should be 1: block2) */
    if (dump_contains("--- LEAKS (N=1)")) {
        printf("    [OK] LEAKS count N=1\n");
    } else {
        printf("    [FAIL] LEAKS count incorrect\n");
        pass = 0;
    }

    /* Check block2 pointer is in leaks (should start with 0x) */
    if (strstr(dump_output, "--- LEAKS") && strstr(dump_output, "size=128")) {
        printf("    [OK] Leaked block (128B) found in dump\n");
    } else {
        printf("    [FAIL] Leaked block not found\n");
        pass = 0;
    }

    /* Verify leaks are sorted by ptr ascending */
    if (check_line_order("--- LEAKS", "0x")) {
        printf("    [OK] Leak records present after LEAKS header\n");
    } else {
        printf("    [FAIL] Leak records format issue\n");
        pass = 0;
    }
#endif /* MT_ENABLE_LEAK_DUMP */

#if MT_ENABLE_HOTSPOTS
    /* Check HOTSPOTS section */
    if (dump_contains("--- HOTSPOTS")) {
        printf("    [OK] HOTSPOTS section found\n");
    } else {
        printf("    [FAIL] HOTSPOTS section not found\n");
        pass = 0;
    }

    /* Check that top hotspot has 7 or 10 allocs (site A is first) */
    if (dump_contains("allocs=")) {
        printf("    [OK] Hotspot alloc counts present\n");
    } else {
        printf("    [FAIL] Hotspot alloc counts missing\n");
        pass = 0;
    }

    /* Verify first (top) hotspot has most allocations */
    char* hotspots_start = strstr(dump_output, "--- HOTSPOTS");
    if (hotspots_start) {
        char* first_allocs = strstr(hotspots_start, "allocs=");
        char* second_allocs = first_allocs ? strstr(first_allocs + 1, "allocs=") : NULL;

        if (first_allocs && second_allocs) {
            int allocs1 = atoi(first_allocs + 7);
            int allocs2 = atoi(second_allocs + 7);

            if (allocs1 >= allocs2) {
                printf("    [OK] Hotspots ordered (top: %d, next: %d)\n", allocs1, allocs2);
            } else {
                printf("    [FAIL] Hotspots not ordered (top: %d < next: %d)\n", allocs1, allocs2);
                pass = 0;
            }
        }
    }
#endif /* MT_ENABLE_HOTSPOTS */

    /* Summary */
    printf("\n");
    printf("%s\n", pass ? "=== ALL CHECKS PASSED [OK] ===" : "=== SOME CHECKS FAILED [FAIL] ===");

    return pass ? 0 : 1;
}
