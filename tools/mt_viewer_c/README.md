# MTV1 Stream Viewer (mt_viewer_c)

A standalone C11 viewer for the MCU Malloc Tracker's MTV1 stream protocol. Zero external dependencies (no ncurses, SDL, ImGui).

## Features

- **Capture**: Read MTV1 streams from serial port (Win32 COM, POSIX /dev/ttyXXX)
- **Replay**: Load and visualize MTV1 streams in an interactive TUI
- **Diff**: Compare two captured streams with detailed reports
- **Export**: Convert MTV1 streams to CSV and JSONL formats
- **TUI Screens**:
  - Overview: Last snapshot stats and top hotspots
  - Timeline: ASCII sparkline visualization of heap usage over time
  - Heatmap: Size buckets or callsite hotspots heatmap
- **Robust Transport**: MTV1 frame parser with automatic resync on corruption

## Building

### From root project directory:

```bash
cmake -B build -DMTV1_VIEWER=ON
cmake --build build --target mt_viewer
cmake --build build --target gen_vectors  # Generate test vectors
ctest                                       # Run tests
```

### Windows (MSVC):

```bash
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -DMTV1_VIEWER=ON ..
cmake --build . --target mt_viewer
ctest
```

### Linux/macOS (GCC/Clang):

```bash
mkdir build
cd build
cmake -DMTV1_VIEWER=ON ..
make
ctest
```

## Usage

### Capture from Serial Port

```bash
mt_viewer_c capture --port COM5 --baud 921600 --out run.bin --seconds 60
```

Options:
- `--port PORT` — COM port (Windows) or device (Linux: /dev/ttyUSB0)
- `--baud BAUD` — Baud rate (default: 921600)
- `--out FILE` — Output MTV1 stream file
- `--seconds N` — Capture for N seconds (0 = until END frame)

### Replay with TUI

```bash
mt_viewer_c replay --in run.bin --filemap mt_filemap.txt
```

**TUI Controls:**
- `1/2/3` — Switch screens (Overview, Timeline, Heatmap)
- `h` — Toggle heatmap mode (Size/Callsite)
- `d` — Show diff view (if loaded)
- `q` — Quit

### Compare Two Runs

```bash
mt_viewer_c diff --a runA.bin --b runB.bin --out report.md --filemap mt_filemap.txt
```

Output report includes:
- Peak heap usage delta
- Timeline slope change
- Number of spikes (>10% sudden increase)
- New/resolved hotspots
- Top hotspots from both runs

### Export to CSV/JSONL

```bash
mt_viewer_c export --in run.bin --csv snapshots.csv --jsonl records.jsonl
```

- CSV: One row per snapshot with timeline data
- JSONL: One JSON object per snapshot with all allocation records

## MTV1 Protocol

MTV1 is a robust, deterministic binary stream format:

```c
/* Frame header (20 bytes, little-endian) */
struct {
    uint8_t  magic[4];      /* "MTV1" */
    uint8_t  version;       /* 1 */
    uint8_t  type;          /* 1=SNAPSHOT_MTS1, 2=TELEMETRY, 3=MARK, 4=END */
    uint16_t flags;
    uint32_t seq;           /* monotonic frame counter */
    uint32_t payload_len;   /* bytes following header */
    uint32_t crc32;         /* CRC32/IEEE (polynomial 0xEDB88320) */
}
```

**Resync:** On parse error, scanner searches byte-by-byte for "MTV1" magic.

**CRC:** IEEE polynomial (0xEDB88320), seed 0xFFFFFFFF, final XOR 0xFFFFFFFF.

## MTS1 Snapshot Format

MTV1 type=1 payload contains an MTS1 snapshot:

```
Header (40 bytes):
  [0:4]  magic "MTS1"
  [4:6]  version (1)
  [6:8]  flags (overflow, frag_na, drops, etc.)
  [8:12] record_count
  [12:16] current_used
  [16:20] peak_used
  [20:24] total_allocs
  [24:28] total_frees
  [28:32] seq
  [32:36] crc32
  [36:40] padding

Records (24 bytes each):
  [0:8]   ptr (u64, little-endian)
  [8:12]  size (u32)
  [12:16] file_id (FNV1a-32 hash of __FILE__)
  [16:18] line (u16)
  [18:19] state (u8, =1 for USED)
  [19:20] _pad
  [20:24] seq (u32, age counter)
```

