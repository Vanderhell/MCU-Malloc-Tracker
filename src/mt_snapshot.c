/*
 * MCU Malloc Tracker - Binary Snapshot (Phase 5)
 *
 * Deterministic binary dump of heap state.
 * Format: Header (36B) + Records (24B each)
 * CRC32 for integrity
 * No malloc
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

/* ============================================================================
 * SNAPSHOT WRITE
 * ============================================================================ */

/**
 * mt_snapshot_write(out, out_cap)
 * Write deterministic binary snapshot of heap state.
 *
 * Returns: Number of bytes written, or 0 on failure
 */
size_t mt_snapshot_write(uint8_t* out, size_t out_cap)
{
    if (!out || out_cap < sizeof(mt_snapshot_header_t)) {
        return 0;
    }

    /* Get current stats */
    mt_heap_stats_t stats = mt_stats();
    const mt_alloc_rec_t* alloc_table = mt__alloc_table();
    uint32_t table_cap = mt__alloc_table_cap();

    /* Collect USED record indices */
    uint16_t used_indices[MT_MAX_ALLOCS];
    uint32_t used_count = 0;

    for (uint32_t i = 0; i < table_cap; i++) {
        if (alloc_table[i].state == MT_STATE_USED) {
            used_indices[used_count++] = (uint16_t)i;
        }
    }

    /* Sort indices by ptr (ascending) — O(n²) bubble sort */
    for (uint32_t i = 0; i < used_count; i++) {
        for (uint32_t j = i + 1; j < used_count; j++) {
            uint64_t ptr_i = alloc_table[used_indices[i]].ptr;
            uint64_t ptr_j = alloc_table[used_indices[j]].ptr;

            if (ptr_j < ptr_i) {
                /* Swap */
                uint16_t tmp = used_indices[i];
                used_indices[i] = used_indices[j];
                used_indices[j] = tmp;
            }
        }
    }

    /* Calculate required space */
    size_t record_bytes = 24;  /* Fixed record size */
    size_t header_bytes = sizeof(mt_snapshot_header_t);
    size_t needed = header_bytes + (used_count * record_bytes);

    uint32_t final_count = used_count;
    uint32_t overflow = 0;

    if (needed > out_cap) {
        /* Truncate records to fit */
        final_count = (out_cap - header_bytes) / record_bytes;
        overflow = 1;
    }

    /* Initialize header */
    memset(out, 0, header_bytes);
    mt_snapshot_header_t* hdr = (mt_snapshot_header_t*)out;

    hdr->magic[0] = 'M';
    hdr->magic[1] = 'T';
    hdr->magic[2] = 'S';
    hdr->magic[3] = '1';
    hdr->version = 1;
    hdr->flags = 0;

    if (overflow) {
        hdr->flags |= MT_SNAPSHOT_FLAG_OVERFLOW;
    }

    if (stats.flags & MT_STAT_FLAG_FRAG_NA) {
        hdr->flags |= 0x0002;  /* MT_SNAP_FLAG_FRAG_NA */
    }

    if (stats.flags & MT_STAT_FLAG_DROPS) {
        hdr->flags |= 0x0004;  /* MT_SNAP_FLAG_DROPS */
    }

    hdr->record_count = final_count;
    hdr->current_used = stats.current_used;
    hdr->peak_used = stats.peak_used;
    hdr->total_allocs = stats.total_allocs;
    hdr->total_frees = stats.total_frees;
    hdr->seq = mt__seq_now();
    hdr->crc32 = 0;  /* Will compute later */
    hdr->flags |= 0x0008;  /* MT_SNAP_FLAG_CRC_OK — set BEFORE CRC calculation */

    /* Write records */
    uint8_t* record_ptr = out + header_bytes;

    for (uint32_t i = 0; i < final_count; i++) {
        const mt_alloc_rec_t* rec = &alloc_table[used_indices[i]];

        /* Little-endian write (portable) */
        uint8_t* p = record_ptr + (i * 24);

        /* ptr (uint64_t) */
        p[0] = (rec->ptr >> 0) & 0xFF;
        p[1] = (rec->ptr >> 8) & 0xFF;
        p[2] = (rec->ptr >> 16) & 0xFF;
        p[3] = (rec->ptr >> 24) & 0xFF;
        p[4] = (rec->ptr >> 32) & 0xFF;
        p[5] = (rec->ptr >> 40) & 0xFF;
        p[6] = (rec->ptr >> 48) & 0xFF;
        p[7] = (rec->ptr >> 56) & 0xFF;

        /* size (uint32_t) */
        p[8] = (rec->size >> 0) & 0xFF;
        p[9] = (rec->size >> 8) & 0xFF;
        p[10] = (rec->size >> 16) & 0xFF;
        p[11] = (rec->size >> 24) & 0xFF;

        /* file_id (uint32_t) */
        p[12] = (rec->file_id >> 0) & 0xFF;
        p[13] = (rec->file_id >> 8) & 0xFF;
        p[14] = (rec->file_id >> 16) & 0xFF;
        p[15] = (rec->file_id >> 24) & 0xFF;

        /* line (uint16_t) */
        p[16] = (rec->line >> 0) & 0xFF;
        p[17] = (rec->line >> 8) & 0xFF;

        /* state (uint8_t) — always 1 */
        p[18] = 1;

        /* _pad (uint8_t) */
        p[19] = 0;

        /* seq (uint32_t) */
        p[20] = (rec->seq >> 0) & 0xFF;
        p[21] = (rec->seq >> 8) & 0xFF;
        p[22] = (rec->seq >> 16) & 0xFF;
        p[23] = (rec->seq >> 24) & 0xFF;
    }

    /* Calculate CRC32 (strict contract: final XOR applied once at very end) */
    uint32_t crc = 0xFFFFFFFFu;
    crc = mt_crc32_ieee_update(out, header_bytes - 4, crc);  /* Process header excluding CRC field */

    size_t records_size = final_count * 24;
    crc = mt_crc32_ieee_update(record_ptr, records_size, crc);  /* Process records, continuing from header CRC */

    crc ^= 0xFFFFFFFFu;  /* Final XOR applied once after all data */

    /* Write CRC back to header */
    hdr->crc32 = crc;

    /* Return total bytes written */
    return header_bytes + (final_count * 24);
}

#endif /* MT_ENABLE_SNAPSHOT */
