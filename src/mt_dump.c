/*
 * MCU Malloc Tracker - Text Dumps (Phase 6)
 * Output: UART/text format for stats, leaks, hotspots
 * No malloc, deterministic, small stack buffers
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../include/mt_config.h"
#include "../include/mt_types.h"
#include "../include/mt_api.h"
#include "../include/mt_internal.h"
#include "../include/mt_hotspots.h"

/* ============================================================================
 * PHASE 6A — UTILITY FORMATTERS
 * ============================================================================ */

/**
 * mt_write_line(write_fn, line)
 * Safely output a line using write_fn callback.
 */
static void mt_write_line(void (*write_fn)(const char* s), const char* line)
{
    if (write_fn && line) {
        write_fn(line);
    }
}

/**
 * mt_write_u32(write_fn, key, value)
 * Format and output a key=value pair (uint32_t).
 */
static void mt_write_u32(void (*write_fn)(const char* s),
                         const char* key, uint32_t value)
{
    if (!write_fn || !key) {
        return;
    }

    char buf[96];
    snprintf(buf, sizeof(buf), "%s=%u\n", key, value);
    write_fn(buf);
}

/**
 * mt_write_hex32(write_fn, key, value)
 * Format and output a key=0xVALUE pair (uint32_t hex).
 */
static void mt_write_hex32(void (*write_fn)(const char* s),
                           const char* key, uint32_t value)
{
    if (!write_fn || !key) {
        return;
    }

    char buf[96];
    snprintf(buf, sizeof(buf), "%s=0x%08x\n", key, value);
    write_fn(buf);
}

/**
 * mt_write_hex64(write_fn, key, value)
 * Format and output a key=0xVALUE pair (uint64_t hex).
 */
static void mt_write_hex64(void (*write_fn)(const char* s),
                           const char* key, uint64_t value)
{
    if (!write_fn || !key) {
        return;
    }

    char buf[96];
    snprintf(buf, sizeof(buf), "%s=0x%016llx\n", key, (unsigned long long)value);
    write_fn(buf);
}

/* ============================================================================
 * PHASE 6B — mt_dump_uart() (MAIN ENTRYPOINT)
 * ============================================================================ */

void mt_dump_uart(void (*write_fn)(const char* s))
{
    if (!write_fn) {
        return;
    }

    char buf[128];

    /* Header */
    mt_write_line(write_fn, "=== MCU MALLOC TRACKER ===\n");

    /* Version */
    mt_write_u32(write_fn, "version", 1);

    /* Stats */
    mt_heap_stats_t stats = mt_stats();

    mt_write_u32(write_fn, "used_bytes", stats.current_used);
    mt_write_u32(write_fn, "peak_bytes", stats.peak_used);

    /* Counters on one line */
    snprintf(buf, sizeof(buf), "allocs=%u frees=%u reallocs=%u\n",
             stats.total_allocs, stats.total_frees, stats.total_reallocs);
    write_fn(buf);

    /* Active allocations */
    uint32_t active = stats.total_allocs - stats.total_frees;
    mt_write_u32(write_fn, "active_allocs", active);

    /* Table usage */
    uint32_t used_count = mt__used_count();
    uint32_t tombstone_count = mt__tombstone_count();
    uint32_t drops = mt__drop_count();

    snprintf(buf, sizeof(buf), "table_used=%u tombstones=%u drops=%u\n",
             used_count, tombstone_count, drops);
    write_fn(buf);

    /* Fragmentation */
    const char* frag_mode = (stats.flags & MT_STAT_FLAG_FRAG_NA) ? "NA" : "REAL";
    snprintf(buf, sizeof(buf), "frag_mode=%s\n", frag_mode);
    write_fn(buf);

    /* Fragmentation metrics */
    if (stats.flags & MT_STAT_FLAG_FRAG_NA) {
        mt_write_line(write_fn, "frag_permille=NA health=NA\n");
    } else {
        const char* health_str = "N/A";
        if ((stats.frag_health & 0xFF) == 0) {
            health_str = "OK";
        } else if ((stats.frag_health & 0xFF) == 1) {
            health_str = "WARN";
        } else if ((stats.frag_health & 0xFF) == 2) {
            health_str = "CRITICAL";
        }

        snprintf(buf, sizeof(buf), "frag_permille=%u health=%s\n",
                 stats.frag_permille, health_str);
        write_fn(buf);
    }

    /* Flags */
    mt_write_hex32(write_fn, "flags", stats.flags);

    /* ========== LEAKS (if enabled) ========== */
#if MT_ENABLE_LEAK_DUMP
    if (active > 0) {
        snprintf(buf, sizeof(buf), "--- LEAKS (N=%u)\n", active);
        write_fn(buf);
        mt_dump_leaks(write_fn);
    } else {
        snprintf(buf, sizeof(buf), "--- LEAKS (N=%u)\n", active);
        write_fn(buf);
    }
#endif /* MT_ENABLE_LEAK_DUMP */

    /* ========== HOTSPOTS (if enabled) ========== */
#if MT_ENABLE_HOTSPOTS
    mt_write_line(write_fn, "--- HOTSPOTS (top 10)\n");
    mt_dump_hotspots(write_fn);
#endif /* MT_ENABLE_HOTSPOTS */

    /* Footer */
    mt_write_line(write_fn, "=== END ===\n");
}

