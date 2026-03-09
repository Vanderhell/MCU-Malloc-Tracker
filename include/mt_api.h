#ifndef MT_API_H
#define MT_API_H

#include <stddef.h>
#include <stdint.h>
#include "mt_types.h"

/*
 * MCU Malloc Tracker - Public API
 *
 * Clean, minimal API for malloc/free tracking on MCU.
 * All functions are deterministic and non-blocking.
 * No dynamic allocation within tracker (guaranteed by design).
 */

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

/**
 * mt_init()
 * Initialize the malloc tracker.
 * Must be called once before any malloc/free/realloc calls.
 * Safe to call multiple times (idempotent).
 */
void mt_init(void);

/* ============================================================================
 * ALLOCATION TRACKING
 * ============================================================================ */

/**
 * mt_malloc(size, file, line)
 * Tracked malloc wrapper.
 *
 * Args:
 *   size: allocation size in bytes
 *   file: __FILE__ macro (or any const char* filename)
 *   line: __LINE__ macro (or line number)
 *
 * Returns:
 *   Pointer to allocated memory (or NULL if allocation fails)
 *
 * Typical usage:
 *   #define malloc(x) mt_malloc((x), __FILE__, __LINE__)
 *
 * Notes:
 *   - Calls MT_REAL_MALLOC internally
 *   - Tracks allocation in fixed-size table
 *   - If table is full (MT_MAX_ALLOCS reached), new allocs are dropped
 *     with a warning (see KNOWN_LIMITS.md)
 */
void* mt_malloc(size_t size, const char* file, int line);

/**
 * mt_free(ptr, file, line)
 * Tracked free wrapper.
 *
 * Args:
 *   ptr: pointer to free
 *   file: __FILE__ macro
 *   line: __LINE__ macro
 *
 * Notes:
 *   - Calls MT_REAL_FREE internally
 *   - Marks allocation record as TOMBSTONE (not removed, to preserve hash table structure)
 *   - Safe to call with NULL pointer
 */
void mt_free(void* ptr, const char* file, int line);

/**
 * mt_realloc(ptr, size, file, line)
 * Tracked realloc wrapper.
 *
 * Args:
 *   ptr: pointer to reallocate (or NULL)
 *   size: new size in bytes (or 0 to free)
 *   file: __FILE__ macro
 *   line: __LINE__ macro
 *
 * Returns:
 *   New pointer to resized memory (or NULL if realloc fails)
 *
 * Notes:
 *   - Calls MT_REAL_REALLOC internally
 *   - Handles 3 cases: ptr==NULL (malloc), size==0 (free), else (resize)
 *   - Updates tracking records accordingly
 */
void* mt_realloc(void* ptr, size_t size, const char* file, int line);

/* ============================================================================
 * HEAP STATISTICS & DIAGNOSTICS
 * ============================================================================ */

/**
 * mt_stats()
 * Get current heap statistics.
 *
 * Returns:
 *   mt_heap_stats_t with current metrics
 *
 * Notes:
 *   - O(n) operation where n = MT_MAX_ALLOCS (but typically fast)
 *   - Fragmentation metrics (largest_free, total_free) require MT_PLATFORM_HEAP_WALK
 *     If not available, they are set to 0 (see KNOWN_LIMITS.md)
 *   - Safe to call from any context
 */
mt_heap_stats_t mt_stats(void);

/* ============================================================================
 * SNAPSHOT & BINARY DUMP
 * ============================================================================ */

#if MT_ENABLE_SNAPSHOT

/**
 * mt_snapshot_write(out, out_cap)
 * Write binary snapshot of current heap state.
 *
 * Args:
 *   out: output buffer
 *   out_cap: buffer capacity in bytes
 *
 * Returns:
 *   Number of bytes written to out
 *   If snapshot is larger than out_cap, truncated (see MT_SNAPSHOT_FLAG_OVERFLOW in header)
 *
 * Binary format:
 *   - Header: mt_snapshot_header_t (40 bytes)
 *   - Records: array of mt_alloc_rec_t (28 bytes each)
 *   - Records are deterministic: sorted by ptr (ascending)
 *   - Only USED records are included (EMPTY/TOMBSTONE skipped)
 *
 * Notes:
 *   - Deterministic and reproducible (no timestamps, no random)
 *   - Use tools/mt_decode.py to parse binary dump
 *   - Suitable for crash dumps (save to flash/RAM before reset)
 */
