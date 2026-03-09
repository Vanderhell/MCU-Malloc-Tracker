/*
 * MCU Malloc Tracker - Core Implementation
 *
 * Phase 2: O(1) hash table malloc/free/realloc tracking
 * - No malloc in tracker (static tables only)
 * - Deterministic (no timestamps, no random)
 * - Recursion guard
 * - Drop-on-full policy
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "../include/mt_config.h"
#include "../include/mt_types.h"
#include "../include/mt_api.h"
#include "../include/mt_internal.h"
#include "../include/mt_hotspots.h"

/* ============================================================================
 * PHASE 2A — GLOBAL STATE (strictly internal)
 * ============================================================================ */

static mt_alloc_rec_t g_allocs[MT_MAX_ALLOCS];
static uint32_t g_seq               = 0;
static uint32_t g_used_count        = 0;
static uint32_t g_tombstone_count   = 0;
static uint32_t g_drop_count        = 0;
static uint8_t  g_inside            = 0;

/* Stats tracking */
static uint32_t g_total_allocs      = 0;
static uint32_t g_total_frees       = 0;
static uint32_t g_total_reallocs    = 0;
static uint32_t g_current_used      = 0;
static uint32_t g_peak_used         = 0;

/* ============================================================================
 * PHASE 2B — POINTER HASH (shift + mix, not FNV1a)
 * ============================================================================ */

/**
 * mt_hash_ptr()
 * Fast, deterministic pointer hash using shift+mix.
 * Works for both 32-bit and 64-bit pointers.
 */
static inline uint32_t mt_hash_ptr(uintptr_t p)
{
    /* Remove alignment bits (typically aligned to 4+ bytes) */
    p >>= 2;

    /* Mix for 64-bit pointers: fold high bits into low bits */
    uint32_t x = (uint32_t)(p ^ (p >> 32));

    /* Cheap 32-bit mixing (2x multiply) */
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;

    return x;
}

/* ============================================================================
 * PHASE 2C — HASH TABLE OPERATIONS (O(1))
 * ============================================================================ */

/**
 * mt_find_slot(ptr, out_idx)
 * Find a USED allocation record by pointer address.
 * Returns 1 if found, 0 if not found.
 * Linear probing stops on EMPTY (not found) or when USED with matching ptr.
 * TOMBSTONE slots are skipped (deleted records).
 */
static int mt_find_slot(void* ptr, uint32_t* out_idx)
{
    uint32_t h = mt_hash_ptr((uintptr_t)ptr);
    uint32_t mask = MT_MAX_ALLOCS - 1;
    uint32_t idx = h & mask;

    for (uint32_t i = 0; i < MT_MAX_ALLOCS; i++) {
        mt_alloc_rec_t* rec = &g_allocs[idx];

        if (rec->state == MT_STATE_EMPTY) {
            /* Not found */
            return 0;
        }

        if (rec->state == MT_STATE_USED && rec->ptr == (uint64_t)(uintptr_t)ptr) {
            /* Found */
            if (out_idx) *out_idx = idx;
            return 1;
        }

        /* TOMBSTONE or different ptr: continue probing */
        idx = (idx + 1) & mask;
    }

    /* Table is full (shouldn't reach here if we enforce drops) */
    return 0;
}

/**
 * mt_find_insert_slot(ptr, out_idx)
 * Find a slot suitable for inserting a new allocation record.
 * Prefers TOMBSTONE slots (reuse deleted slots), then EMPTY.
 * Returns 1 if slot found, 0 if table is full.
 * Linear probing: stop at EMPTY (best case).
 */
static int mt_find_insert_slot(void* ptr, uint32_t* out_idx)
{
    uint32_t h = mt_hash_ptr((uintptr_t)ptr);
    uint32_t mask = MT_MAX_ALLOCS - 1;
    uint32_t idx = h & mask;
    uint32_t tombstone_idx = (uint32_t)-1;

    for (uint32_t i = 0; i < MT_MAX_ALLOCS; i++) {
        mt_alloc_rec_t* rec = &g_allocs[idx];

        if (rec->state == MT_STATE_EMPTY) {
            /* Prefer EMPTY over TOMBSTONE */
            if (out_idx) *out_idx = idx;
            return 1;
        }

        if (rec->state == MT_STATE_TOMBSTONE && tombstone_idx == (uint32_t)-1) {
            /* Remember first TOMBSTONE, but keep looking for EMPTY */
            tombstone_idx = idx;
        }

        idx = (idx + 1) & mask;
    }

    /* No EMPTY found. Use TOMBSTONE if available */
    if (tombstone_idx != (uint32_t)-1) {
        if (out_idx) *out_idx = tombstone_idx;
        return 1;
    }

    /* Table is full */
    return 0;
}