/* ============================================================================
 * PHASE 6C — mt_dump_leaks() (DETERMINISTIC LEAK DUMP)
 * ============================================================================ */

#if MT_ENABLE_LEAK_DUMP

/**
 * Sort allocation indices by ptr (ascending).
 * O(n²) bubble sort — acceptable for small n.
 */
static void mt_alloc_sort_indices_by_ptr(uint16_t* indices, uint32_t count,
                                         const mt_alloc_rec_t* allocs)
{
    for (uint32_t i = 0; i < count; i++) {
        for (uint32_t j = i + 1; j < count; j++) {
            uint64_t ptr_i = allocs[indices[i]].ptr;
            uint64_t ptr_j = allocs[indices[j]].ptr;

            if (ptr_j < ptr_i) {
                /* Swap */
                uint16_t tmp = indices[i];
                indices[i] = indices[j];
                indices[j] = tmp;
            }
        }
    }
}

void mt_dump_leaks(void (*write_fn)(const char* s))
{
    if (!write_fn) {
        return;
    }

    const mt_alloc_rec_t* allocs = mt__alloc_table();
    uint32_t table_cap = mt__alloc_table_cap();

    /* Collect USED allocation indices */
    uint16_t indices[MT_MAX_ALLOCS];
    uint32_t used_count = 0;

    for (uint32_t i = 0; i < table_cap; i++) {
        if (allocs[i].state == MT_STATE_USED) {
            indices[used_count++] = (uint16_t)i;
        }
    }

    if (used_count == 0) {
        mt_write_line(write_fn, "(no active allocations)\n");
        return;
    }

    /* Sort by ptr (ascending) */
    mt_alloc_sort_indices_by_ptr(indices, used_count, allocs);

    /* Dump each leak */
    char buf[128];
    for (uint32_t i = 0; i < used_count; i++) {
        const mt_alloc_rec_t* rec = &allocs[indices[i]];

        snprintf(buf, sizeof(buf),
                 "0x%016llx size=%u file=0x%08x line=%u seq=%u\n",
                 (unsigned long long)rec->ptr,
                 rec->size,
                 rec->file_id,
                 rec->line,
                 rec->seq);
        write_fn(buf);
    }
}

#endif /* MT_ENABLE_LEAK_DUMP */

/* ============================================================================
 * PHASE 6D+ — mt_dump_hotspots() (UNIFIED STYLE)
 * ============================================================================ */

#if MT_ENABLE_HOTSPOTS

/**
 * Sort hotspot indices deterministically.
 * Criteria: allocs DESC, bytes DESC, file_id ASC, line ASC
 */
