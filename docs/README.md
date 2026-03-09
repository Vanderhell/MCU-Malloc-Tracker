# MCU Malloc Tracker — Documentation

**Minimalist diagnostic C library for microcontrollers (MCU)**

Tracks `malloc/free/realloc` allocations with deterministic heap diagnostics without OS, without internal dynamic allocation, and with minimal runtime overhead.

## Motto

**"Know your heap."** 🎯

---

## Key Features

### 1. Leak Detection
Identifies all unfreed allocations with output showing:
- Pointer address
- Size
- Source file + line
- Sequence number (order)

### 2. Allocation Hotspot Detection
Identifies which call sites create the most allocations:
- foo.c:120 → 540 allocs
- driver_spi.c:88 → 430 allocs
- net.c:221 → 120 allocs

### 3. Heap Fragmentation Analysis
**Two modes:**

#### Mode A: Real Fragmentation (with Platform Support)
If your platform provides heap walk hooks:
```c
MT_PLATFORM_HEAP_WALK = 1
size_t mt_platform_heap_total_free(void);
size_t mt_platform_heap_largest_free(void);
```

Tracker calculates:
- Total free bytes
- Largest contiguous free block
- Fragmentation ratio (0..1000 permille)
- Health status: OK / WARNING / CRITICAL

#### Mode B: N/A (No Platform Support)
If platform is not supported:
```c
MT_PLATFORM_HEAP_WALK = 0  (default)
```

Tracker explicitly reports:
- `frag_health = N/A`
- `MT_STAT_FLAG_FRAG_NA` flag
- All fragmentation metrics = 0

**Important:** Tracker never lies. Without heap walk, fragmentation cannot be calculated and this is honestly reported.

### 4. Deterministic Binary Dump
Heap snapshot in the form of:
- **Deterministic binary format** (little-endian, fixed-size)
- Time-independent (no RTC dependencies)
- Suitable for storage before reset / watchdog event
- PC decoder tool for analysis

### 5. Heap Telemetry (Optional)
Periodic output showing:
- Current used
- Peak used
- Allocation count
- Fragmentation status
- Drop count (if table full)

---

## Design Principles

- **0 malloc in tracker**: All data is static (fixed-size tables)
- **O(1) operations**: Hash table with linear probing
- **Deterministic**: No timestamps, no randomness, no OS
- **ISR safe**: Optional MT_LOCK/MT_UNLOCK (default no-op)
- **Tiny RAM**: ~3 KB default (configurable)
- **Tiny runtime**: < 1 µs per malloc/free overhead

---

## Integration

### Drop-in Macros (Debug Build)

```c
#include "mt_api.h"

#ifdef DEBUG
#define malloc(x)       mt_malloc((x), __FILE__, __LINE__)
#define free(p)         mt_free((p), __FILE__, __LINE__)
#define realloc(p, x)   mt_realloc((p), (x), __FILE__, __LINE__)
#endif
```

### Initialization

```c
mt_init();  /* Once at startup */
```

### Get Stats

```c
mt_heap_stats_t stats = mt_stats();
printf("Used: %u bytes\n", stats.current_used);
printf("Peak: %u bytes\n", stats.peak_used);
printf("Frag status: %d (0=OK, 1=WARN, 2=CRITICAL, 3=N/A)\n", stats.frag_health);
```

### Dump Leaks

```c
void my_output(const char* s) { printf("%s", s); }
mt_dump_leaks(my_output);
```

---

## Supported Platforms

Target: STM32, ESP32, nRF52, RP2040 (bare-metal, no OS)

### Fragmentation Support

| Platform | Heap Walk | Status |
|----------|-----------|--------|
| STM32F4 | Custom required | Ready (user implements) |
| STM32H7 | Custom required | Ready (user implements) |
| ESP32 | Custom required | Ready (user implements) |
| nRF52 | Custom required | Ready (user implements) |
| RP2040 | Custom required | Ready (user implements) |

---

## Configuration

File: `include/mt_config.h`

### Key Options

| Option | Default | Meaning |
|--------|---------|---------|
| `MT_MAX_ALLOCS` | 512 | Max tracked allocations (must be power-of-2) |
| `MT_MAX_HOTSPOTS` | 64 | Max hotspot entries |
| `MT_ENABLE_SNAPSHOT` | 1 | Binary dump feature |
| `MT_ENABLE_HOTSPOTS` | 1 | Hotspot tracking |
| `MT_ENABLE_LEAK_DUMP` | 1 | Leak listing |
| `MT_FILE_ID_MODE` | 1 | 0=strings, 1=hash (smaller binary) |
| `MT_PLATFORM_HEAP_WALK` | 0 | 0=N/A, 1=with platform hooks |

See [CONFIG.md](CONFIG.md) for all options.

---

## Memory Overhead

Typical configuration:
- Allocation table: ~14 KB (512 × 28 B)
- Counters: ~128 B
- Hotspot table: ~4 KB (64 × 12 B)

**Total: ~18 KB** (configurable via MT_MAX_ALLOCS)

---

## Runtime Overhead

Per operation (typical MCU @ 100 MHz):
- malloc overhead: < 1 µs (hash insert)
- free overhead: < 1 µs (hash find + tombstone)
- realloc overhead: < 1 µs (record update)

---

## Known Limitations

1. **Fragmentation requires platform support**
   - Without heap walk hooks, fragmentation is N/A (but honestly reported)
   - With hooks, real metrics available

2. **Table full behavior**
   - When MT_MAX_ALLOCS exhausted, new allocations are dropped
   - Detectable via `stats.table_drops` and `MT_STAT_FLAG_DROPS`

3. **No rehashing**
   - Linear probing degrades with many tombstones
   - Acceptable for MCU (small tables, short-lived allocations)

4. **ISR malloc**
   - Requires user-provided MT_LOCK/MT_UNLOCK
   - Default is no-op (single-threaded only)

---

## Documentation

- [INTEGRATION.md](INTEGRATION.md) — How to integrate
- [CONFIG.md](CONFIG.md) — Configuration options
- [DESIGN.md](DESIGN.md) — Architectural decisions
- [KNOWN_LIMITS.md](KNOWN_LIMITS.md) — Detailed limitations + risks
- [FILEMAP.md](FILEMAP.md) — File ID mapping for decoder

---

## Build & Test

```bash
mkdir build && cd build
cmake ..
cmake --build .
./Debug/selftest_phase2.exe
./Debug/selftest_phase3.exe
```

---

## License

MIT (see LICENSE)

---

**This library is designed for embedded systems where visibility into heap behavior is critical but tools like Valgrind or AddressSanitizer are unavailable.**
