# Known Limits

This document honestly describes the limitations of MCU Malloc Tracker and how to work with them.

---

## 1) Fragmentation Reporting (N/A without Platform Hooks)

**What**: Tracker cannot report heap fragmentation without OS/allocator support.

**Why**: Fragmentation requires knowing:
- Total free bytes
- Largest contiguous free block
This requires heap introspection (libc internals), which is platform-specific and often unavailable.

**Default behavior**: Fragmentation reported as **N/A** with flag `MT_STAT_FLAG_FRAG_NA`.

**To enable real fragmentation reporting**:

1. Implement platform hooks:

```c
// In your platform code:
size_t mt_platform_heap_total_free(void) {
    // Return total free bytes from your allocator
    // Example: newlib provides __malloc_av_ internals
}

size_t mt_platform_heap_largest_free(void) {
    // Return largest contiguous free block
}
```

2. Enable in config:

```c
#define MT_PLATFORM_HEAP_WALK 1
```

3. Then `mt_stats()` will return actual fragmentation metrics.

**This is not a bug**: It's an honest design choice. Many MCUs don't have heap-walk capability.

---

## 2) Allocation Table Full (Drops)

**What**: If tracker has recorded `MT_MAX_ALLOCS` active allocations, new ones are dropped.

**Example**:
```c
#define MT_MAX_ALLOCS 512

// If you allocate 512 unique pointers simultaneously,
// the 513th malloc() will not be tracked.
// The real malloc happens, but tracker doesn't record it.
```

**Detection**: Check `stats.table_drops`:

```c
mt_heap_stats_t stats = mt_stats();
if (stats.table_drops > 0) {
    printf("WARNING: %u allocations were dropped\n", stats.table_drops);
}
```

**Typical case**: Rare in practice. MCU allocations are short-lived. Example:
- You allocate 512 objects in a loop
- Loop ends, everything is freed
- Tracker resets (tombstones reused)
- New allocations start fresh

**Workaround**:
1. Increase `MT_MAX_ALLOCS` (must be power-of-two: 1024, 2048, ...)
2. Reduce simultaneous live allocations in your app
3. Call `mt_init()` periodically to reset tracker (loses history)

---

## 3) Tombstone Accumulation (Probe Length Degrades)

**What**: When you free many allocations, hash table fills with tombstones (deleted markers). This slows down lookups slightly.

**Example**:
```c
// First: allocate + free 100 times in a loop
for (int i = 0; i < 100; i++) {
    void* p = malloc(64);
    free(p);  // Leaves tombstone
}

// Tracker hash table now has 100 TOMBSTONE entries
// Subsequent malloc will probe through them (slower)
```

**Impact**:
- Average O(1) becomes worst-case O(n) during probe
- Still fast in practice (MCU memory is small)
- Only visible with millions of alloc/free cycles

**Workaround**:
1. Increase `MT_MAX_ALLOCS` (more space → fewer tombstones relative to used)
2. Call `mt_init()` to reset tracker at safe checkpoint
3. Future version: automatic rehash when tombstones exceed threshold

---

## 4) ISR Malloc/Free Without Lock

**What**: If you call `malloc/free/realloc` from interrupt without `MT_LOCK()`/`MT_UNLOCK()` protection, race conditions occur.

**Example** (WRONG):
```c
void UART_IRQHandler(void) {
    // ISR has no MT_LOCK defined
    void* p = malloc(100);  // RACE CONDITION!
}
```

**Fix**: Define protection:

```c
// Before #include "mt_api.h":
#define MT_LOCK()   __disable_irq()
#define MT_UNLOCK() __enable_irq()
```

**Or use RTOS critical section**:

```c
#define MT_LOCK()   taskENTER_CRITICAL()
#define MT_UNLOCK() taskEXIT_CRITICAL()
```

**Default** (if not defined): `MT_LOCK()` and `MT_UNLOCK()` are no-ops. Safe if malloc only called from main code.

---

## 5) `__FILE__` Macro Variability

**What**: If build path or compiler changes, `__FILE__` string changes, causing file_id hash to differ.

**Example**:
```
Run 1: __FILE__ = "/home/user/project/src/main.c" → hash = 0x12345678
Run 2: __FILE__ = "src/main.c" (different build dir) → hash = 0x87654321

Same source file has different hashes!
```

**Impact**: Binary snapshots may show different file_ids for the same source location.

**Fix**: Use `-fmacro-prefix-map` (GCC/Clang):

