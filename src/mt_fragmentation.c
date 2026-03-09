/*
 * MCU Malloc Tracker - Fragmentation Analysis
 *
 * Two modes:
 * A) Platform heap-walk (if MT_PLATFORM_HEAP_WALK==1)
 *    Requires user to provide: mt_platform_heap_total_free(), mt_platform_heap_largest_free()
 * B) N/A (if MT_PLATFORM_HEAP_WALK==0)
 *    All fragmentation metrics set to 0/N/A with flag
 */

#include <stddef.h>
#include <stdint.h>
#include "../include/mt_config.h"
#include "../include/mt_types.h"

/* ============================================================================
 * FRAGMENTATION THRESHOLDS (configurable)
 * ============================================================================ */

#ifndef MT_FRAG_OK_MAX
#define MT_FRAG_OK_MAX 200          /* permille: 0..200 = OK */
#endif

#ifndef MT_FRAG_WARN_MAX
#define MT_FRAG_WARN_MAX 500        /* permille: 200..500 = WARNING */
#endif

/* Above 500 permille = CRITICAL */

/* ============================================================================
 * FRAGMENTATION HEALTH ENUM
 * ============================================================================ */

typedef enum {
    MT_FRAG_HEALTH_OK       = 0,
    MT_FRAG_HEALTH_WARN     = 1,
    MT_FRAG_HEALTH_CRITICAL = 2,
    MT_FRAG_HEALTH_NA       = 3
} mt_frag_health_t;

/* ============================================================================
 * FRAGMENTATION CALCULATION
 * ============================================================================ */

/**
 * mt_calc_fragmentation()
 * Calculate fragmentation ratio in permille (0..1000) using fixed-point.
 *
 * Args:
 *   total_free: Total free bytes in heap
 *   largest_free: Largest contiguous free block
 *
 * Returns:
 *   Fragmentation in permille (0..1000)
 *   0 = no fragmentation (largest == total)
 *   1000 = maximum fragmentation (largest ~= 0)
 *
 * Formula (fixed-point):
 *   frag = 1000 - (largest_free * 1000) / total_free
 *   clamped to [0, 1000]
 *
 * Special case:
 *   If total_free == 0, return 0 (no free space = no fragmentation issue)
 */
static uint16_t mt_calc_fragmentation(uint32_t total_free, uint32_t largest_free)
{
    if (total_free == 0) {
        return 0;  /* No free space */
    }

    if (largest_free >= total_free) {
        return 0;  /* No fragmentation (largest == total) */
    }

    /* Fixed-point: (largest * 1000) / total, then 1000 - ratio */
    uint32_t ratio = (largest_free * 1000u) / total_free;
    uint16_t frag = 1000u - ratio;

    /* Clamp to [0, 1000] (shouldn't be needed, but safe) */
    if (frag > 1000) {
        frag = 1000;
    }

    return frag;
}

/**
 * mt_frag_health_classify()
 * Classify fragmentation health based on permille ratio.
 */
static mt_frag_health_t mt_frag_health_classify(uint16_t frag_permille, int frag_available)
{
    if (!frag_available) {
        return MT_FRAG_HEALTH_NA;
    }

    if (frag_permille < MT_FRAG_OK_MAX) {
        return MT_FRAG_HEALTH_OK;
    }

    if (frag_permille < MT_FRAG_WARN_MAX) {
        return MT_FRAG_HEALTH_WARN;
    }

    return MT_FRAG_HEALTH_CRITICAL;
}

/* ============================================================================
 * PUBLIC FRAGMENTATION FUNCTION
 * ============================================================================ */

/**
 * mt_get_fragmentation()
 * Get fragmentation metrics.
 *
 * Returns:
 *   uint32_t with packed data:
 *   - Bits 0..15: frag_permille (0..1000)
 *   - Bits 16..18: health (0=OK, 1=WARN, 2=CRITICAL, 3=NA)
 *   - Bit 19: available flag (1 if real data, 0 if N/A)
 */
uint32_t mt_get_fragmentation(uint32_t* out_total_free, uint32_t* out_largest_free)
{
    uint32_t total_free = 0;
    uint32_t largest_free = 0;
    mt_frag_health_t health = MT_FRAG_HEALTH_NA;
    uint16_t frag_permille = 0;
    int frag_available = 0;

#if MT_PLATFORM_HEAP_WALK == 1
    /* Mode A: Platform heap walk (user-provided hooks) */
    total_free = mt_platform_heap_total_free();
    largest_free = mt_platform_heap_largest_free();
    frag_permille = mt_calc_fragmentation(total_free, largest_free);
    health = mt_frag_health_classify(frag_permille, 1);
    frag_available = 1;
#else
    /* Mode B: N/A (no platform support) */
    total_free = 0;
    largest_free = 0;
    frag_permille = 0;
    health = MT_FRAG_HEALTH_NA;
    frag_available = 0;
#endif

    if (out_total_free) {
        *out_total_free = total_free;
    }
    if (out_largest_free) {
        *out_largest_free = largest_free;
    }

    /* Pack into single uint32_t */
    uint32_t result = 0;
    result |= (frag_permille & 0xFFFF);           /* Bits 0..15: permille */
    result |= ((health & 0x03) << 16);            /* Bits 16..17: health */
    result |= ((frag_available & 0x01) << 19);    /* Bit 19: available */

    return result;
}

/**
 * mt_frag_health_from_packed()
 * Extract health enum from packed fragmentation value.
 */
mt_frag_health_t mt_frag_health_from_packed(uint32_t packed)
{
    return (mt_frag_health_t)((packed >> 16) & 0x03);
}

/**
 * mt_frag_permille_from_packed()
 * Extract fragmentation permille from packed value.
 */
uint16_t mt_frag_permille_from_packed(uint32_t packed)
{
    return (uint16_t)(packed & 0xFFFF);
}

/**
 * mt_frag_available_from_packed()
 * Check if fragmentation data is real (1) or N/A (0).
 */
int mt_frag_available_from_packed(uint32_t packed)
{
    return (packed >> 19) & 0x01;
}
