#!/usr/bin/env python3
"""Quick test of mt_decode.py"""

import sys
import struct

# Create test binary snapshot data
data = bytearray()

# Header (36 bytes)
data.extend(b'MTS1')  # magic
data.extend(struct.pack('<H', 1))  # version
data.extend(struct.pack('<H', 0x0008))  # flags (CRC_OK)
data.extend(struct.pack('<I', 2))  # record_count
data.extend(struct.pack('<I', 640))  # current_used
data.extend(struct.pack('<I', 640))  # peak_used
data.extend(struct.pack('<I', 3))  # total_allocs
data.extend(struct.pack('<I', 1))  # total_frees
data.extend(struct.pack('<I', 5))  # seq
data.extend(struct.pack('<I', 0))  # crc32 (placeholder)

# Records (2x24 bytes)
# Record 1: ptr=0x0000000100000000, size=128, file_id=0x6c9c16d2, line=38, seq=2
data.extend(struct.pack('<Q', 0x0000000100000000))
data.extend(struct.pack('<I', 128))
data.extend(struct.pack('<I', 0x6c9c16d2))
data.extend(struct.pack('<H', 38))
data.extend(struct.pack('<B', 1))  # state
data.extend(struct.pack('<B', 0))  # pad
data.extend(struct.pack('<I', 2))  # seq

# Record 2: ptr=0x0000000200000000, size=512, file_id=0x6c9c16d2, line=46, seq=4
data.extend(struct.pack('<Q', 0x0000000200000000))
data.extend(struct.pack('<I', 512))
data.extend(struct.pack('<I', 0x6c9c16d2))
data.extend(struct.pack('<H', 46))
data.extend(struct.pack('<B', 1))  # state
data.extend(struct.pack('<B', 0))  # pad
data.extend(struct.pack('<I', 4))  # seq

# Calculate CRC32
def crc32_ieee(data_bytes, seed=0xFFFFFFFF):
    """Calculate CRC32 (IEEE, polynomial 0xEDB88320).

    Returns raw CRC without final XOR — final XOR applied once after all data.
    This allows chaining multiple calls with previous result as seed.
    """
    crc = seed
    for byte in data_bytes:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xEDB88320
            else:
                crc >>= 1
    return crc  # Return raw CRC (no final XOR)

# First, set CRC_OK flag (0x0008) before CRC calculation
flags_offset = 6
current_flags = struct.unpack('<H', data[flags_offset:flags_offset+2])[0]
current_flags |= 0x0008  # Set CRC_OK flag
struct.pack_into('<H', data, flags_offset, current_flags)

# CRC header (excluding CRC field) + records
# Process header[0:32] (32 bytes, excludes CRC field at offset 32-35) + records
crc = 0xFFFFFFFF
crc = crc32_ieee(bytes(data[0:32]), crc)  # Process header (0-31), CRC field is zeroed
crc = crc32_ieee(data[36:], crc)  # Process records
crc ^= 0xFFFFFFFF  # Final XOR applied once after all data

# Write CRC back at offset 32
struct.pack_into('<I', data, 32, crc)

# Write file
with open('test_snapshot.bin', 'wb') as f:
    f.write(data)

print(f"Test snapshot written: test_snapshot.bin ({len(data)} bytes)")
print(f"CRC32: 0x{crc:08x}")