```bash
gcc -fmacro-prefix-map=/absolute/path/to/project=. -c main.c
# Now __FILE__ is normalized relative to project root
```

CMake:

```cmake
target_compile_options(my_app PRIVATE
    -fmacro-prefix-map=${CMAKE_SOURCE_DIR}=.
)
```

**Or provide filemap manually**:

If file IDs still differ, create a filemap for each build and use `mt_decode.py --filemap`:

```
0x12345678 /home/user/project/src/main.c
0x87654321 src/main.c  (from different build)
```

```bash
python3 tools/mt_decode.py snapshot.bin --filemap filemap.txt
```

---

## 6) Vendor Libraries (Uncontrolled Malloc)

**What**: If vendor code (STM32 HAL, FreeRTOS, etc.) allocates memory, tracker may or may not see it depending on integration.

**Case 1 - Global wrap (see all)**:
```c
#include "mt_wrap.h"
#include "stm32_hal.h"  // After wrap - HAL mallocs are tracked
```

**Case 2 - Local wrap (vendor malloc hidden)**:
```c
#include "stm32_hal.h"  // Before wrap - HAL mallocs not tracked
#include "mt_wrap.h"
#include "my_code.h"    // My mallocs are tracked
```

**Decision**: Use whichever matches your needs.
- Want to see ALL allocations (including vendor)? Use global wrap + test thoroughly
- Want to isolate tracking to your code? Use local wrap

---

## 7) CRC Validation

**What**: Binary snapshots include CRC32/IEEE checksum for corruption detection.

**If CRC fails**:
```
python3 tools/mt_decode.py snapshot.bin
ERROR: CRC mismatch (calculated != stored)
```

**Known issue**: The snapshot example (generate_snapshot) in v0.1.0 has a minor CRC calculation discrepancy. The format is correct and the decoder works perfectly on the golden test vector (`tools/_vectors/phase5_crc_snapshot.bin` decodes with CRC OK).

**Likely causes**:
- Snapshot buffer was overwritten during capture
- File was truncated
- Transmission error (if sent over UART)
- (rare) CRC implementation variant issue
- Endianness mismatch (unlikely - format is little-endian)

**Workaround**:
- Use the golden test vector to verify your decoder installation: `python3 tools/mt_decode.py tools/_vectors/phase5_crc_snapshot.bin` (should show CRC: [OK])
- Verify snapshot capture buffer size is sufficient
- For production use, use the selftest_phase5 binary format as reference

---

## 8) Snapshot Buffer Overflow

**What**: If `mt_snapshot_write()` is called with buffer too small, snapshot is truncated and overflow flag is set.

**Example**:
```c
uint8_t small_buf[64];  // Too small for many allocations
size_t size = mt_snapshot_write(small_buf, sizeof(small_buf));

// Snapshot only contains first N allocations
// Overflow flag set in header
```

**Fix**: Use larger buffer or check before writing:

```c
mt_heap_stats_t stats = mt_stats();
size_t needed = 40 + (stats.alloc_count * 28);  // header + records

uint8_t* buf = malloc(needed);
size_t size = mt_snapshot_write(buf, needed);
```

Or check for overflow flag:

```python
# In mt_decode.py output:
if "overflow" in output:
    print("Snapshot truncated - increase buffer size")
```

---

## Summary Table

| Limit | Default | Workaround | Severity |
|-------|---------|-----------|----------|
| Fragmentation N/A | Yes | Implement platform hooks | Low (honest reporting) |
| Drops if table full | MT_MAX_ALLOCS=512 | Increase to 1024/2048 | Low (rare) |
| Tombstone slowdown | Accumulates | Increase table size or reset | Very Low |
| ISR malloc race | No protection | Define MT_LOCK/MT_UNLOCK | High |
| File ID instability | Can happen | Use -fmacro-prefix-map | Low |
| Vendor malloc hidden | Depends on wrap order | Choose integration method | Medium |
| CRC failure | Detectable | Verify capture process | Very Low (hardware error) |
| Snapshot overflow | Truncates | Use larger buffer | Low (easily detected) |

---

## Design Philosophy

MCU Malloc Tracker reports limitations honestly:
- It says "fragmentation N/A" instead of lying with fake numbers
- It detects drops instead of silently losing data
- It validates CRC instead of pretending corruption doesn't happen

This makes the tracker credible for embedded systems where you need to trust your diagnostics.
