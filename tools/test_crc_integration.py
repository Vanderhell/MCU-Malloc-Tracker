#!/usr/bin/env python3
"""
CRC32 Integration Test — Verify C and Python implementations match on golden vector.

Usage:
  python3 test_crc_integration.py
"""

import struct
import sys
import os


def crc32_ieee(data, seed=0xFFFFFFFF):
    """Pure Python CRC32 (IEEE 0xEDB88320) — matches C implementation."""
    crc = seed
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xEDB88320
            else:
                crc >>= 1
    return crc


def verify_golden_vector():
    """Load golden test vector and verify CRC computation."""

    print("=" * 70)
    print("CRC32 Integration Test — C and Python Synchronization")
    print("=" * 70)

    # Path to golden vector
    golden_path = os.path.join(os.path.dirname(__file__), "_vectors", "phase5_crc_snapshot.bin")

    print(f"\n[1] Loading golden test vector...")
    if not os.path.exists(golden_path):
        print(f"ERROR: Golden vector not found: {golden_path}")
        return False

    with open(golden_path, "rb") as f:
        snapshot = f.read(64)

    if len(snapshot) != 64:
        print(f"ERROR: Expected 64 bytes, got {len(snapshot)}")
        return False

    print(f"    Loaded: {len(snapshot)} bytes (40B header + 24B record)")

    # Extract embedded CRC from header
    embedded_crc = struct.unpack('<I', snapshot[32:36])[0]
    print(f"    Embedded CRC: 0x{embedded_crc:08x}")

    # Compute CRC using Python implementation
    print(f"\n[2] Computing CRC using Python mt_crc32_ieee()...")
    crc = 0xFFFFFFFF
    crc = crc32_ieee(snapshot[0:32], crc)    # Header: bytes 0-31
    crc = crc32_ieee(snapshot[40:64], crc)   # Record: bytes 40-63
    crc ^= 0xFFFFFFFF                        # Final XOR

    print(f"    Computed CRC: 0x{crc:08x}")

    if crc != embedded_crc:
        print(f"\nERROR: CRC mismatch!")
        print(f"    Expected: 0x{embedded_crc:08x}")
        print(f"    Got:      0x{crc:08x}")
        return False

    print(f"    Match: YES [OK]")

    # Verify header structure
    print(f"\n[3] Verifying snapshot structure...")

    magic = snapshot[0:4]
    version = struct.unpack('<H', snapshot[4:6])[0]
    flags = struct.unpack('<H', snapshot[6:8])[0]
    record_count = struct.unpack('<I', snapshot[8:12])[0]
    current_used = struct.unpack('<I', snapshot[12:16])[0]

    checks = [
        ("Magic", magic == b'MTS1', f"{magic.decode('ascii', errors='replace')} == MTS1"),
        ("Version", version == 1, f"{version} == 1"),
        ("Flags", flags & 0x0008, f"0x{flags:04x} has CRC_OK flag"),
        ("Record count", record_count == 1, f"{record_count} == 1"),
        ("Current used", current_used == 64, f"{current_used} == 64"),
    ]

    all_pass = True
    for check_name, result, desc in checks:
        status = "[OK]" if result else "[FAIL]"
        print(f"    {status} {check_name}: {desc}")
        if not result:
            all_pass = False

    if not all_pass:
        return False

    # Show record structure
    print(f"\n[4] Record structure...")
    ptr = struct.unpack('<Q', snapshot[40:48])[0]
    size = struct.unpack('<I', snapshot[48:52])[0]
    file_id = struct.unpack('<I', snapshot[52:56])[0]
    line = struct.unpack('<H', snapshot[56:58])[0]
    state = snapshot[58]
    seq = struct.unpack('<I', snapshot[60:64])[0]

    print(f"    ptr=0x{ptr:016x}")
    print(f"    size={size}")
    print(f"    file_id=0x{file_id:08x}")
    print(f"    line={line}")
    print(f"    state={state} (1=USED)")
    print(f"    seq={seq}")

    print(f"\n" + "=" * 70)
    print(f"RESULT: ALL CHECKS PASSED [OK]")
    print(f"=" * 70)
    print(f"\nC and Python CRC32 implementations are synchronized!")
    print(f"Both compute: 0x{crc:08x}")
    print(f"\nFormat contract is LOCKED and VERIFIED.")

    return True


if __name__ == "__main__":
    success = verify_golden_vector()
    sys.exit(0 if success else 1)
