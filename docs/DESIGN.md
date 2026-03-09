# Design Decisions — MCU Malloc Tracker

## Why These Decisions?

### 1. O(1) Hash Table Instead of Lists

**Decision**: Open addressing with linear probing + tombstones

**Why**:
- Deterministic performance (no list traversal)
- MCU-friendly (fixed memory, predictable)
- Worst case: O(n) probe length, but O(1) average with good load factor
- No malloc for list nodes (we have zero malloc in tracker)

**Trade-off**: With many tombstones, probe length increases. Acceptable because:
- MCU allocations typically short-lived
- Table size is bounded (MT_MAX_ALLOCS)
- Cleanup deferred (not critical for diagnostics)

---

### 2. Power-of-Two Table Size

**Decision**: Enforce MT_MAX_ALLOCS as power-of-two (compile-time check)

**Why**:
- `idx = hash & (N-1)` is branchless and fast
- Modulo `%` is slower on MCU (division is expensive)
- Deterministic vs. probabilistic clustering patterns

**Implementation**:
```c
#if (MT_MAX_ALLOCS & (MT_MAX_ALLOCS - 1)) != 0
#error "MT_MAX_ALLOCS must be power-of-two"
#endif
```

---

### 3. Sequence Counter Instead of Timestamps

**Decision**: `g_seq` uint32_t monotonic counter, not RTC

**Why**:
- RTC often unavailable on MCU or not synchronized
- Timestamps create non-deterministic state (same allocation order ≠ same state)
- Seq preserves determinism: replay same malloc sequence = same internal state
- Perfect for binary dumps and debugging

**Value**: Age inferred from seq distance, reproducible across runs

---

### 4. Tombstones for Deleted Slots

**Decision**: Mark deleted slots as TOMBSTONE, don't remove

**Why**:
- Avoids probe chain disruption (linear probing requirement)
- O(1) deletion (just mark state byte)
- Reused on next insert (TOMBSTONE → USED)
- No compaction overhead

**Cost**: Tombstone accumulation degrades probe length. Mitigated by:
- Short-lived allocations (MCU typical)
- Optional cleanup in future phase
- Table_tombstones metric for monitoring

---

### 5. Drop-on-Full Policy

**Decision**: When allocation table is full, drop tracking (but keep real allocation)

**Why**:
- Deterministic (no eviction, no LRU logic)
- Rare in practice (MT_MAX_ALLOCS is large, allocations short-lived)
- Detectable: `stats.table_drops` counter
- Better than silently losing allocations or crashing

**Alternative rejected**: Rehashing/resizing (violates "no malloc in tracker")

---

### 6. Fixed-Point Fragmentation (Permille)

**Decision**: `uint16_t frag_permille` (0..1000) instead of `float`

**Why**:
- No FPU needed on MCU (fixed-point arithmetic)
- Deterministic (float has rounding surprises)
- Range 0..1000 is intuitive (per-mille = per-thousand)
- Simple threshold checks (e.g., 200 = 20% fragmentation)

**Formula**:
```c
frag = 1000 - (largest_free * 1000) / total_free
```

---

### 7. Fragmentation N/A by Default

**Decision**: Without platform heap walk, fragmentation is explicitly N/A (not fake)

**Why**:
- Truth is more valuable than guessing
- Embedded systems need real metrics, not estimates
- Platform hooks are optional, not required
- Flag (MT_STAT_FLAG_FRAG_NA) makes it explicit

**User experience**:
- Without heap walk: `frag_health = N/A`, flag set, metrics = 0
- With heap walk: Real fragmentation (if hooks provided)
- Never silently wrong

---

### 8. File ID Hashing (FNV1a)

**Decision**: Hash __FILE__ to uint32_t (FNV1a), not store pointer

**Why**:
- Binary dump size (28 B per record) vs. pointer+string
- Deterministic (hash always same for "foo.c")
- PC decoder requires external --filemap to reverse

**Option 0** (fallback): Store __FILE__ pointer
- Simpler, but breaks binary portability

**Default**: Mode 1 (hash), small binary, PC tool needed to decode

---

### 9. Recursion Guard

**Decision**: `g_inside` flag + MT_LOCK wrapper for malloc hooking

**Why**:
- Prevents malloc-inside-malloc during tracking
- Essential when hooking: `#define malloc mt_malloc`
- MT_LOCK() inside guard (not outside) to prevent lock-malloc recursion

**Example**:
```c
void mt_malloc(...) {
    if (g_inside) return MT_REAL_MALLOC(...);  /* Bypass */
    g_inside = 1;
    MT_LOCK();
    /* ... track ... */
    MT_UNLOCK();
    g_inside = 0;
}
```

---

### 10. Two-Part State Split

**Decision**: `mt_core.c` (O(1) cached counters) vs. `mt_heap_stats.c` (composition)

**Why**:
- `mt_stats()` must be O(1) → return cached counters
- O(n) iteration (table walk) only in dumps/snapshots
- Internal header `mt_internal.h` exposes O(1) getters
- Clean separation: allocation vs. statistics

---

## Rejected Alternatives

### A. Linked List Instead of Hash Table
- **Why rejected**: No malloc for list nodes → must use static list. But with 512+ fixed nodes, not much better than hash table. Hash is faster for find.

### B. Always-Available Fragmentation
- **Why rejected**: Requires heap walk for all MCU types. Better to be honest: "not available" vs. guessing.

### C. Floating-Point Fragmentation Ratio
- **Why rejected**: FPU not always available, non-deterministic, overkill for embedded.

### D. Automatic Rehashing
- **Why rejected**: Would require malloc in tracker (forbidden). Accept tombstones instead.

### E. Timestamps + RTC
- **Why rejected**: RTC often not initialized at early boot, not synchronized. Seq counter better for determinism.

---

## Future Optimizations (Not Phase 3)

1. **Tombstone Cleanup**
   - Periodic or manual compaction of TOMBSTONE slots
   - Trade: O(n) compaction vs. better probe length
   - Worth if tombstone_count > 10% of table

2. **Hotspot Bucketing**
   - Combine hotspots by file (not per line)
   - Reduces table bloat, still useful

3. **Stack Capture**
   - Optional stack trace for each allocation
   - Requires platform-specific unwinding
   - Valuable but expensive (RAM, CPU)

4. **Real-Time Telemetry**
   - Periodic dumps over UART/RTT/SWO
   - Already framework-ready, just needs output

---

## Trade-Offs Summary

| Decision | Pro | Con | Acceptable? |
|----------|-----|-----|-------------|
| Hash table | Fast, deterministic | Tombstone bloom | ✅ Yes |
| Power-of-2 size | Branchless hashing | Compile error if wrong | ✅ Yes |
| Seq counter | Deterministic, no RTC | Not wall-clock time | ✅ Yes |
| Tombstones | O(1) delete | Probe bloom | ✅ Yes |
| Drop-on-full | Deterministic, simple | Rare lost data | ✅ Yes |
| Fixed-point frag | No FPU needed | Not continuous | ✅ Yes |
| Frag N/A default | Honest, no guessing | Needs platform code | ✅ Yes |
| File ID hash | Small binary | Needs decoder | ✅ Yes |
| Recursion guard | Safe malloc hook | Extra flag check | ✅ Yes |
| Split state | O(1) stats | Extra header file | ✅ Yes |

---

## Conclusion

Every decision prioritizes:
1. **Determinism** (reproducible, no surprises)
2. **Honesty** (no fake metrics)
3. **Simplicity** (minimal code, minimal RAM)
4. **MCU-friendliness** (no FPU, no malloc, no OS)

Result: A tracker that embedded developers can trust.