## Filemap Format

Optional filemap file for resolving file_id to paths:

```
0x12345678 src/main.c
0xABCDEF00 src/heap.c
```

Format: `<0xHASH> <path>`

## Limitations

- **No ncurses:** TUI uses ANSI escape codes + platform-specific key input
- **No RTT stub:** Serial transport only (RTT stub returns EOF immediately)
- **ASCII-only sparklines:** No Unicode block elements (pure ASCII for compatibility)
- **Memory allocation OK:** Dynamic arrays with malloc/realloc (viewer is a PC tool)

## Testing

Three test vectors in `vectors/`:
- `run_clean.bin` — 5 valid snapshots + END
- `run_corrupt_resync.bin` — Frame + 13 garbage bytes + Frame + END (tests resync)
- `run_mixed.bin` — Snapshot + Telemetry + Mark + Snapshot + END

Run tests:
```bash
ctest
```

Individual tests:
```bash
./test_protocol      # Validates resync on corruption
./test_decode_mts1   # Validates MTS1 parsing, ptr monotonic, CRC
./test_diff          # Validates diff on identity (same file vs itself)
```

## Architecture

**Core Library (`libmtv1_core`):**
- `mtv1_crc32.c` — IEEE CRC32 (no lookup table, reflects Phase 5 contract exactly)
- `mtv1_protocol.c` — Frame parser with 4-byte sliding-window resync
- `mtv1_decode_mts1.c` — MTS1 binary snapshot decoder
- `mtv1_model.c` — In-memory run model (timeline, hotspots, aggregation)
- `mtv1_filemap.c` — File ID → path mapping
- `mtv1_render_tui.c` — ANSI-based TUI (no external libs)
- `mtv1_transport_serial_*.c` — Platform-specific serial I/O (Win32 + POSIX)

**Executables:**
- `mt_viewer` — Main CLI tool (capture, replay, diff, export)
- `test_protocol`, `test_decode_mts1`, `test_diff` — Validation tests

## Platform Support

- **Windows 10+** — VT100 terminal mode + COM API
- **Linux** — termios raw mode + /dev/ttyXXX
- **macOS** — POSIX serial (termios)

Build detection automatic via `-DMTV1_PLATFORM_WIN32` compile definition.

## Design Rationale

### No Shared Code with Core Tracker
The viewer re-implements CRC32, not shared with `mt_snapshot.c`. Rationale: viewer is a standalone PC tool that may outlive the tracker library version; it's architecturally cleaner to avoid dependencies.

### CRC32 Contract Duplication
Exact mirror of `mt_crc32_ieee()` polynomial and sequencing. Locked in Phase 5, tested against Python decoder in `mt_decode.py`.

### Resync State Machine
4-byte sliding window approach is the most reliable. Simpler chunked reads would fail when corruption splits the magic boundary.

### Dynamic Memory OK
Spec explicitly permits malloc/free in viewer (it's a PC tool, not MCU firmware).

### No ncurses Dependency
ANSI escape codes work on modern Windows (Win10+, VT100 mode) and all POSIX terminals. Platform-specific key input: `_getch()` on Win32, `tcsetattr` raw + `read()` on POSIX.

## Compliance Checklist

- ✅ C11 only, no external deps
- ✅ No include of core tracker headers (enforced, not linked)
- ✅ MTV1 frame parser with resync
- ✅ MTS1 decoder with CRC verification
- ✅ Deterministic (no timestamps, O(n²) sorts)
- ✅ TUI: 3 screens (Overview, Timeline, Heatmap) + diff
- ✅ All 4 CLI commands (capture, replay, diff, export)
- ✅ Platform support (Win/Linux/macOS)
- ✅ Test vectors + 3 validation tests
- ✅ CMake build with ctest integration

## References

- `docs/mt_dump_format.md` — MTS1 binary format (frozen)
- `tools/mt_decode.py` — Reference Python decoder
- `src/mt_snapshot.c` — CRC32 contract in tracker
