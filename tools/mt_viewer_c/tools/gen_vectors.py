#!/usr/bin/env python3
"""Generate MTV1 test vectors for mt_viewer_c tests."""

import struct
import binascii
import sys
import os

def crc32_ieee_update(data, crc):
    """CRC32/IEEE update (reflected polynomial 0xEDB88320)."""
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xEDB88320
            else:
                crc = crc >> 1
    return crc

def crc32_ieee_full(data):
    """CRC32/IEEE full (init, update, final XOR)."""
    crc = 0xFFFFFFFF
    crc = crc32_ieee_update(data, crc)
    crc ^= 0xFFFFFFFF
    return crc

def build_mts1_payload(record_count=0, current_used=0, peak_used=0, seq=1):
    """Build a minimal valid MTS1 snapshot payload."""
    # 40-byte header
    header = bytearray(40)
    header[0:4] = b'MTS1'
    struct.pack_into('<H', header, 4, 1)           # version = 1
    struct.pack_into('<H', header, 6, 0)           # flags = 0
    struct.pack_into('<I', header, 8, record_count)
    struct.pack_into('<I', header, 12, current_used)
    struct.pack_into('<I', header, 16, peak_used)
    struct.pack_into('<I', header, 20, record_count)  # total_allocs = record_count for simplicity
    struct.pack_into('<I', header, 24, 0)          # total_frees = 0
    struct.pack_into('<I', header, 28, seq)        # seq
    # crc32 field (offset 32) left as 0 for now

    # Build records (if any)
    records = bytearray()
    for i in range(record_count):
        rec = bytearray(24)
        struct.pack_into('<Q', rec, 0, 0x1000 + i * 0x1000)   # ptr: spaced out
        struct.pack_into('<I', rec, 8, 128)        # size = 128
        struct.pack_into('<I', rec, 12, 0x12345678)  # file_id (dummy)
        struct.pack_into('<H', rec, 16, 10 + i)    # line
        rec[18] = 1                                 # state = USED
        rec[19] = 0                                 # _pad
        struct.pack_into('<I', rec, 20, 1 + i)     # seq
        records.extend(rec)

    # Calculate CRC over header[0:32] + records
    crc = 0xFFFFFFFF
    crc = crc32_ieee_update(bytes(header[0:32]), crc)
    crc = crc32_ieee_update(bytes(records), crc)
    crc ^= 0xFFFFFFFF

    # Store CRC
    struct.pack_into('<I', header, 32, crc)

    return bytes(header) + bytes(records)

def build_mtv1_frame(frame_type, seq, payload):
    """Build a complete MTV1 frame."""
    header = bytearray(20)
    header[0:4] = b'MTV1'
    header[4] = 1                          # version = 1
    header[5] = frame_type
    struct.pack_into('<H', header, 6, 0)  # flags = 0
    struct.pack_into('<I', header, 8, seq)
    struct.pack_into('<I', header, 12, len(payload))
    # crc32 field (offset 16-19) left as 0 for CRC calculation

    # Calculate CRC: full 20-byte header (with crc=0) + payload
    crc = 0xFFFFFFFF
    crc = crc32_ieee_update(bytes(header), crc)  # all 20 bytes of header
    crc = crc32_ieee_update(payload, crc)
    crc ^= 0xFFFFFFFF

    struct.pack_into('<I', header, 16, crc)

    return bytes(header) + payload

def gen_run_clean():
    """Generate: 5 valid snapshots + END frame."""
    frames = bytearray()
    for i in range(5):
        current = 1000 + i*100
        peak = 1000 + (4-i)*100  # Peak decreases or stays same
        payload = build_mts1_payload(record_count=(i % 2), current_used=current, peak_used=peak, seq=i+1)
        frame = build_mtv1_frame(1, i, payload)  # type=1 is SNAPSHOT_MTS1
        frames.extend(frame)

    # END frame (type=4, no payload)
    end_frame = build_mtv1_frame(4, 5, b'')
    frames.extend(end_frame)

    return bytes(frames)

def gen_run_corrupt_resync():
    """Generate: valid frame + 13 garbage bytes + valid frame + END."""
    frames = bytearray()

    # First valid frame
    payload1 = build_mts1_payload(record_count=1, current_used=2000, seq=1)
    frame1 = build_mtv1_frame(1, 1, payload1)
    frames.extend(frame1)

    # 13 garbage bytes
    frames.extend(b'\xDE\xAD\xBE\xEF\x00\x11\x22\x33\x44\x55\x66\x77\x88')

    # Second valid frame (different seq)
    payload2 = build_mts1_payload(record_count=0, current_used=1500, seq=2)
    frame2 = build_mtv1_frame(1, 2, payload2)
    frames.extend(frame2)

    # END frame
    end_frame = build_mtv1_frame(4, 3, b'')
    frames.extend(end_frame)

    return bytes(frames)

def gen_run_mixed():
    """Generate: snapshot + telemetry + mark + snapshot + END."""
    frames = bytearray()

    # Snapshot
    payload1 = build_mts1_payload(record_count=1, current_used=1000, seq=1)
    frame1 = build_mtv1_frame(1, 1, payload1)  # SNAPSHOT_MTS1
    frames.extend(frame1)

    # Telemetry
    telem_text = b'heap ok'
    frame2 = build_mtv1_frame(2, 2, telem_text)  # TELEMETRY_TEXT
    frames.extend(frame2)

    # Mark
    mark_text = b'bootup'
    frame3 = build_mtv1_frame(3, 3, mark_text)  # MARK_TEXT
    frames.extend(frame3)

    # Snapshot
    payload2 = build_mts1_payload(record_count=2, current_used=1200, seq=4)
    frame4 = build_mtv1_frame(1, 4, payload2)
    frames.extend(frame4)

    # END
    end_frame = build_mtv1_frame(4, 5, b'')
    frames.extend(end_frame)

    return bytes(frames)

def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else '.'

    print(f"Generating MTV1 test vectors to {out_dir}/")

    with open(os.path.join(out_dir, 'run_clean.bin'), 'wb') as f:
        f.write(gen_run_clean())
    print("  run_clean.bin (5 snapshots + END)")

    with open(os.path.join(out_dir, 'run_corrupt_resync.bin'), 'wb') as f:
        f.write(gen_run_corrupt_resync())
    print("  run_corrupt_resync.bin (frame + garbage + frame + END)")

    with open(os.path.join(out_dir, 'run_mixed.bin'), 'wb') as f:
        f.write(gen_run_mixed())
    print("  run_mixed.bin (snapshot + telemetry + mark + snapshot + END)")

if __name__ == '__main__':
    main()
