#!/usr/bin/env python3
"""
MCU Malloc Tracker - Binary Snapshot Decoder

Reads binary snapshot (MTS1 format) and produces human-readable output.
Supports filemap for file_id → file:line translation.

Usage:
  python mt_decode.py dump.bin [--filemap mt_filemap.txt] [--json out.json] [--top N]
"""

import struct
import sys
import argparse
from pathlib import Path

# ============================================================================
# Binary Format Constants
# ============================================================================

MAGIC = b'MTS1'
VERSION = 1
HEADER_SIZE = 36
RECORD_SIZE = 24

# Flags
FLAG_OVERFLOW = 0x0001
FLAG_FRAG_NA = 0x0002
FLAG_DROPS = 0x0004
FLAG_CRC_OK = 0x0008


def crc32_ieee(data, seed=0xFFFFFFFF):
    """Calculate CRC32 (IEEE, polynomial 0xEDB88320).

    Returns raw CRC without final XOR — final XOR applied once after all data.
    This allows chaining multiple calls with previous result as seed.
    """
    crc = seed
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xEDB88320
            else:
                crc >>= 1
    return crc  # Return raw CRC (no final XOR)


def read_u64_le(data, offset):
    """Read uint64_t in little-endian."""
    return struct.unpack('<Q', data[offset:offset+8])[0]


def read_u32_le(data, offset):
    """Read uint32_t in little-endian."""
    return struct.unpack('<I', data[offset:offset+4])[0]


def read_u16_le(data, offset):
    """Read uint16_t in little-endian."""
    return struct.unpack('<H', data[offset:offset+2])[0]


def parse_header(data):
    """Parse snapshot header."""
    if len(data) < HEADER_SIZE:
        raise ValueError("Data too small for header")

    magic = data[0:4]
    if magic != MAGIC:
        raise ValueError(f"Invalid magic: {magic}")

    version = read_u16_le(data, 4)
    flags = read_u16_le(data, 6)
    record_count = read_u32_le(data, 8)
    current_used = read_u32_le(data, 12)
    peak_used = read_u32_le(data, 16)
    total_allocs = read_u32_le(data, 20)
    total_frees = read_u32_le(data, 24)
    seq = read_u32_le(data, 28)
    crc32_value = read_u32_le(data, 32)

    return {
        'magic': magic,
        'version': version,
        'flags': flags,
        'record_count': record_count,
        'current_used': current_used,
        'peak_used': peak_used,
        'total_allocs': total_allocs,
        'total_frees': total_frees,
        'seq': seq,
        'crc32': crc32_value,
    }


def parse_record(data, offset):
    """Parse single allocation record."""
    if len(data) < offset + RECORD_SIZE:
        return None

    ptr = read_u64_le(data, offset)
    size = read_u32_le(data, offset + 8)
    file_id = read_u32_le(data, offset + 12)
    line = read_u16_le(data, offset + 16)
    state = data[offset + 18]
    seq = read_u32_le(data, offset + 20)

    return {
        'ptr': ptr,
        'size': size,
        'file_id': file_id,
        'line': line,
        'state': state,
        'seq': seq,
    }


