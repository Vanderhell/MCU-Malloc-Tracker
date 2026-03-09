#ifndef MT_TYPES_H
#define MT_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include "mt_config.h"

/*
 * MCU Malloc Tracker - Type Definitions
 *
 * RULES:
 * - All types are fixed-size (no variable-length arrays or pointers)
 * - State byte (EMPTY/USED/TOMBSTONE) is explicit enum
 * - Deterministic and portable across platforms (little-endian binary format)
 */

/* ============================================================================
 * ALLOCATION RECORD STATE
 * ============================================================================ */
typedef enum {
    MT_STATE_EMPTY     = 0,
    MT_STATE_USED      = 1,
    MT_STATE_TOMBSTONE = 2
} mt_alloc_state_t;

/* ============================================================================
 * ALLOCATION RECORD
 * ============================================================================ */
typedef struct {
    uint64_t ptr;           /* Allocated pointer (always 64-bit for determinism) */
    uint32_t size;          /* Allocation size in bytes */
    uint32_t file_id;       /* File hash (FNV1a-32) or file ptr depending on MT_FILE_ID_MODE */
    uint16_t line;          /* Source line number (clamped to 65535) */
    uint8_t  state;         /* mt_alloc_state_t (0=EMPTY, 1=USED, 2=TOMBSTONE) */
    uint8_t  _pad;          /* Padding for alignment */
    uint32_t seq;           /* Sequence counter (deterministic ordering, not timestamp) */
} mt_alloc_rec_t;

/* Note: mt_alloc_rec_t size should be 28 bytes (packed) for binary format */
/* Size verification: compile with -DVERIFY_SIZES and check sizeof(mt_alloc_rec_t) */

/* ============================================================================
 * HEAP STATISTICS
 * ============================================================================ */
typedef struct {
    uint32_t current_used;      /* Currently allocated bytes */
    uint32_t peak_used;         /* Peak allocated bytes */
    uint32_t total_allocs;      /* Total malloc calls */
    uint32_t total_frees;       /* Total free calls */
    uint32_t total_reallocs;    /* Total realloc calls */
    uint32_t alloc_count;       /* Active allocation count */
    uint32_t table_used;        /* Used slots in alloc table */
    uint32_t table_tombstones;  /* Tombstone slots */
    uint32_t table_drops;       /* Dropped allocations (table full) */
    uint32_t largest_free;      /* Largest free block (0 if not available) */
    uint32_t total_free;        /* Total free bytes (0 if not available) */
    uint16_t frag_permille;     /* Fragmentation ratio in permille (0..1000, 0 if N/A) */
    uint8_t  frag_health;       /* 0=OK, 1=WARN, 2=CRITICAL, 3=NA */
    uint8_t  _pad;              /* Padding for alignment */
    uint32_t flags;             /* Status flags (see MT_STAT_FLAG_*) */
} mt_heap_stats_t;

/* Flags for mt_heap_stats_t.flags */
#define MT_STAT_FLAG_FRAG_NA    (1u << 0)  /* Fragmentation N/A (no platform support) */
#define MT_STAT_FLAG_DROPS      (1u << 1)  /* Has dropped allocations (table full) */
#define MT_STAT_FLAG_OVERFLOW   (1u << 2)  /* Snapshot overflow (Phase 5) */

/* ============================================================================
 * HOTSPOT RECORD (Phase 4)
 * ============================================================================ */
typedef struct {
    uint32_t file_id;       /* File hash (FNV1a-32) */
    uint16_t line;          /* Source line number */
    uint8_t  used;          /* 0=empty, 1=in use */
    uint8_t  _pad1;         /* Padding */
    uint32_t allocs;        /* Number of malloc calls from this site */
    uint32_t bytes;         /* Total bytes allocated from this site */
    uint32_t last_seq;      /* Last sequence number (for "recent activity") */
} mt_hotspot_rec_t;

/* Size check: should be 20 bytes */
/* Note: mt_hotspot_rec_t size = 20 bytes (fixed for determinism) */

/* ============================================================================
 * HOTSPOT TABLE (internal)
 * ============================================================================ */
typedef struct {
    mt_hotspot_rec_t records[MT_MAX_HOTSPOTS];
    uint32_t count;
} mt_hotspot_table_t;

/* ============================================================================
 * SNAPSHOT BINARY FORMAT HEADER
 * ============================================================================ */
typedef struct {
    char     magic[4];          /* "MTS1" */
    uint16_t version;           /* Format version = 1 */
    uint16_t flags;             /* Bit flags (b0=overflow, ...) */
    uint32_t record_count;      /* Number of mt_alloc_rec_t records following */
    uint32_t current_used;      /* Current heap used bytes */
    uint32_t peak_used;         /* Peak heap used bytes */
    uint32_t total_allocs;      /* Total malloc calls at snapshot time */
    uint32_t total_frees;       /* Total free calls at snapshot time */
    uint32_t seq;               /* Global sequence counter at snapshot time */
    uint32_t crc32;             /* CRC32 of header (without this field) + records */
} mt_snapshot_header_t;

/* Note: mt_snapshot_header_t size should be 36 bytes (packed) for binary format */
/* Size verification: compile with -DVERIFY_SIZES and check sizeof(mt_snapshot_header_t) */

/* ============================================================================
 * SNAPSHOT OVERFLOW FLAG
 * ============================================================================ */
#define MT_SNAPSHOT_FLAG_OVERFLOW 0x0001

/* ============================================================================
 * INTERNAL TRACKER STATE (not exposed to API)
 * ============================================================================ */
typedef struct {
    mt_alloc_rec_t allocs[MT_MAX_ALLOCS];
    mt_hotspot_table_t hotspots;
    uint32_t seq;               /* Monotonic sequence counter (deterministic age) */
    uint32_t total_allocs;      /* Lifetime malloc count */
    uint32_t total_frees;       /* Lifetime free count */
    uint32_t total_reallocs;    /* Lifetime realloc count */
    uint32_t current_used;      /* Current bytes allocated */
    uint32_t peak_used;         /* Peak bytes allocated */
    int      inside;            /* Recursion guard (1=inside tracker, 0=outside) */
} mt_tracker_state_t;

#endif /* MT_TYPES_H */
