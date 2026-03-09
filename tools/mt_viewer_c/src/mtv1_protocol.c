#include "mtv1_protocol.h"
#include "mtv1_crc32.h"
#include "mtv1_util.h"
#include <string.h>
#include <stdlib.h>

mtv1_parse_result_t mtv1_frame_read(
    int (*read_byte)(void* ctx),
    void*          ctx,
    mtv1_frame_t*  out_frame,
    uint32_t*      out_resync_bytes) {

    if (out_resync_bytes)
        *out_resync_bytes = 0;

    memset(out_frame, 0, sizeof(*out_frame));
    out_frame->payload = NULL;

    uint32_t resync_bytes = 0;
    uint8_t header_buf[20];

    /* Phase 1: Search for MTV1 magic */
    uint8_t magic_window[4] = {0};
    int magic_found = 0;
    uint32_t bytes_before_magic = 0;

    while (!magic_found) {
        int byte_val = read_byte(ctx);
        if (byte_val < 0)
            return MTV1_PARSE_EOF;

        uint8_t byte = (uint8_t)byte_val;

        /* Slide the window */
        magic_window[0] = magic_window[1];
        magic_window[1] = magic_window[2];
        magic_window[2] = magic_window[3];
        magic_window[3] = byte;

        if (magic_window[0] == 'M' && magic_window[1] == 'T' &&
            magic_window[2] == 'V' && magic_window[3] == '1') {
            magic_found = 1;
            memcpy(header_buf, "MTV1", 4);
            /* Subtract 3 because the last 3 bytes of the magic are new, not skipped */
            resync_bytes = bytes_before_magic > 3 ? bytes_before_magic - 3 : 0;
        } else {
            bytes_before_magic++;
        }
    }

    /* Phase 2: Read remaining 16 bytes of header (bytes 4-19) */
    for (int i = 0; i < 16; i++) {
        int byte_val = read_byte(ctx);
        if (byte_val < 0) {
            if (out_resync_bytes)
                *out_resync_bytes = resync_bytes;
            return MTV1_PARSE_EOF;
        }
        header_buf[4 + i] = (uint8_t)byte_val;
    }

    /* Phase 3: Parse header */
    out_frame->hdr.version = header_buf[4];
    out_frame->hdr.type = header_buf[5];
    out_frame->hdr.flags = util_read_u16le(&header_buf[6]);
    out_frame->hdr.seq = util_read_u32le(&header_buf[8]);
    out_frame->hdr.payload_len = util_read_u32le(&header_buf[12]);
    out_frame->hdr.crc32 = util_read_u32le(&header_buf[16]);
    memcpy(out_frame->hdr.magic, "MTV1", 4);

    /* Validate header */
    if (out_frame->hdr.version != 1 || out_frame->hdr.type > 4 ||
        out_frame->hdr.payload_len > 65536) {
        /* Invalid header, but we've already consumed data. Report error. */
        if (out_resync_bytes)
            *out_resync_bytes = resync_bytes;
        return MTV1_PARSE_ERROR;
    }

    /* Special case: END frame has no payload */
    if (out_frame->hdr.type == MTV1_TYPE_END) {
        out_frame->payload = NULL;
        out_frame->crc_ok = 1; /* END frames don't require CRC check */
        if (out_resync_bytes)
            *out_resync_bytes = resync_bytes;
        return MTV1_PARSE_OK;
    }

    /* Phase 4: Read payload */
    if (out_frame->hdr.payload_len > 0) {
        out_frame->payload = (uint8_t*)malloc(out_frame->hdr.payload_len);
        if (!out_frame->payload) {
            if (out_resync_bytes)
                *out_resync_bytes = resync_bytes;
            return MTV1_PARSE_ERROR;
        }

        for (uint32_t i = 0; i < out_frame->hdr.payload_len; i++) {
            int byte_val = read_byte(ctx);
            if (byte_val < 0) {
                free(out_frame->payload);
                out_frame->payload = NULL;
                if (out_resync_bytes)
                    *out_resync_bytes = resync_bytes;
                return MTV1_PARSE_EOF;
            }
            out_frame->payload[i] = (uint8_t)byte_val;
        }
    }

    /* Phase 5: Verify CRC */
    out_frame->crc_ok = mtv1_frame_verify_crc(out_frame);

    if (out_resync_bytes)
        *out_resync_bytes = resync_bytes;

    /* Return RESYNC if we skipped any bytes during magic search */
    if (resync_bytes > 0)
        return MTV1_PARSE_RESYNC;
    return MTV1_PARSE_OK;
}

int mtv1_frame_verify_crc(const mtv1_frame_t* f) {
    /* CRC covers: 20-byte header (with crc32 field zeroed) + payload */
    uint8_t header_copy[20];
    memcpy(header_copy, &f->hdr, 20);

    /* Zero the CRC field (bytes 16-19 of the frame header) */
    memset(&header_copy[16], 0, 4);

    uint32_t crc = 0xFFFFFFFFu;
    crc = mtv1_crc32_update(header_copy, 20, crc);
    if (f->hdr.payload_len > 0)
        crc = mtv1_crc32_update(f->payload, f->hdr.payload_len, crc);
    crc ^= 0xFFFFFFFFu;

    return (crc == f->hdr.crc32) ? 1 : 0;
}

void mtv1_frame_free(mtv1_frame_t* f) {
    if (f && f->payload) {
        free(f->payload);
        f->payload = NULL;
    }
}