def load_filemap(filemap_path):
    """Load filemap (hash -> filepath mapping)."""
    filemap = {}
    try:
        with open(filemap_path, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                parts = line.split(None, 1)
                if len(parts) == 2:
                    hash_str = parts[0]
                    filepath = parts[1]
                    try:
                        hash_val = int(hash_str, 16)
                        filemap[hash_val] = filepath
                    except ValueError:
                        pass
    except FileNotFoundError:
        print(f"Warning: filemap '{filemap_path}' not found", file=sys.stderr)

    return filemap


def format_file_and_line(file_id, line, filemap):
    """Format file_id and line (with filemap lookup if available)."""
    if filemap and file_id in filemap:
        return f"{filemap[file_id]}:{line}"
    else:
        return f"0x{file_id:08x}:{line}"


def dump_text(header, records, filemap=None, top_n=None):
    """Output human-readable text dump."""
    print("=" * 70)
    print("MCU Malloc Tracker - Binary Snapshot Decoder")
    print("=" * 70)

    print(f"\nHeader:")
    print(f"  Magic:         {header['magic'].decode('ascii', errors='replace')}")
    print(f"  Version:       {header['version']}")
    print(f"  Flags:         0x{header['flags']:04x}")
    print(f"    Overflow:    {'Yes' if header['flags'] & FLAG_OVERFLOW else 'No'}")
    print(f"    Frag N/A:    {'Yes' if header['flags'] & FLAG_FRAG_NA else 'No'}")
    print(f"    Drops:       {'Yes' if header['flags'] & FLAG_DROPS else 'No'}")
    print(f"    CRC Valid:   {'Yes' if header['flags'] & FLAG_CRC_OK else 'No'}")
    print(f"  Record Count:  {header['record_count']}")
    print(f"  CRC32:         0x{header['crc32']:08x}")

    print(f"\nStatistics:")
    print(f"  Current Used:  {header['current_used']} bytes")
    print(f"  Peak Used:     {header['peak_used']} bytes")
    print(f"  Total Allocs:  {header['total_allocs']}")
    print(f"  Total Frees:   {header['total_frees']}")
    print(f"  Seq Counter:   {header['seq']}")

    if not records:
        print(f"\n(no records)")
        return

    # Show first N records
    limit = top_n if top_n else len(records)
    print(f"\nAllocations (showing {min(limit, len(records))} of {len(records)}):")
    print(f"{'Ptr':<20} {'Size':<10} {'File:Line':<30} {'Seq':<6}")
    print("-" * 70)

    for i, rec in enumerate(records[:limit]):
        ptr_str = f"0x{rec['ptr']:016x}"
        size_str = str(rec['size'])
        file_line = format_file_and_line(rec['file_id'], rec['line'], filemap)
        seq_str = str(rec['seq'])
        print(f"{ptr_str:<20} {size_str:<10} {file_line:<30} {seq_str:<6}")

    if len(records) > limit:
        print(f"... and {len(records) - limit} more")

    print(f"\nTotal size represented: {sum(r['size'] for r in records)} bytes")


def main():
    parser = argparse.ArgumentParser(
        description='Decode MCU Malloc Tracker binary snapshot'
    )
    parser.add_argument('dump', help='Binary snapshot file (MTS1)')
    parser.add_argument('--filemap', help='Filemap file (hash -> filepath)')
    parser.add_argument('--json', help='JSON output file (optional)')
    parser.add_argument('--top', type=int, default=10, help='Top N records to show (default 10)')

    args = parser.parse_args()

    # Read binary file
    try:
        with open(args.dump, 'rb') as f:
            data = f.read()
    except FileNotFoundError:
        print(f"Error: File '{args.dump}' not found", file=sys.stderr)
        sys.exit(1)

    # Parse header
    try:
        header = parse_header(data)
    except ValueError as e:
        print(f"Error parsing header: {e}", file=sys.stderr)
        sys.exit(1)

    # Parse records
    records = []
    for i in range(header['record_count']):
        offset = HEADER_SIZE + i * RECORD_SIZE
        rec = parse_record(data, offset)
        if rec:
            records.append(rec)

    # Load filemap if provided
    filemap = {}
    if args.filemap:
        filemap = load_filemap(args.filemap)

    # Verify CRC if flag is set
    if header['flags'] & FLAG_CRC_OK:
        # Recompute CRC (strict contract: final XOR applied once at very end)
        # The CRC is calculated over header (excluding CRC field) + records
        header_data = data[0:HEADER_SIZE - 4]  # Process all header except CRC field (bytes 0-31)
        records_data = data[HEADER_SIZE:HEADER_SIZE + header['record_count'] * RECORD_SIZE]

        computed_crc = 0xFFFFFFFF
        computed_crc = crc32_ieee(bytes(header_data), computed_crc)  # Process header
        computed_crc = crc32_ieee(records_data, computed_crc)  # Process records
        computed_crc ^= 0xFFFFFFFF  # Final XOR applied once after all data

        if computed_crc == header['crc32']:
            crc_status = "[OK] Valid"
        else:
            crc_status = f"[FAIL] Mismatch (computed: 0x{computed_crc:08x})"
    else:
        crc_status = "[N/A] Not present"

    print(f"CRC32 Status: {crc_status}\n")

    # Output text dump
    dump_text(header, records, filemap, args.top)

    # Optional: JSON output
    if args.json:
        import json
        output = {
            'header': {
                'magic': header['magic'].decode('ascii'),
                'version': header['version'],
                'flags': header['flags'],
                'record_count': header['record_count'],
                'current_used': header['current_used'],
                'peak_used': header['peak_used'],
                'total_allocs': header['total_allocs'],
                'total_frees': header['total_frees'],
                'seq': header['seq'],
                'crc32': f"0x{header['crc32']:08x}",
            },
            'records': [
                {
                    'ptr': f"0x{r['ptr']:016x}",
                    'size': r['size'],
                    'file_id': f"0x{r['file_id']:08x}",
                    'line': r['line'],
                    'seq': r['seq'],
                }
                for r in records
            ]
        }

        with open(args.json, 'w') as f:
            json.dump(output, f, indent=2)
        print(f"\nJSON output written to: {args.json}")


if __name__ == '__main__':
    main()
