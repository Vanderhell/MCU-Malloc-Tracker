# Changelog

All notable changes to MCU Malloc Tracker are documented here.

## [0.1.0] — 2026-03-09

Initial release with core features for deterministic heap diagnostics on bare-metal MCU.

### Added

#### Core Tracker
- **O(1) Hash Table Tracking** — Malloc/free/realloc interception with deterministic hash table (linear probing, no rehashing)
- **Zero Internal Allocation** — All tracking data in static arrays, no malloc inside tracker
- **ISR Safety** — Optional MT_LOCK/MT_UNLOCK for interrupt-safe operation
- **Recursion Guard** — Prevents tracker re-entrancy

#### Statistics & Monitoring
- **Extended Heap Statistics** — Current used, peak used, allocation counts, free counts, realloc counts
- **Allocation Hotspots** — Identifies which source code locations (file:line) allocate the most
- **Fragmentation Analysis** — Two modes:
  - Real fragmentation (with platform heap-walk hooks)
  - Honest N/A reporting (without platform support)
- **Health Status Flags** — Explicit signaling of table saturation, fragmentation status, anomalies

#### Binary Snapshot Format
- **MTS1 v1 Format** — Deterministic, little-endian, fixed-size binary snapshots
- **CRC32/IEEE Validation** — Corruption detection for stored snapshots
- **Pointer Ordering** — Stable, deterministic snapshot ordering by allocation address
- **File ID Hashing** — Configurable: store file pointers (MT_FILE_ID_MODE=0) or compact 32-bit hashes (MT_FILE_ID_MODE=1)

#### Output & Debugging
- **Text Dumps** — Human-readable leak listings, hotspot reports, statistics
- **PC Decoder Tool** — Python script (mt_decode.py) to decode MTS1 snapshots on desktop
- **Filemap Support** — Map file ID hashes back to source code locations for analysis
- **Telemetry Hooks** — Optional periodic snapshots of heap state

#### Documentation
- **README.md** — 60-second quickstart with workflow example
- **INTEGRATION.md** — Multiple integration strategies (global wrap, per-module, ISR-safe)
- **CONFIG.md** — All 15 configuration options documented
- **DESIGN.md** — Rationale for key architectural decisions
- **KNOWN_LIMITS.md** — Honest documentation of limitations and workarounds
- **FILEMAP.md** — File ID mapping and stable filename handling

#### Configuration
- Power-of-two compile-time checking for MT_MAX_ALLOCS
- Configurable allocation table size (default 512)
- Configurable hotspot detection table (default 64 entries)
- Feature flags for snapshot, hotspots, leak dump, telemetry
- Platform abstraction for heap-walk hooks

#### Examples & Tests
- **snapshot_demo** — Core workflow: allocate, snapshot, decode
- **Selftest Suite** — Unit tests for core tracker, stats, hotspots, snapshot format, CRC validation
- **CRC Integration Test** — Verifies C and Python CRC implementations match
- **Test Vectors** — Golden reference data for format validation

### Platforms
- **Tested:** MSVC 2022 (Windows)
- **Compatible:** GCC/Clang (C99, little-endian assumed)
- **Target MCU:** STM32, ESP32, nRF52, RP2040, RISC-V (any C99 bare-metal platform)

### Known Limitations
- Fragmentation is N/A without platform heap-walk hooks (honest, not hidden)
- Allocation table saturation drops new allocations (detectable via stats)
- Linear probing performance degrades with many tombstones (acceptable for small tables)
- ISR malloc requires explicit MT_LOCK/MT_UNLOCK implementation
- `__FILE__` string variability across builds (mitigation: `-fmacro-prefix-map` in GCC/Clang)

### License
MIT

---

## Future Roadmap (Planned)

### MTV1 Live Streaming (v0.2.0)
- Real-time UART streaming of heap events
- C11 viewer TUI for live heap analysis
- Cross-platform serial transport (Win32, POSIX)

### Enhanced Fragmentation (v0.3.0)
- Default implementations for STM32, ESP32, nRF52
- Visual fragmentation heatmaps
- Predictive warnings for upcoming saturation

### Performance Profiling (v0.4.0)
- Call graph analysis of hottest allocators
- Temporal analysis (allocation/free patterns over time)
- Cluster detection for related allocations

---

**Motto:** Know your heap.