/**
 * mt_slot_free(idx)
 * Mark allocation slot as TOMBSTONE (freed).
 * Decrements used_count, increments tombstone_count.
 */
static void mt_slot_free(uint32_t idx)
{
    mt_alloc_rec_t* rec = &g_allocs[idx];

    if (rec->state == MT_STATE_USED) {
        rec->state = MT_STATE_TOMBSTONE;
        g_used_count--;
        g_tombstone_count++;
    }
}

/* ============================================================================
 * PHASE 2D — RECURSION GUARD + INITIALIZATION
 * ============================================================================ */

void mt_init(void)
{
    /* Initialize all slots to EMPTY */
    for (uint32_t i = 0; i < MT_MAX_ALLOCS; i++) {
        g_allocs[i].state = MT_STATE_EMPTY;
    }

    /* Reset counters */
    g_seq = 1;              /* Start from 1, not 0 (0 reserved for "no allocation") */
    g_used_count = 0;
    g_tombstone_count = 0;
    g_drop_count = 0;
    g_inside = 0;

    /* Reset stats */
    g_total_allocs = 0;
    g_total_frees = 0;
    g_total_reallocs = 0;
    g_current_used = 0;
    g_peak_used = 0;

    /* Initialize hotspots (Phase 4) */
    mt_hotspot_init();
}

/* ============================================================================
 * PHASE 2E — mt_malloc
 * ============================================================================ */

void* mt_malloc(size_t size, const char* file, int line)
{
    /* Recursion guard: if already inside tracker, call real malloc */
    if (g_inside != 0) {
        return MT_REAL_MALLOC(size);
    }

    /* Allocate with real malloc */
    void* ptr = MT_REAL_MALLOC(size);
    if (ptr == NULL) {
        return NULL;
    }

    /* Enter critical section */
    g_inside = 1;
    MT_LOCK();

    /* Compute file_id for tracking (used in both drop and insert cases) */
    uint32_t file_id = 0;
    uint16_t line_num = (uint16_t)line;

#if MT_CAPTURE_CALLSITE
#if MT_FILE_ID_MODE == 1
    /* Hash filename (FNV1a-32) */
    uint32_t hash = 2166136261u;    /* FNV1a offset basis */
    for (const char* c = file; *c; c++) {
        hash ^= (uint8_t)*c;
        hash *= 16777619u;          /* FNV1a prime */
    }
    file_id = hash;
#else
    /* Store pointer to filename string */
    file_id = (uint32_t)(uintptr_t)file;
#endif
#endif

    /* Try to find insert slot */
    uint32_t idx;
    uint32_t current_seq = g_seq;

    if (!mt_find_insert_slot(ptr, &idx)) {
        /* Table full: drop tracking (but keep allocation and record hotspot) */
        g_drop_count++;

        /* Record hotspot even though alloc table dropd (Phase 4) */
        mt_hotspot_record(file_id, line_num, (uint32_t)size, current_seq);

        MT_UNLOCK();
        g_inside = 0;
        return ptr;
    }

    /* Record allocation */
    mt_alloc_rec_t* rec = &g_allocs[idx];
    rec->ptr = (uint64_t)(uintptr_t)ptr;
    rec->size = (uint32_t)size;      /* Clamp to uint32_t */
    rec->file_id = file_id;
    rec->line = line_num;
    rec->seq = current_seq;
    rec->state = MT_STATE_USED;

    /* Update counters */
    g_used_count++;
    g_total_allocs++;
    g_seq++;  /* Increment sequence after use */
    g_current_used += (uint32_t)size;
    if (g_current_used > g_peak_used) {
        g_peak_used = g_current_used;
    }

    /* Record hotspot (Phase 4) */
    mt_hotspot_record(file_id, line_num, (uint32_t)size, current_seq);

    MT_UNLOCK();
    g_inside = 0;

    return ptr;
}

