# Configuration

Configuration is in `include/mt_config.h`.

## Required Rules

### MT_MAX_ALLOCS must be power-of-two

Build will FAIL if it is not one of:
- 128, 256, 512, 1024, 2048, 4096, ...

## Most Important Settings

### Allocation Capacity

- `MT_MAX_ALLOCS` (default: 512)
  - Larger = more RAM, fewer drops
  - Smaller = faster table saturation

### Hotspots

- `MT_ENABLE_HOTSPOTS` (0/1, default 1)
- `MT_MAX_HOTSPOTS` (default 64)
- `MT_HOTSPOT_TRACK_REALLOC` (default 0)

### Snapshot

- `MT_ENABLE_SNAPSHOT` (0/1, default 1)
- CRC32 in snapshot format (MTS1 v1)

### Telemetry

- `MT_ENABLE_TELEMETRY` (0/1, default 0)
- `MT_TELEMETRY_PERIOD_TICKS` (e.g., 10000 ~ 1s)

### Callsite Capture

- `MT_CAPTURE_CALLSITE` (0/1, default 1)
- `MT_FILE_ID_MODE`
  - 0 = Store pointer to `__FILE__` (more RAM, simple)
  - 1 = Store 32-bit hash (compact, works with filemap tool) **recommended**

### ISR Safety

- `MT_LOCK()` / `MT_UNLOCK()`
  - Default: no-op
  - User must define for their platform if ISR malloc needed

### Platform Heap-Walk (Real Fragmentation)

- `MT_PLATFORM_HEAP_WALK` (0/1, default 0)

If enabled (1), user must provide:
- `size_t mt_platform_heap_total_free(void);`
- `size_t mt_platform_heap_largest_free(void);`

Without these, fragmentation reports as N/A (honest).

## Health Monitoring

Monitor via `mt_stats()` and dumps:

- `drops > 0` → Table too small or too many simultaneous allocations
- `tombstones` increasing → Longer lookups (still correct)
