#ifndef MT_HOTSPOTS_H
#define MT_HOTSPOTS_H

#include <stddef.h>
#include <stdint.h>
#include "mt_types.h"
#include "mt_config.h"

/*
 * MCU Malloc Tracker - Hotspot Tracking (Internal)
 *
 * This header is STRICTLY INTERNAL (not part of public API).
 * Used by mt_core.c to record allocation hotspots.
 */

#if MT_ENABLE_HOTSPOTS

/**
 * mt_hotspot_record(file_id, line, size, seq)
 * Record a malloc event at (file_id, line).
 *
 * O(n) average, where n = MT_MAX_HOTSPOTS (typically 64).
 * Called by mt_malloc() after successful allocation.
 */
void mt_hotspot_record(uint32_t file_id, uint16_t line, uint32_t size, uint32_t seq);

/**
 * mt_hotspot_init()
 * Initialize hotspot table (called from mt_init()).
 */
void mt_hotspot_init(void);

/**
 * mt_hotspot_table()
 * Get pointer to hotspot table (for dumps).
 */
const mt_hotspot_rec_t* mt_hotspot_table(void);

/**
 * mt_hotspot_count()
 * Get number of active hotspots.
 */
uint32_t mt_hotspot_count(void);

/**
 * mt_hotspot_drops()
 * Get number of dropped hotspots (table was full).
 */
uint32_t mt_hotspot_drops(void);

#else /* MT_ENABLE_HOTSPOTS */

/* Stubs when hotspot tracking is disabled */
#define mt_hotspot_record(file_id, line, size, seq) do {} while(0)
#define mt_hotspot_init() do {} while(0)
#define mt_hotspot_table() NULL
#define mt_hotspot_count() 0
#define mt_hotspot_drops() 0

#endif /* MT_ENABLE_HOTSPOTS */

#endif /* MT_HOTSPOTS_H */
