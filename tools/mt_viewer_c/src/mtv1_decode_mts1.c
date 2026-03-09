#include "mtv1_decode_mts1.h"
#include "mtv1_crc32.h"
#include "mtv1_util.h"
#include <string.h>
#include <stdlib.h>

/* ===== Little-endian readers (for consistency) ===== */

uint16_t mts1_read_u16le(const uint8_t* p) {
    return util_read_u16le(p);
}

uint32_t mts1_read_u32le(const uint8_t* p) {
    return util_read_u32le(p);
}

uint64_t mts1_read_u64le(const uint8_t* p) {
    return util_read_u64le(p);
}

/* ===== MTS1 parsing ===== */

int mts1_parse(const uint8_t* payload, uint32_t payload_len,
               mts1_snapshot_t* out) {
    /* Minimum: 40-byte header */
    if (payload_len < 40)
        return -1;

    mts1_header_t* hdr = &out->hdr;

    /* Parse header (first 40 bytes) */
    if (payload[0] != 'M' || payload[1] != 'T' ||
        payload[2] != 'S' || payload[3] != '1')
        return -1; /* bad magic */

    memcpy(hdr->magic, payload, 4);
    hdr->version = mts1_read_u16le(payload + 4);
    hdr->flags = mts1_read_u16le(payload + 6);
    hdr->record_count = mts1_read_u32le(payload + 8);
    hdr->current_used = mts1_read_u32le(payload + 12);
    hdr->peak_used = mts1_read_u32le(payload + 16);
    hdr->total_allocs = mts1_read_u32le(payload + 20);
    hdr->total_frees = mts1_read_u32le(payload + 24);
    hdr->seq = mts1_read_u32le(payload + 28);
    hdr->crc32 = mts1_read_u32le(payload + 32);

    /* Validate version */
    if (hdr->version != 1)
        return -1;

    /* Validate payload size: must be at least header, may have records */
    uint32_t expected_size = 40 + hdr->record_count * 24;
    if (payload_len < expected_size)
        return -1; /* payload too short */

    /* Allocate records */
    out->records = (mts1_record_t*)malloc(hdr->record_count * sizeof(mts1_record_t));
    if (!out->records && hdr->record_count > 0)
        return -1;

    /* Parse records */
    const uint8_t* rec_ptr = payload + 40;
    for (uint32_t i = 0; i < hdr->record_count; i++) {
        mts1_record_t* rec = &out->records[i];
        rec->ptr = mts1_read_u64le(rec_ptr + 0);
        rec->size = mts1_read_u32le(rec_ptr + 8);
        rec->file_id = mts1_read_u32le(rec_ptr + 12);
        rec->line = mts1_read_u16le(rec_ptr + 16);
        rec->state = rec_ptr[18];
        rec->_pad = rec_ptr[19];
        rec->seq = mts1_read_u32le(rec_ptr + 20);
        rec_ptr += 24;
    }

    out->crc_ok = -1; /* not yet verified */
    return 0;
}

int mts1_verify_crc(const uint8_t* payload, uint32_t payload_len,
                    const mts1_snapshot_t* snap) {
    /* Recompute CRC: header[0:32] + records, final XOR once */
    if (payload_len < 40)
        return 0;

    uint32_t crc = 0xFFFFFFFFu;
    crc = mtv1_crc32_update(payload, 32, crc);           /* header[0:32] */
    crc = mtv1_crc32_update(payload + 40, payload_len - 40, crc);  /* records */
    crc ^= 0xFFFFFFFFu;

    return (crc == snap->hdr.crc32) ? 1 : 0;
}

void mts1_snapshot_free(mts1_snapshot_t* snap) {
    if (snap && snap->records) {
        free(snap->records);
        snap->records = NULL;
    }
}
