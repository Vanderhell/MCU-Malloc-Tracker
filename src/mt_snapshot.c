/*
 * MCU Malloc Tracker - Binary Snapshot (Phase 5)
 *
 * Deterministic binary dump of heap state.
 * Wire format: 36-byte header + 24-byte records, all little-endian.
 * CRC32/IEEE covers header bytes 0..31 followed by all record bytes.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "../include/mt_config.h"
#include "../include/mt_types.h"
#include "../include/mt_api.h"
#include "../include/mt_internal.h"
#include "../include/mt_hotspots.h"
#include "../include/mt_crc32_ieee.h"

#if MT_ENABLE_SNAPSHOT

enum {
    MT_SNAPSHOT_HEADER_WIRE_SIZE = 36u,
    MT_SNAPSHOT_RECORD_WIRE_SIZE = 24u
};

static void mt_write_u16le(uint8_t* dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void mt_write_u32le(uint8_t* dst, uint32_t value)
{
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
    dst[2] = (uint8_t)((value >> 16) & 0xFFu);
    dst[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static void mt_write_u64le(uint8_t* dst, uintptr_t value)
{
    uint64_t v = (uint64_t)value;
    dst[0] = (uint8_t)(v & 0xFFu);
    dst[1] = (uint8_t)((v >> 8) & 0xFFu);
    dst[2] = (uint8_t)((v >> 16) & 0xFFu);
    dst[3] = (uint8_t)((v >> 24) & 0xFFu);
    dst[4] = (uint8_t)((v >> 32) & 0xFFu);
    dst[5] = (uint8_t)((v >> 40) & 0xFFu);
    dst[6] = (uint8_t)((v >> 48) & 0xFFu);
    dst[7] = (uint8_t)((v >> 56) & 0xFFu);
}

size_t mt_snapshot_write(uint8_t* out, size_t out_cap)
{
    if (!out || out_cap < MT_SNAPSHOT_HEADER_WIRE_SIZE) {
        return 0;
    }

    mt_heap_stats_t stats = mt_stats();
    const mt_alloc_rec_t* alloc_table = mt__alloc_table();
    uint32_t table_cap = mt__alloc_table_cap();

    uint16_t used_indices[MT_MAX_ALLOCS] = {0};
    uint32_t used_count = 0;

    for (uint32_t i = 0; i < table_cap; i++) {
        if (alloc_table[i].state == MT_STATE_USED && used_count < MT_MAX_ALLOCS) {
            used_indices[used_count++] = (uint16_t)i;
        }
    }

    for (uint32_t i = 0; i < used_count; i++) {
        for (uint32_t j = i + 1; j < used_count; j++) {
            uintptr_t ptr_i = alloc_table[used_indices[i]].ptr;
            uintptr_t ptr_j = alloc_table[used_indices[j]].ptr;
            if (ptr_j < ptr_i) {
                uint16_t tmp = used_indices[i];
                used_indices[i] = used_indices[j];
                used_indices[j] = tmp;
            }
        }
    }

    size_t header_bytes = MT_SNAPSHOT_HEADER_WIRE_SIZE;
    size_t record_bytes = MT_SNAPSHOT_RECORD_WIRE_SIZE;
    size_t needed = header_bytes + (used_count * record_bytes);

    uint32_t final_count = used_count;
    uint16_t flags = 0;

    if (needed > out_cap) {
        final_count = (uint32_t)((out_cap - header_bytes) / record_bytes);
        flags |= MT_SNAPSHOT_FLAG_OVERFLOW;
    }

    if (stats.flags & MT_STAT_FLAG_FRAG_NA) {
        flags |= MT_SNAPSHOT_FLAG_FRAG_NA;
    }
    if (stats.flags & MT_STAT_FLAG_DROPS) {
        flags |= MT_SNAPSHOT_FLAG_DROPS;
    }
    flags |= MT_SNAPSHOT_FLAG_CRC_OK;

    memset(out, 0, header_bytes);
    out[0] = 'M';
    out[1] = 'T';
    out[2] = 'S';
    out[3] = '1';
    mt_write_u16le(out + 4, 1u);
    mt_write_u16le(out + 6, flags);
    mt_write_u32le(out + 8, final_count);
    mt_write_u32le(out + 12, stats.current_used);
    mt_write_u32le(out + 16, stats.peak_used);
    mt_write_u32le(out + 20, stats.total_allocs);
    mt_write_u32le(out + 24, stats.total_frees);
    mt_write_u32le(out + 28, mt__seq_now());
    mt_write_u32le(out + 32, 0u);

    uint8_t* record_ptr = out + header_bytes;
    for (uint32_t i = 0; i < final_count; i++) {
        const mt_alloc_rec_t* rec = &alloc_table[used_indices[i]];
        uint8_t* p = record_ptr + (i * record_bytes);
        mt_write_u64le(p + 0, rec->ptr);
        mt_write_u32le(p + 8, rec->size);
        mt_write_u32le(p + 12, rec->file_id);
        mt_write_u16le(p + 16, rec->line);
        p[18] = (uint8_t)MT_STATE_USED;
        p[19] = 0u;
        mt_write_u32le(p + 20, rec->seq);
    }

    uint32_t crc = 0xFFFFFFFFu;
    crc = mt_crc32_ieee_update(out, header_bytes - 4u, crc);
    crc = mt_crc32_ieee_update(record_ptr, (size_t)final_count * record_bytes, crc);
    crc ^= 0xFFFFFFFFu;
    mt_write_u32le(out + 32, crc);

    return header_bytes + ((size_t)final_count * record_bytes);
}

#endif /* MT_ENABLE_SNAPSHOT */