static void mt_hotspot_sort_indices(uint16_t* indices, uint32_t count,
                                      const mt_hotspot_rec_t* hotspots)
{
    for (uint32_t i = 0; i < count; i++) {
        for (uint32_t j = i + 1; j < count; j++) {
            const mt_hotspot_rec_t* rec_i = &hotspots[indices[i]];
            const mt_hotspot_rec_t* rec_j = &hotspots[indices[j]];

            int cmp = 0;

            /* Sort by: allocs DESC */
            if (rec_i->allocs != rec_j->allocs) {
                cmp = (rec_i->allocs > rec_j->allocs) ? -1 : 1;
            }
            /* Then: bytes DESC */
            else if (rec_i->bytes != rec_j->bytes) {
                cmp = (rec_i->bytes > rec_j->bytes) ? -1 : 1;
            }
            /* Then: file_id ASC */
            else if (rec_i->file_id != rec_j->file_id) {
                cmp = (rec_i->file_id < rec_j->file_id) ? -1 : 1;
            }
            /* Then: line ASC */
            else if (rec_i->line != rec_j->line) {
                cmp = (rec_i->line < rec_j->line) ? -1 : 1;
            }

            /* Swap if needed */
            if (cmp > 0) {
                uint16_t tmp = indices[i];
                indices[i] = indices[j];
                indices[j] = tmp;
            }
        }
    }
}

void mt_dump_hotspots(void (*write_fn)(const char* s))
{
    if (!write_fn) {
        return;
    }

    uint32_t count = mt_hotspot_count();
    if (count == 0) {
        mt_write_line(write_fn, "(no hotspots)\n");
        return;
    }

    /* Collect indices of used hotspots */
    uint16_t indices[MT_MAX_HOTSPOTS];
    uint32_t used_count = 0;
    const mt_hotspot_rec_t* table = mt_hotspot_table();

    for (uint32_t i = 0; i < count; i++) {
        if (table[i].used) {
            indices[used_count++] = (uint16_t)i;
        }
    }

    if (used_count == 0) {
        mt_write_line(write_fn, "(no active hotspots)\n");
        return;
    }

    /* Sort indices deterministically */
    mt_hotspot_sort_indices(indices, used_count, table);

    /* Dump top 10 (or all if fewer) */
    uint32_t limit = (used_count > 10) ? 10 : used_count;
    char buf[128];

    for (uint32_t i = 0; i < limit; i++) {
        const mt_hotspot_rec_t* rec = &table[indices[i]];

        snprintf(buf, sizeof(buf),
                 "file=0x%08x line=%u allocs=%u bytes=%u last_seq=%u\n",
                 rec->file_id, rec->line, rec->allocs, rec->bytes, rec->last_seq);
        write_fn(buf);
    }

    /* Report drops if any */
    uint32_t drops = mt_hotspot_drops();
    if (drops > 0) {
        snprintf(buf, sizeof(buf), "hotspot_drops=%u\n", drops);
        write_fn(buf);
    }
}

#endif /* MT_ENABLE_HOTSPOTS */

/* ============================================================================
 * PHASE 6E — mt_telemetry_tick() (OPTIONAL PERIODIC REPORTING)
 * ============================================================================ */

#if MT_ENABLE_TELEMETRY

static uint32_t g_telemetry_last_ticks = 0;

void mt_telemetry_tick(uint32_t now_ticks, void (*write_fn)(const char* s))
{
    if (!write_fn) {
        return;
    }

    /* Check if enough time has passed */
    uint32_t delta = now_ticks - g_telemetry_last_ticks;
    if (delta < MT_TELEMETRY_PERIOD_TICKS) {
        return;
    }

    g_telemetry_last_ticks = now_ticks;

    /* Get stats */
    mt_heap_stats_t stats = mt_stats();

    /* Format telemetry line */
    char buf[128];
    uint32_t active = stats.total_allocs - stats.total_frees;
    uint32_t drops = mt__drop_count();

    const char* frag_str = "NA";
    char frag_buf[16];
    if (!(stats.flags & MT_STAT_FLAG_FRAG_NA)) {
        snprintf(frag_buf, sizeof(frag_buf), "%uPM", stats.frag_permille);
        frag_str = frag_buf;
    }

    snprintf(buf, sizeof(buf),
             "MT: used=%u peak=%u active=%u drops=%u frag=%s\n",
             stats.current_used, stats.peak_used, active, drops, frag_str);
    write_fn(buf);
}

#endif /* MT_ENABLE_TELEMETRY */