size_t mt_snapshot_write(uint8_t* out, size_t out_cap);

#endif /* MT_ENABLE_SNAPSHOT */

/* ============================================================================
 * TEXT DUMPS (via callback)
 * ============================================================================ */

/**
 * mt_dump_uart(write_fn)
 * Dump all statistics to UART (or any output).
 *
 * Args:
 *   write_fn: callback function void (*)(const char* s) to output text
 *            Each call contains a line or partial line.
 *            Callback is responsible for UART, RTT, printf, etc.
 *
 * Output includes:
 *   - Heap statistics (current_used, peak, alloc count, etc.)
 *   - Table usage (USED/TOMBSTONE/EMPTY slots)
 *   - Fragmentation status (if available)
 *
 * Notes:
 *   - Human-readable format
 *   - Safe to call from main context (not ISR)
 *   - write_fn must be provided by user (tracker is output-agnostic)
 */
void mt_dump_uart(void (*write_fn)(const char* s));

#if MT_ENABLE_LEAK_DUMP

/**
 * mt_dump_leaks(write_fn)
 * Dump all active (unfreed) allocations.
 *
 * Args:
 *   write_fn: callback function void (*)(const char* s) for output
 *
 * Output format (per allocation):
 *   ptr=0x20001820 size=128 file_id=0xHASH line=221
 *   (if MT_FILE_ID_MODE==0, file_id is replaced with filename string)
 *
 * Notes:
 *   - Lists all USED records (potential leaks)
 *   - Sorted deterministically (by ptr ascending)
 *   - Useful for identifying unfreed blocks before crash/reset
 */
void mt_dump_leaks(void (*write_fn)(const char* s));

#endif /* MT_ENABLE_LEAK_DUMP */

#if MT_ENABLE_HOTSPOTS

/**
 * mt_dump_hotspots(write_fn)
 * Dump allocation hotspots (call sites with most allocations).
 *
 * Args:
 *   write_fn: callback function void (*)(const char* s) for output
 *
 * Output format (per hotspot, top N):
 *   file_id=0xHASH line=120 count=540
 *   (if MT_FILE_ID_MODE==0, file_id is replaced with filename string)
 *
 * Notes:
 *   - Top MT_MAX_HOTSPOTS entries
 *   - Sorted by alloc count (descending), then deterministically
 *   - Helps identify inefficient allocation patterns
 */
void mt_dump_hotspots(void (*write_fn)(const char* s));

#endif /* MT_ENABLE_HOTSPOTS */

#if MT_ENABLE_TELEMETRY

/**
 * mt_telemetry_tick(now_ticks, write_fn)
 * Optional periodic telemetry report (no OS required).
 *
 * Args:
 *   now_ticks: current tick counter (monotonic, user-provided)
 *   write_fn: callback function void (*)(const char* s) for output
 *
 * Contract:
 *   - User calls this periodically (e.g., every 1s/10s)
 *   - Library tracks last_ticks internally
 *   - When now_ticks - last_ticks >= MT_TELEMETRY_PERIOD_TICKS,
 *     outputs a single-line telemetry report
 *   - No OS/time functions required (tick counter is external)
 *
 * Output format:
 *   MT: used=XXXX peak=XXXX active=XXX drops=XXX frag=NA|XXXPM
 *
 * Notes:
 *   - Lightweight report (single line, minimal overhead)
 *   - Useful for real-time monitoring without full dumps
 */
void mt_telemetry_tick(uint32_t now_ticks, void (*write_fn)(const char* s));

#endif /* MT_ENABLE_TELEMETRY */

/* ============================================================================
 * CONVENIENCE MACROS (user-defined)
 * ============================================================================ */

/*
 * Drop-in replacement macros for debugging builds:
 *
 * In debug build, add to your project:
 *
 *   #define malloc(x)       mt_malloc((x), __FILE__, __LINE__)
 *   #define free(p)         mt_free((p), __FILE__, __LINE__)
 *   #define realloc(p, x)   mt_realloc((p), (x), __FILE__, __LINE__)
 *
 * Then all malloc/free/realloc calls are automatically tracked.
 *
 * CAUTION: Some vendor libraries may bypass these macros or require real malloc.
 * In that case, selectively undefine/redefine per module (see INTEGRATION.md).
 */

#endif /* MT_API_H */
