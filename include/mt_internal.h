#ifndef MT_INTERNAL_H
#define MT_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include "mt_types.h"

/*
 * MCU Malloc Tracker - Internal Accessors
 *
 * This header is STRICTLY INTERNAL (mt__ prefix).
 * Not part of public API.
 * Used by mt_heap_stats.c, mt_fragmentation.c, mt_snapshot.c, mt_dump.c
 * to access core tracker state without iteration.
 */

/* ============================================================================
 * INTERNAL TYPE DEFINITIONS
 * ============================================================================ */

typedef struct {
    uint32_t current_used;
    uint32_t peak_used;
    uint32_t total_allocs;
    uint32_t total_frees;
    uint32_t total_reallocs;
    uint32_t alloc_count;
} mt_stats_core_t;

/* ============================================================================
 * CORE STATE ACCESSORS (read-only)
 * ============================================================================ */

/**
 * mt__alloc_table()
 * Get pointer to allocation table.
 * Use only for iteration in dumps/snapshots (O(n) is acceptable there).
 * Do NOT call from mt_stats() (must be O(1)).
 */
const mt_alloc_rec_t* mt__alloc_table(void);

/**
 * mt__alloc_table_cap()
 * Get allocation table capacity (MT_MAX_ALLOCS).
 */
uint32_t mt__alloc_table_cap(void);

/**
 * mt__used_count()
 * Get number of USED slots (active allocations).
 * O(1) - cached counter.
 */
uint32_t mt__used_count(void);

/**
 * mt__tombstone_count()
 * Get number of TOMBSTONE slots (freed records).
 * O(1) - cached counter.
 */
uint32_t mt__tombstone_count(void);

/**
 * mt__drop_count()
 * Get number of dropped allocations (table was full).
 * O(1) - cached counter.
 */
uint32_t mt__drop_count(void);

/**
 * mt__seq_now()
 * Get current sequence counter value.
 * O(1).
 */
uint32_t mt__seq_now(void);

/**
 * mt__stats_core()
 * Get core statistics already computed and cached.
 * This is O(1) read of counters.
 * Called by mt_stats() in mt_heap_stats.c.
 */
mt_stats_core_t mt__stats_core(void);

#endif /* MT_INTERNAL_H */
