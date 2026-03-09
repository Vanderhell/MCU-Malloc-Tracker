/*
 * MCU Malloc Tracker - MTV1 Streamer Implementation
 *
 * Send MTV1 frames with proper CRC32/IEEE and sequencing.
 * No malloc, callback-based.
 */

#include <string.h>
#include "../include/mtv1_config.h"
#include "../include/mtv1_stream.h"
#include "../include/mt_crc32_ieee.h"

/* MTV1 streamer context (stack-allocated by user or embedded) */
typedef struct mtv1_stream {
    mtv1_tx_fn tx_fn;
    void*      tx_ctx;
    uint32_t   seq_counter;
} mtv1_stream_t;

/* Static context (one instance per MCU) */
static mtv1_stream_t g_streamer = {0};

/**
 * mtv1_stream_init(tx_fn, tx_ctx)
 * Initialize MTV1 streamer.
 */
mtv1_stream_t* mtv1_stream_init(mtv1_tx_fn tx_fn, void* tx_ctx)
{
    if (!tx_fn) {
        return NULL;
    }

    g_streamer.tx_fn = tx_fn;
    g_streamer.tx_ctx = tx_ctx;
    g_streamer.seq_counter = 0;  /* Will be incremented to 1 on first call */

    return &g_streamer;
}

/**
 * mtv1_stream_next_seq()
 * Get and advance sequence counter.
 */
uint32_t mtv1_stream_next_seq(mtv1_stream_t* stream)
{
    if (!stream) {
        return 0;
    }

    return ++stream->seq_counter;
}

/**
 * mtv1_send_frame(stream, type, payload, payload_len)
 * Low-level frame sender (internal).
 * Returns 0 on success, -1 on error.
 */
static int mtv1_send_frame(mtv1_stream_t* stream, uint8_t type, const uint8_t* payload, uint32_t payload_len)
{
    if (!stream || !stream->tx_fn) {
        return -1;
    }

    /* Validate payload length */
    if (payload_len > MTV1_TX_MAX_PAYLOAD) {
        return -1;
    }

    /* Build frame header (20 bytes) */
    uint8_t frame_buf[20 + MTV1_TX_MAX_PAYLOAD];
    mtv1_frame_hdr_t* hdr = (mtv1_frame_hdr_t*)frame_buf;

    /* Magic */
    hdr->magic[0] = 'M';
    hdr->magic[1] = 'T';
    hdr->magic[2] = 'V';
    hdr->magic[3] = '1';

    /* Header fields */
    hdr->version = 1;
    hdr->type = type;
    hdr->flags = 0;
    hdr->seq = mtv1_stream_next_seq(stream);
    hdr->payload_len = payload_len;
    hdr->crc32 = 0;  /* Will compute */

    /* Copy payload */
    if (payload_len > 0 && payload) {
        memcpy(frame_buf + 20, payload, payload_len);
    }

    /* Calculate CRC32 over header(crc=0) + payload */
    uint32_t crc = 0xFFFFFFFFu;
    crc = mt_crc32_ieee_update(frame_buf, 20, crc);  /* header with crc32=0 */
    if (payload_len > 0) {
        crc = mt_crc32_ieee_update(frame_buf + 20, payload_len, crc);  /* payload */
    }
    crc ^= 0xFFFFFFFFu;  /* final XOR */

    /* Store CRC in header */
    hdr->crc32 = crc;

    /* Transmit frame (header + payload) */
    size_t frame_total = 20 + payload_len;
    int tx_result = stream->tx_fn(frame_buf, frame_total, stream->tx_ctx);

    return (tx_result >= 0) ? 0 : -1;
}

/**
 * mtv1_send_snapshot(stream, snapshot_buf, snapshot_len)
 * Send MTS1 snapshot as MTV1 frame.
 */
int mtv1_send_snapshot(mtv1_stream_t* stream, const uint8_t* snapshot_buf, uint32_t snapshot_len)
{
    if (!snapshot_buf || snapshot_len < 40) {  /* Minimum: MTS1 header */
        return -1;
    }

    return mtv1_send_frame(stream, MTV1_TYPE_SNAPSHOT_MTS1, snapshot_buf, snapshot_len);
}

/**
 * mtv1_send_telemetry_line(stream, text)
 * Send text telemetry.
 */
int mtv1_send_telemetry_line(mtv1_stream_t* stream, const char* text)
{
    if (!text) {
        return -1;
    }

    size_t len = 0;
    for (const char* p = text; *p; p++) {
        len++;
    }

    if (len > MTV1_TX_MAX_PAYLOAD) {
        return -1;
    }

    return mtv1_send_frame(stream, MTV1_TYPE_TELEMETRY_TEXT, (const uint8_t*)text, (uint32_t)len);
}

/**
 * mtv1_send_mark(stream, label)
 * Send mark.
 */
int mtv1_send_mark(mtv1_stream_t* stream, const char* label)
{
    if (!label) {
        return -1;
    }

    size_t len = 0;
    for (const char* p = label; *p; p++) {
        len++;
    }

    if (len > MTV1_TX_MAX_PAYLOAD) {
        return -1;
    }

    return mtv1_send_frame(stream, MTV1_TYPE_MARK_TEXT, (const uint8_t*)label, (uint32_t)len);
}

/**
 * mtv1_send_end(stream)
 * Send END frame.
 */
int mtv1_send_end(mtv1_stream_t* stream)
{
    return mtv1_send_frame(stream, MTV1_TYPE_END, NULL, 0);
}

/**
 * mtv1_stream_free(stream)
 * Clean up (no-op since no malloc, but for API consistency).
 */
void mtv1_stream_free(mtv1_stream_t* stream)
{
    (void)stream;  /* No cleanup needed */
}
