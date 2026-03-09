/*
 * MCU Malloc Tracker - Heap Statistics
 *
 * Phase 3: Extended stats with fragmentation
 * - Cheap O(1) stats (cached from core)
 * - Fragmentation analysis (2 modes: real or N/A)
 * - Status flags
 */

#include <stddef.h>
#include <stdint.h>
#include "../include/mt_config.h"
#include "../include/mt_types.h"
#include "../include/mt_api.h"
#include "../include/mt_internal.h"

/* Forward declarations for internal functions from mt_fragmentation.c */
extern uint32_t mt_get_fragmentation(uint32_t* out_total_free, uint32_t* out_largest_free);
extern int mt_frag_health_from_packed(uint32_t packed);  /* Returns enum as int */
extern uint16_t mt_frag_permille_from_packed(uint32_t packed);
extern int mt_frag_available_from_packed(uint32_t packed);

/* ============================================================================
 * EXTENDED mt_stats() with Fragmentation
 * ============================================================================ */

/**
 * mt_stats()
 * Get current heap statistics.
 *
 * O(1) operation: returns cached counters from core + computed fragmentation.
 * No heap walk iteration (that's O(n) and happens only in dumps).
 */
mt_heap_stats_t mt_stats(void)
{
    mt_heap_stats_t stats = {0};

    /* Get core stats (O(1) cached values) */
    mt_stats_core_t core = mt__stats_core();
    stats.current_used = core.current_used;
    stats.peak_used = core.peak_used;
    stats.total_allocs = core.total_allocs;
    stats.total_frees = core.total_frees;
    stats.total_reallocs = core.total_reallocs;
    stats.alloc_count = core.alloc_count;

    /* Get core table metrics */
    stats.table_used = mt__used_count();
    stats.table_tombstones = mt__tombstone_count();
    stats.table_drops = mt__drop_count();

    /* Get fragmentation (may be real or N/A depending on platform) */
    uint32_t total_free = 0;
    uint32_t largest_free = 0;
    uint32_t frag_packed = mt_get_fragmentation(&total_free, &largest_free);

    stats.total_free = total_free;
    stats.largest_free = largest_free;
    stats.frag_permille = mt_frag_permille_from_packed(frag_packed);
    stats.frag_health = (uint8_t)mt_frag_health_from_packed(frag_packed);

    /* Set flags */
    stats.flags = 0;

    if (!mt_frag_available_from_packed(frag_packed)) {
        stats.flags |= MT_STAT_FLAG_FRAG_NA;
    }

    if (stats.table_drops > 0) {
        stats.flags |= MT_STAT_FLAG_DROPS;
    }

    return stats;
}
