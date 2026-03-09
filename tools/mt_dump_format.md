# MCU Malloc Tracker — Binary Snapshot Format v1

**Format Version**: 1
**Endianness**: Little-endian (Intel)
**Date**: 2026-03-05

---

## Header

**Size**: Fixed 40 bytes (must be known before reading records)

```c
struct mt_snapshot_header_t {
    char     magic[4];          /* "MTS1" (0x4D, 0x54, 0x53, 0x31) */
    uint16_t version;           /* 1 */
    uint16_t flags;             /* See below */
    uint32_t record_count;      /* Number of allocation records */
    uint32_t current_used;      /* Current heap used bytes */
    uint32_t peak_used;         /* Peak heap used bytes */
    uint32_t total_allocs;      /* Total malloc count */
    uint32_t total_frees;       /* Total free count */
    uint32_t seq;               /* Sequence counter at snapshot */
    uint32_t crc32;             /* CRC32 (IEEE, seed 0xFFFFFFFF) */
};
```

**Byte layout**:
```
Offset  Type      Field
0       char[4]   magic         "MTS1"
4       uint16    version       1
6       uint16    flags
8       uint32    record_count
12      uint32    current_used
16      uint32    peak_used
20      uint32    total_allocs
24      uint32    total_frees
28      uint32    seq
32      uint32    crc32
36      (padding to 40)
```

**Flags** (in header.flags):
```
0x0001  MT_SNAP_FLAG_OVERFLOW    — Records truncated (didn't fit in buffer)
0x0002  MT_SNAP_FLAG_FRAG_NA     — Fragmentation data not available
0x0004  MT_SNAP_FLAG_DROPS       — Some allocations were dropped (table full)
0x0008  MT_SNAP_FLAG_CRC_OK      — CRC32 field is valid
```

---

## Records

**Each record**: Fixed 24 bytes

**Only USED allocations are included** (EMPTY/TOMBSTONE excluded)

**Record order**: Sorted by `ptr` (ascending, uint64)

```c
struct mt_alloc_record_t {
    uint64_t ptr;               /* Allocation pointer */
    uint32_t size;              /* Allocation size */
    uint32_t file_id;           /* File hash (FNV1a-32) or file ptr */
    uint16_t line;              /* Source line number */
    uint8_t  state;             /* Always 1 (USED) */
    uint8_t  _pad;              /* Padding */
    uint32_t seq;               /* Sequence number (age indicator) */
};
```

**Byte layout**:
```
Offset  Type      Field
0       uint64    ptr
8       uint32    size
12      uint32    file_id
16      uint16    line
18      uint8     state         (always 1)
19      uint8     _pad
20      uint32    seq
```

**Total record size**: 24 bytes

---

## CRC32 Calculation (LOCKED CONTRACT)

**Algorithm**: IEEE CRC32 (polynomial 0xEDB88320)

**STRICT CONTRACT** (C and Python must match exactly):

```c
/* C Implementation */
uint32_t crc = 0xFFFFFFFF;
crc = mt_crc32_ieee(header_bytes, 32, crc);  /* Header: bytes 0-31 (excludes CRC field at 32-35) */
crc = mt_crc32_ieee(records_bytes, count*24, crc);  /* Records: 24 bytes per allocation */
crc ^= 0xFFFFFFFF;
/* crc is now the final value to store in header.crc32 */
```

```python
# Python Decoder
import binascii
crc = 0xFFFFFFFF
crc = binascii.crc32(data[0:32], crc) & 0xFFFFFFFF  # Header: bytes 0-31
crc = binascii.crc32(data[40:40+count*24], crc) & 0xFFFFFFFF  # Records: starting at byte 40
crc ^= 0xFFFFFFFF
# crc should match header.crc32 (stored at bytes 32-35)
```

**Data Covered**:
1. Header: bytes 0-31 only (32 bytes total, excludes CRC field at 32-35 and padding 36-39)
2. Records: all allocation records (24 bytes each, starting at byte 40 of the file)

**No mutations between calls** — Each call to mt_crc32_ieee processes contiguous bytes with accumulated CRC state.

**Bitwise CRC calculation (no lookup table needed)**:
```
for each byte in data:
    crc ^= byte
    for bit in 0..7:
        if crc & 1:
            crc = (crc >> 1) ^ 0xEDB88320
        else:
            crc >>= 1
return crc  /* Return raw CRC WITHOUT final XOR */
```

**Important**: mt_crc32_ieee() returns the raw polynomial CRC. The final XOR (0xFFFFFFFF) is applied ONCE after all data has been processed, not inside the function. This allows chaining multiple calls with the previous result as the seed.

---

## File Layout Example

```
[Header: 40 bytes]
  Magic:        "MTS1"
  Version:      1
  Flags:        0x0008 (CRC_OK)
  RecordCount:  2
  CurrentUsed:  640
  ...
  CRC32:        0xABCD1234

[Record 0: 24 bytes]
  Ptr:          0x0000000020000000
  Size:         256
  FileID:       0x6c9c16d2
  Line:         42
  State:        1
  Seq:          1

[Record 1: 24 bytes]
  Ptr:          0x0000000020001000
  Size:         384
  FileID:       0x6c9c16d2
  Line:         88
  State:        1
  Seq:          3

Total: 40 + 24 + 24 = 88 bytes
```

---

## Endianness Note

**All multi-byte values are little-endian**.

Example:
- `uint32_t 0x12345678` is stored as bytes: `78 56 34 12`
- `uint64_t 0x0000000012345678` is stored as: `78 56 34 12 00 00 00 00`

**Portable reading** (C code):
```c
uint32_t read_u32_le(const uint8_t* p) {
    return p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
}

uint64_t read_u64_le(const uint8_t* p) {
    return read_u32_le(p) | ((uint64_t)read_u32_le(p+4) << 32);
}
```

---

## Compatibility

**v1 → v2 migration** (future):
- Header size is always known (40 bytes)
- Record size is always known (24 bytes)
- New fields can be appended to header (unused bytes after uint32_t seq)
- Record format is fixed (cannot add fields without breaking v1 readers)

---

## Tools

**Encoder**: `mt_snapshot_write()` (in C, on MCU)

**Decoder**: `tools/mt_decode.py` (Python, on PC)

**Filemap**: `tools/mt_filemap_gen.py` → `mt_filemap.txt` (hash → file mapping)

---

## Validation Checklist

- [ ] File starts with "MTS1"
- [ ] Version == 1
- [ ] Header size == 40 bytes
- [ ] Record size == 24 bytes
- [ ] All records have state == 1 (USED)
- [ ] Records are sorted by ptr (ascending)
- [ ] CRC32 matches (if MT_SNAP_FLAG_CRC_OK set)
- [ ] record_count > 0
- [ ] No records beyond stated count

---

**Last Updated**: Phase 5
**Status**: FROZEN (no changes without version bump)
