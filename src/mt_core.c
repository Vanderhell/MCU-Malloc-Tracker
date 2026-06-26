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
static uint8_t  g_initialized       = 0;

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
#if UINTPTR_MAX > 0xffffffffu
    uint64_t x = (uint64_t)p >> 3;
    uint32_t h = (uint32_t)(x ^ (x >> 32));
#else
    uint32_t h = (uint32_t)p >> 2;
#endif

    h ^= h >> 16;
    h *= 0x7feb352dU;
    h ^= h >> 15;
    h *= 0x846ca68bU;
    h ^= h >> 16;
    return h;
}

static inline uint32_t mt_clamp_size_u32(size_t size)
{
    if (size > UINT32_MAX) {
        return UINT32_MAX;
    }
    return (uint32_t)size;
}

static inline void mt_add_current_used(uint32_t delta)
{
    if (UINT32_MAX - g_current_used < delta) {
        g_current_used = UINT32_MAX;
    } else {
        g_current_used += delta;
    }

    if (g_current_used > g_peak_used) {
        g_peak_used = g_current_used;
    }
}

static inline void mt_sub_current_used(uint32_t delta)
{
    if (g_current_used < delta) {
        g_current_used = 0;
    } else {
        g_current_used -= delta;
    }
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
static int mt_find_slot(const void* ptr, uint32_t* out_idx)
{
    uint32_t h = mt_hash_ptr((uintptr_t)ptr);
    uint32_t mask = MT_MAX_ALLOCS - 1;
    uint32_t idx = h & mask;

    for (uint32_t i = 0; i < MT_MAX_ALLOCS; i++) {
        const mt_alloc_rec_t* rec = &g_allocs[idx];

        if (rec->state == MT_STATE_EMPTY) {
            /* Not found */
            return 0;
        }

        if (rec->state == MT_STATE_USED && rec->ptr == (uintptr_t)ptr) {
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
static int mt_find_insert_slot(const void* ptr, uint32_t* out_idx)
{
    uint32_t h = mt_hash_ptr((uintptr_t)ptr);
    uint32_t mask = MT_MAX_ALLOCS - 1;
    uint32_t idx = h & mask;
    uint32_t tombstone_idx = MT_MAX_ALLOCS;

    for (uint32_t i = 0; i < MT_MAX_ALLOCS; i++) {
        const mt_alloc_rec_t* rec = &g_allocs[idx];

        if (rec->state == MT_STATE_EMPTY) {
            if (tombstone_idx != MT_MAX_ALLOCS) {
                if (out_idx) *out_idx = tombstone_idx;
            } else if (out_idx) {
                *out_idx = idx;
            }
            return 1;
        }

        if (rec->state == MT_STATE_TOMBSTONE && tombstone_idx == MT_MAX_ALLOCS) {
            tombstone_idx = idx;
        }

        idx = (idx + 1) & mask;
    }

    /* No EMPTY found. Use TOMBSTONE if available */
    if (tombstone_idx != MT_MAX_ALLOCS) {
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
    if (g_initialized) {
        return;
    }

    /* Initialize all slots to EMPTY */
    for (uint32_t i = 0; i < MT_MAX_ALLOCS; i++) {
        g_allocs[i].state = MT_STATE_EMPTY;
        g_allocs[i].ptr = 0;
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
    g_initialized = 1;
}

/* ============================================================================
 * PHASE 2E — mt_malloc
 * ============================================================================ */

void* mt_malloc(size_t size, const char* file, int line)
{
    if (!g_initialized) {
        mt_init();
    }

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
    uint16_t line_num;

    if (line < 0) {
        line_num = 0;
    } else if (line > 65535) {
        line_num = 65535;
    } else {
        line_num = (uint16_t)line;
    }

#if MT_CAPTURE_CALLSITE
#if MT_FILE_ID_MODE == 1
    /* Hash filename (FNV1a-32) */
    uint32_t hash = 2166136261u;    /* FNV1a offset basis */
    if (file != NULL) {
        for (const char* c = file; *c; c++) {
            hash ^= (uint8_t)*c;
            hash *= 16777619u;      /* FNV1a prime */
        }
    }
    file_id = hash;
#else
    /* Store pointer to filename string */
#if UINTPTR_MAX > 0xffffffffu
    file_id = (uint32_t)((uintptr_t)file ^ ((uintptr_t)file >> 32));
#else
    file_id = (uint32_t)(uintptr_t)file;
#endif
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
    rec->ptr = (uintptr_t)ptr;
    rec->size = mt_clamp_size_u32(size);
    rec->file_id = file_id;
    rec->line = line_num;
    rec->seq = current_seq;
    rec->state = MT_STATE_USED;

    /* Update counters */
    g_used_count++;
    g_total_allocs++;
    g_seq++;  /* Increment sequence after use */
    mt_add_current_used(rec->size);

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

    if (!g_initialized) {
        mt_init();
    }

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
        mt_sub_current_used(rec->size);  /* Update current usage */
        g_total_frees++;

        mt_slot_free(idx);              /* Mark as TOMBSTONE */
        MT_UNLOCK();
        g_inside = 0;
        MT_REAL_FREE(ptr);
        return;
    }

    /* If the pointer was already freed, do not forward it to the allocator. */
    for (uint32_t i = 0; i < MT_MAX_ALLOCS; i++) {
        if (g_allocs[i].state == MT_STATE_TOMBSTONE &&
            g_allocs[i].ptr == (uintptr_t)ptr) {
            MT_UNLOCK();
            g_inside = 0;
            return;
        }
    }

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
    if (!g_initialized) {
        mt_init();
    }

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

    uint32_t idx;
    int tracked = mt_find_slot(ptr, &idx);
    mt_alloc_rec_t* rec = tracked ? &g_allocs[idx] : NULL;
    uint32_t old_size = 0;
    uint32_t old_file_id = 0;
    uint16_t old_line = 0;
    if (tracked) {
        old_size = rec->size;
        old_file_id = rec->file_id;
        old_line = rec->line;
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

    if (tracked) {

        if (new_ptr == ptr) {
            rec->size = (uint32_t)size;
            if (size > old_size) {
                mt_add_current_used(mt_clamp_size_u32(size - old_size));
            } else {
                mt_sub_current_used(mt_clamp_size_u32(old_size - size));
            }
            rec->seq = g_seq++;
            g_total_reallocs++;
            MT_UNLOCK();
            g_inside = 0;
            return new_ptr;
        }

        mt_slot_free(idx);

        uint32_t new_idx;
        if (mt_find_insert_slot(new_ptr, &new_idx)) {
            mt_alloc_rec_t* new_rec = &g_allocs[new_idx];
            int reuse_tombstone = (new_rec->state == MT_STATE_TOMBSTONE);

            new_rec->ptr = (uintptr_t)new_ptr;
            new_rec->size = mt_clamp_size_u32(size);
            new_rec->file_id = old_file_id;
            new_rec->line = old_line;
            new_rec->seq = g_seq++;
            new_rec->state = MT_STATE_USED;

            if (reuse_tombstone && g_tombstone_count > 0) {
                g_tombstone_count--;
            }
            g_used_count++;

            if (size > old_size) {
                mt_add_current_used(mt_clamp_size_u32(size - old_size));
            } else {
                mt_sub_current_used(mt_clamp_size_u32(old_size - size));
            }

            g_total_reallocs++;
            MT_UNLOCK();
            g_inside = 0;
            return new_ptr;
        }
    } else {
        uint32_t new_idx;
        if (mt_find_insert_slot(new_ptr, &new_idx)) {
            mt_alloc_rec_t* new_rec = &g_allocs[new_idx];
            int reuse_tombstone = (new_rec->state == MT_STATE_TOMBSTONE);

            new_rec->ptr = (uintptr_t)new_ptr;
            new_rec->size = mt_clamp_size_u32(size);
            new_rec->file_id = 0;
            new_rec->line = 0;
            new_rec->seq = g_seq++;
            new_rec->state = MT_STATE_USED;

            if (reuse_tombstone && g_tombstone_count > 0) {
                g_tombstone_count--;
            }
            g_used_count++;
            mt_add_current_used(new_rec->size);
            g_total_reallocs++;
            MT_UNLOCK();
            g_inside = 0;
            return new_ptr;
        }
    }

    /* Should be unreachable because tombstones are reused on successful realloc. */
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
