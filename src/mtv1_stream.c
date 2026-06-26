/*
 * MCU Malloc Tracker - MTV1 Streamer Implementation
 *
 * Send MTV1 frames with proper CRC32/IEEE and sequencing.
 * Contexts are caller-owned via the opaque handle returned by init/free.
 */

#include <stdlib.h>
#include <string.h>
#include "../include/mtv1_config.h"
#include "../include/mtv1_stream.h"
#include "../include/mt_crc32_ieee.h"

struct mtv1_stream {
    mtv1_tx_fn tx_fn;
    void* tx_ctx;
    uint32_t seq_counter;
};

static void mtv1_write_u16le(uint8_t* dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void mtv1_write_u32le(uint8_t* dst, uint32_t value)
{
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
    dst[2] = (uint8_t)((value >> 16) & 0xFFu);
    dst[3] = (uint8_t)((value >> 24) & 0xFFu);
}

mtv1_stream_t* mtv1_stream_init(mtv1_tx_fn tx_fn, void* tx_ctx)
{
    if (!tx_fn) {
        return NULL;
    }

    mtv1_stream_t* stream = (mtv1_stream_t*)malloc(sizeof(*stream));
    if (!stream) {
        return NULL;
    }

    stream->tx_fn = tx_fn;
    stream->tx_ctx = tx_ctx;
    stream->seq_counter = 0;
    return stream;
}

uint32_t mtv1_stream_next_seq(mtv1_stream_t* stream)
{
    if (!stream) {
        return 0;
    }

    if (stream->seq_counter == UINT32_MAX) {
        stream->seq_counter = 0;
    }

    stream->seq_counter++;
    return stream->seq_counter;
}

static int mtv1_send_frame(mtv1_stream_t* stream, uint8_t type, const uint8_t* payload, uint32_t payload_len)
{
    if (!stream || !stream->tx_fn) {
        return -1;
    }
    if (payload_len > MTV1_TX_MAX_PAYLOAD) {
        return -1;
    }
    if (payload_len > 0 && !payload) {
        return -1;
    }

    uint8_t frame_buf[20 + MTV1_TX_MAX_PAYLOAD];
    memset(frame_buf, 0, 20);
    frame_buf[0] = 'M';
    frame_buf[1] = 'T';
    frame_buf[2] = 'V';
    frame_buf[3] = '1';
    frame_buf[4] = 1u;
    frame_buf[5] = type;
    mtv1_write_u16le(frame_buf + 6, 0u);
    mtv1_write_u32le(frame_buf + 8, mtv1_stream_next_seq(stream));
    mtv1_write_u32le(frame_buf + 12, payload_len);
    mtv1_write_u32le(frame_buf + 16, 0u);

    if (payload_len > 0) {
        memcpy(frame_buf + 20, payload, payload_len);
    }

    uint32_t crc = 0xFFFFFFFFu;
    crc = mt_crc32_ieee_update(frame_buf, 20u, crc);
    if (payload_len > 0) {
        crc = mt_crc32_ieee_update(frame_buf + 20u, payload_len, crc);
    }
    crc ^= 0xFFFFFFFFu;
    mtv1_write_u32le(frame_buf + 16, crc);

    size_t frame_total = 20u + (size_t)payload_len;
    int tx_result = stream->tx_fn(frame_buf, frame_total, stream->tx_ctx);
    if (tx_result != (int)frame_total) {
        return -1;
    }

    return 0;
}

int mtv1_send_snapshot(mtv1_stream_t* stream, const uint8_t* snapshot_buf, uint32_t snapshot_len)
{
    if (!snapshot_buf || snapshot_len < 36u) {
        return -1;
    }

    return mtv1_send_frame(stream, MTV1_TYPE_SNAPSHOT_MTS1, snapshot_buf, snapshot_len);
}

int mtv1_send_telemetry_line(mtv1_stream_t* stream, const char* text)
{
    if (!text) {
        return -1;
    }

    size_t len = strlen(text);
    if (len > MTV1_TX_MAX_PAYLOAD) {
        return -1;
    }

    return mtv1_send_frame(stream, MTV1_TYPE_TELEMETRY_TEXT, (const uint8_t*)text, (uint32_t)len);
}

int mtv1_send_mark(mtv1_stream_t* stream, const char* label)
{
    if (!label) {
        return -1;
    }

    size_t len = strlen(label);
    if (len > MTV1_TX_MAX_PAYLOAD) {
        return -1;
    }

    return mtv1_send_frame(stream, MTV1_TYPE_MARK_TEXT, (const uint8_t*)label, (uint32_t)len);
}

int mtv1_send_end(mtv1_stream_t* stream)
{
    return mtv1_send_frame(stream, MTV1_TYPE_END, NULL, 0u);
}

void mtv1_stream_free(mtv1_stream_t* stream)
{
    free(stream);
}