/* ============================================================================
 * PHASE 2F — mt_free
 * ============================================================================ */

void mt_free(void* ptr, const char* file, int line)
{
    (void)file;    /* Unused in free, but keep signature consistent */
    (void)line;

    if (ptr == NULL) {
        /* Standard C: free(NULL) is no-op */
        return;
    }

    /* Recursion guard */
    if (g_inside != 0) {
        MT_REAL_FREE(ptr);
        return;
    }

    /* Enter critical section */
    g_inside = 1;
    MT_LOCK();

    /* Find and mark as TOMBSTONE */
    uint32_t idx;
    if (mt_find_slot(ptr, &idx)) {
        mt_alloc_rec_t* rec = &g_allocs[idx];
        g_current_used -= rec->size;    /* Update current usage */
        g_total_frees++;

        mt_slot_free(idx);              /* Mark as TOMBSTONE */
    }
    /* If not found: silently skip (free unknown ptr doesn't crash) */

    MT_UNLOCK();
    g_inside = 0;

    /* Free with real allocator */
    MT_REAL_FREE(ptr);
}

/* ============================================================================
 * PHASE 2G — mt_realloc
 * ============================================================================ */

void* mt_realloc(void* ptr, size_t size, const char* file, int line)
{
    /* Recursion guard */
    if (g_inside != 0) {
        return MT_REAL_REALLOC(ptr, size);
    }

    /* Case 1: ptr == NULL → malloc */
    if (ptr == NULL) {
        return mt_malloc(size, file, line);
    }

    /* Case 2: size == 0 → free + return NULL */
    if (size == 0) {
        mt_free(ptr, file, line);
        return NULL;
    }

    /* Case 3: normal realloc */
    void* new_ptr = MT_REAL_REALLOC(ptr, size);
    if (new_ptr == NULL) {
        /* Realloc failed: original allocation remains unchanged */
        return NULL;
    }

    /* Track realloc */
    g_inside = 1;
    MT_LOCK();

    uint32_t idx;
    if (mt_find_slot(ptr, &idx)) {
        mt_alloc_rec_t* rec = &g_allocs[idx];
        uint32_t old_size = rec->size;

        /* Update size and statistics */
        rec->size = (uint32_t)size;
        rec->ptr = (uint64_t)(uintptr_t)new_ptr;
        rec->seq = g_seq++;             /* New seq for realloc */

        /* Update current usage (can go up or down) */
        if (size > old_size) {
            g_current_used += (uint32_t)(size - old_size);
        } else {
            g_current_used -= (uint32_t)(old_size - size);
        }

        if (g_current_used > g_peak_used) {
            g_peak_used = g_current_used;
        }

        g_total_reallocs++;
    }
    /* If old ptr not found: we have a new pointer in the system
     * (shouldn't happen in well-behaved code, but we don't crash) */

    MT_UNLOCK();
    g_inside = 0;

    return new_ptr;
}

/* ============================================================================
 * INTERNAL ACCESSORS (mt_internal.h)
 * ============================================================================ */

const mt_alloc_rec_t* mt__alloc_table(void)
{
    return g_allocs;
}

uint32_t mt__alloc_table_cap(void)
{
    return MT_MAX_ALLOCS;
}

uint32_t mt__used_count(void)
{
    return g_used_count;
}

uint32_t mt__tombstone_count(void)
{
    return g_tombstone_count;
}

uint32_t mt__drop_count(void)
{
    return g_drop_count;
}

uint32_t mt__seq_now(void)
{
    return g_seq;
}

/**
 * mt__stats_core()
 * Return core statistics (O(1) cached values).
 */
mt_stats_core_t mt__stats_core(void)
{
    mt_stats_core_t stats = {0};
    MT_LOCK();
    stats.current_used = g_current_used;
    stats.peak_used = g_peak_used;
    stats.total_allocs = g_total_allocs;
    stats.total_frees = g_total_frees;
    stats.total_reallocs = g_total_reallocs;
    stats.alloc_count = g_used_count;
    MT_UNLOCK();
    return stats;
}
