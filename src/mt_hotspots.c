/*
 * MCU Malloc Tracker - Allocation Hotspot Detection (Phase 4)
 *
 * Tracks which call sites (file:line) allocate the most.
 * Uses simple linear search O(n) where n=MT_MAX_HOTSPOTS (typically 64).
 * Drop-new policy when table is full.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "../include/mt_config.h"
#include "../include/mt_types.h"

#if MT_ENABLE_HOTSPOTS

/* ============================================================================
 * HOTSPOT GLOBALS
 * ============================================================================ */

static mt_hotspot_rec_t g_hotspots[MT_MAX_HOTSPOTS];
static uint32_t g_hotspots_used = 0;
static uint32_t g_hotspots_drop = 0;

/* ============================================================================
 * HOTSPOT OPERATIONS (O(n) where n=64, acceptable)
 * ============================================================================ */

/**
 * mt_hotspot_record()
 * Record a malloc event at (file_id, line).
 *
 * Args:
 *   file_id: Hash of __FILE__
 *   line: __LINE__
 *   size: Allocation size in bytes
 *   seq: Current sequence number (from mt__seq_now())
 *
 * O(n) linear search, O(1) if found early.
 */
void mt_hotspot_record(uint32_t file_id, uint16_t line, uint32_t size, uint32_t seq)
{
    /* Search for existing entry */
    for (uint32_t i = 0; i < g_hotspots_used; i++) {
        if (g_hotspots[i].used &&
            g_hotspots[i].file_id == file_id &&
            g_hotspots[i].line == line) {
            /* Found: update stats */
            g_hotspots[i].allocs++;
            g_hotspots[i].bytes += size;
            g_hotspots[i].last_seq = seq;
            return;
        }
    }

    /* Not found: try to insert */
    if (g_hotspots_used < MT_MAX_HOTSPOTS) {
        /* Table has space */
        mt_hotspot_rec_t* rec = &g_hotspots[g_hotspots_used];
        rec->file_id = file_id;
        rec->line = line;
        rec->used = 1;
        rec->allocs = 1;
        rec->bytes = size;
        rec->last_seq = seq;
        g_hotspots_used++;
        return;
    }

    /* Table full: drop-new policy */
    g_hotspots_drop++;
}

/**
 * mt_hotspot_init()
 * Initialize hotspot table.
 */
void mt_hotspot_init(void)
{
    memset(g_hotspots, 0, sizeof(g_hotspots));
    g_hotspots_used = 0;
    g_hotspots_drop = 0;
}

/* ============================================================================
 * HOTSPOT ACCESSORS (for dumps)
 * ============================================================================ */

const mt_hotspot_rec_t* mt_hotspot_table(void)
{
    return g_hotspots;
}

uint32_t mt_hotspot_count(void)
{
    return g_hotspots_used;
}

uint32_t mt_hotspot_drops(void)
{
    return g_hotspots_drop;
}

#endif /* MT_ENABLE_HOTSPOTS */
