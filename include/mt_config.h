#ifndef MT_CONFIG_H
#define MT_CONFIG_H

/*
 * MCU Malloc Tracker - Configuration Header
 * RULES:
 * - All config values are compile-time constants (no runtime overhead)
 * - Power-of-two enforcement on MT_MAX_ALLOCS (hard fail)
 * - No dynamic allocation in tracker internals
 */

/* ============================================================================
 * ALLOCATION TABLE SIZE (CRITICAL: must be power-of-two)
 * ============================================================================ */
#ifndef MT_MAX_ALLOCS
#define MT_MAX_ALLOCS 512
#endif

/* Compile-time power-of-two check */
#if (MT_MAX_ALLOCS & (MT_MAX_ALLOCS - 1)) != 0
#error "MT_MAX_ALLOCS must be power-of-two (e.g., 128/256/512/1024). This is required for O(1) hash table performance."
#endif

/* ============================================================================
 * HOTSPOT TRACKING TABLE SIZE
 * ============================================================================ */
#ifndef MT_MAX_HOTSPOTS
#define MT_MAX_HOTSPOTS 64
#endif

/* ============================================================================
 * FEATURE ENABLEMENT
 * ============================================================================ */
#ifndef MT_ENABLE_SNAPSHOT
#define MT_ENABLE_SNAPSHOT 1
#endif

#ifndef MT_ENABLE_TELEMETRY
#define MT_ENABLE_TELEMETRY 0
#endif

#ifndef MT_TELEMETRY_PERIOD_TICKS
#define MT_TELEMETRY_PERIOD_TICKS 10000  /* ~1s at 10kHz */
#endif

#ifndef MT_ENABLE_HOTSPOTS
#define MT_ENABLE_HOTSPOTS 1
#endif

#ifndef MT_HOTSPOT_TRACK_REALLOC
#define MT_HOTSPOT_TRACK_REALLOC 0
#endif

#ifndef MT_ENABLE_LEAK_DUMP
#define MT_ENABLE_LEAK_DUMP 1
#endif

#ifndef MT_CAPTURE_CALLSITE
#define MT_CAPTURE_CALLSITE 1
#endif

/* ============================================================================
 * FILE ID STORAGE MODE
 * ============================================================================ */
#ifndef MT_FILE_ID_MODE
#define MT_FILE_ID_MODE 1
#endif

/*
 * MT_FILE_ID_MODE == 0:
 *   Store pointer to __FILE__ string (RAM heavier, simplest)
 *   Requires string storage in binary snapshot
 *
 * MT_FILE_ID_MODE == 1:
 *   Store FNV1a-32 hash of __FILE__ (deterministic, smaller)
 *   Requires external symbol map file for decoding (--filemap)
 */

/* ============================================================================
 * SYNCHRONIZATION PRIMITIVES (ISR SAFETY)
 * ============================================================================ */
#ifndef MT_LOCK
#define MT_LOCK() do {} while(0)
#endif

#ifndef MT_UNLOCK
#define MT_UNLOCK() do {} while(0)
#endif

/*
 * Default: no-op locks (tracker is single-threaded)
 * User must provide MT_LOCK/MT_UNLOCK if malloc is called from ISR
 * Example override for STM32:
 *   #define MT_LOCK()   __disable_irq()
 *   #define MT_UNLOCK() __enable_irq()
 */

/* ============================================================================
 * UNDERLYING ALLOCATOR HOOKS
 * ============================================================================ */
#ifndef MT_REAL_MALLOC
#define MT_REAL_MALLOC malloc
#endif

#ifndef MT_REAL_FREE
#define MT_REAL_FREE free
#endif

#ifndef MT_REAL_REALLOC
#define MT_REAL_REALLOC realloc
#endif

/*
 * These point to the real malloc/free/realloc implementation
 * Default: standard C library functions
 * Can be overridden for custom allocators (e.g., heap at fixed address)
 */

/* ============================================================================
 * PLATFORM HEAP WALK (OPTIONAL)
 * ============================================================================ */
#ifndef MT_PLATFORM_HEAP_WALK
#define MT_PLATFORM_HEAP_WALK 0
#endif

/*
 * MT_PLATFORM_HEAP_WALK == 0 (default):
 *   Fragmentation metrics are estimated/unavailable
 *   Reliable leak detection only
 *
 * MT_PLATFORM_HEAP_WALK == 1:
 *   Platform provides heap walk callbacks
 *   Requires user to implement:
 *     - mt_platform_heap_total_free()
 *     - mt_platform_heap_largest_free()
 *   See KNOWN_LIMITS.md for details
 */

#if MT_PLATFORM_HEAP_WALK
/* Forward declarations for platform heap walk (user must implement) */
extern uint32_t mt_platform_heap_total_free(void);
extern uint32_t mt_platform_heap_largest_free(void);
#endif

/* ============================================================================
 * SNAPSHOT BUFFER SIZE
 * ============================================================================ */
#ifndef MT_SNAPSHOT_BUFFER_SIZE
#define MT_SNAPSHOT_BUFFER_SIZE 4096
#endif

/*
 * Maximum size of binary snapshot output buffer
 * If snapshot is larger, truncation occurs (reported in binary header)
 */

/* ============================================================================
 * COMPILE-TIME ASSERTIONS (C11 + fallback)
 * ============================================================================ */
#if __STDC_VERSION__ >= 201112L
#define MT_STATIC_ASSERT(cond, msg) _Static_assert((cond), msg)
#else
#define MT_STATIC_ASSERT(cond, msg) typedef int mt_static_assert_[(cond) ? 1 : -1]
#endif

/* Validate critical compile-time config */
MT_STATIC_ASSERT(MT_MAX_ALLOCS > 0, "MT_MAX_ALLOCS must be > 0");
MT_STATIC_ASSERT(MT_MAX_HOTSPOTS > 0, "MT_MAX_HOTSPOTS must be > 0");
MT_STATIC_ASSERT(MT_MAX_HOTSPOTS <= MT_MAX_ALLOCS, "MT_MAX_HOTSPOTS must be <= MT_MAX_ALLOCS");

#endif /* MT_CONFIG_H */
