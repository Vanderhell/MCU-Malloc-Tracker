# MCU Malloc Tracker (v0.1.0)

![C99](https://img.shields.io/badge/C-99-blue?style=flat-square)
![License MIT](https://img.shields.io/badge/license-MIT-green?style=flat-square)
![Status](https://img.shields.io/badge/status-stable-brightgreen?style=flat-square)

Diagnostic C library for bare-metal MCU that tracks `malloc/free/realloc`, detects **memory leaks**, identifies **allocation hotspots**, and captures **deterministic binary snapshots** for post-mortem heap analysis.

Works on: **STM32, ESP32, nRF52, RP2040, RISC-V** (any C99 MCU)

> **Motto**: Know your heap.

## What It Solves

- **Random crashes after hours/days** caused by hidden memory leaks
- **Invisible allocations in loops** and drivers that waste memory
- **Finding which call sites allocate the most** (hotspot identification)
- **Post-mortem heap diagnostics** after HardFault/watchdog reset (dump → analyze later)

## Core Principles

✅ **Deterministic** — No timestamps, reproducible state across runs
✅ **Zero malloc inside tracker** — All static tables, predictable memory
✅ **O(1) operations** — Hash table, no iterations on critical paths
✅ **No external dependencies** — Pure C99 library, runs on any MCU

---

## ⚡ Start Here (60 seconds)

**Want to try it right now?** Run the built-in demo:

```bash
cmake -B build && cmake --build build --target snapshot_demo
cd build
./Debug/snapshot_demo        # (or ./snapshot_demo on Linux)
cd ..
./tools/decode_snapshot.sh snapshot_dump.bin
```

Expected output: Allocation list with statistics and CRC validation.

**Then integrate into your own MCU project:**
1. Copy `include/` and `src/` to your project
2. Call `mt_init()` once, then use `malloc()` normally
3. Call `mt_snapshot_write()` to capture heap state
4. Transfer the binary file to PC and run the decoder

See [Integration Guide](docs/INTEGRATION.md) for detailed setup.

---

## How It Works (Snapshot Workflow)

### In Your MCU Code

```c
#include "mt_api.h"

int main(void) {
    mt_init();  // One-time init

    // Use malloc normally (tracker intercepts automatically)
    void* buf = malloc(256);

    // At any point, capture snapshot:
    uint8_t snapshot[2048];
    size_t size = mt_snapshot_write(snapshot, sizeof(snapshot));

    // Send snapshot to PC (UART, SPI, file, etc.)
    uart_send(snapshot, size);
}
```

### On PC (after receiving snapshot file)

```bash
# Decode it
./tools/decode_snapshot.sh snapshot_dump.bin

# Optional: map file IDs to source locations
./tools/decode_snapshot.sh snapshot_dump.bin --filemap filemap.txt
```

Example output:
```
CRC32 Status: [OK] Valid

Header:
  Magic:         MTS1
  Version:       1
  Record Count:  2
  Current Used:  640 bytes
  Peak Used:     640 bytes

Allocations:
Ptr                  Size       File:Line                      Seq
0x0000000100000000   128        src/driver.c:42                2
0x0000000200000000   512        src/driver.c:43                4

Total size represented: 640 bytes
```

---

## For Your Own Project

### Add to build

```bash
# Copy to your project:
cp -r include/ src/ /your/project/

# In your CMakeLists.txt (or makefile):
# - Add include/ to include paths
# - Add src/mt_*.c to sources
```

### Define malloc hooks

**Option 1: Global (debug only)**
```c
#include "mt_wrap.h"  // Replaces malloc/free/realloc
```

**Option 2: Per-module**
```c
#define malloc(x)   mt_malloc((x), __FILE__, __LINE__)
#define free(p)     mt_free((p), __FILE__, __LINE__)
```

**Option 3: ISR-safe**
```c
#define MT_LOCK()   __disable_irq()
#define MT_UNLOCK() __enable_irq()
```

See [INTEGRATION.md](docs/INTEGRATION.md) for all options.

---

## API

**MCU side:**
- `mt_malloc()`, `mt_free()`, `mt_realloc()` — Drop-in malloc replacements
- `mt_snapshot_write()` — Capture heap state as binary
- `mt_stats()` — Get current heap metrics
- `mt_dump_leaks()`, `mt_dump_hotspots()` — Optional text dumps

**PC side:**
- `tools/decode_snapshot.{sh,bat}` — Decode binary snapshots
- Optional `--filemap` for source line locations

---

## Configuration (Optional)

For the snapshot workflow, defaults work well. Only customize if needed:

```c
// include/mt_config.h
#define MT_MAX_ALLOCS 512            // Increase if tracking >512 live allocations
#define MT_LOCK()   __disable_irq()  // If malloc/free called from ISRs
#define MT_UNLOCK() __enable_irq()
```

All options are compile-time with no runtime overhead. See [CONFIG.md](docs/CONFIG.md) for all options.

---

## Known Limits

- **Fragmentation**: Reports as N/A unless you provide platform heap-walk hooks. This is not a bug; heap fragmentation is unknowable without OS support.
- **Drops**: If table fills up (all `MT_MAX_ALLOCS` slots used), new allocations are dropped. Rare in practice; detectable via `stats.table_drops`.
- **ISR malloc**: Requires `MT_LOCK()`/`MT_UNLOCK()` to be defined correctly for your platform.
- **`__FILE__` stability**: Use `-fmacro-prefix-map` (GCC/Clang) if filenames change between builds.

See `docs/KNOWN_LIMITS.md` for all limitations and workarounds.

---

## Public Example

- **examples/snapshot_demo/** — Primary workflow demo (allocate → snapshot → decode)

(Other examples in `examples/selftest_*` are internal unit tests; see them in the source if curious.)

---

## Architecture

**MCU side** (core library, 2.5 KB LOC):
- Hash table tracking with O(1) malloc/free
- Deterministic binary format (MTS1 v1, frozen)
- CRC32/IEEE checksum for corruption detection

**PC side** (tools):
- Python decoder (mt_decode.py) — reads binary, outputs human text
- Optional C11 viewer (compile with `-DMTV1_VIEWER=ON`) — experimental TUI for live streams

**Protocol**:
- MTS1 v1 binary snapshot format (deterministic, little-endian)
- MTV1 v1 streaming frames (optional, for live UART capture)

See `docs/DESIGN.md` for rationale behind key decisions.

---

## Status

**v0.1.0** — Snapshot workflow is stable and demonstrated. Binary format frozen. Core library tested and working.

MTV1 live streaming support is experimental; snapshot workflow is recommended for production use.

---

## License

MIT (see LICENSE)
