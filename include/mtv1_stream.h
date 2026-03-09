/*
 * MCU Malloc Tracker - MTV1 Streamer API
 *
 * Send heap snapshots, telemetry, and marks over MTV1 protocol.
 * No malloc, callback-based I/O.
 */

#ifndef MTV1_STREAM_H
#define MTV1_STREAM_H

#include <stddef.h>
#include <stdint.h>

/* MTV1 frame header (20 bytes, little-endian) */
typedef struct {
    uint8_t  magic[4];      /* "MTV1" */
    uint8_t  version;       /* 1 */
    uint8_t  type;          /* 1=SNAPSHOT, 2=TELEMETRY, 3=MARK, 4=END */
    uint16_t flags;         /* reserved */
    uint32_t seq;           /* frame sequence number */
    uint32_t payload_len;   /* bytes of payload */
    uint32_t crc32;         /* CRC32/IEEE over header(crc=0) + payload */
} mtv1_frame_hdr_t;

/* MTV1 streamer context — opaque to user */
typedef struct mtv1_stream mtv1_stream_t;

/* I/O callback: transmit bytes to physical layer */
typedef int (*mtv1_tx_fn)(const uint8_t* data, size_t len, void* ctx);

/**
 * mtv1_stream_init(tx_fn, tx_ctx)
 * Initialize MTV1 streamer.
 * Returns opaque handle, or NULL on error.
 * seq counter starts at 1.
 */
mtv1_stream_t* mtv1_stream_init(mtv1_tx_fn tx_fn, void* tx_ctx);

/**
 * mtv1_stream_next_seq()
 * Get and advance sequence counter (pre-increment).
 * Returns 1..UINT32_MAX in deterministic order.
 */
uint32_t mtv1_stream_next_seq(mtv1_stream_t* stream);

/**
 * mtv1_send_snapshot(stream, snapshot_buf, snapshot_len)
 * Send MTS1 snapshot payload as MTV1 SNAPSHOT_MTS1 frame.
 * Returns 0 on success, -1 on error.
 */
int mtv1_send_snapshot(mtv1_stream_t* stream, const uint8_t* snapshot_buf, uint32_t snapshot_len);

/**
 * mtv1_send_telemetry_line(stream, text)
 * Send text telemetry as MTV1 TELEMETRY_TEXT frame.
 * text = null-terminated string (no length param needed).
 * Returns 0 on success, -1 on error or text too long.
 */
int mtv1_send_telemetry_line(mtv1_stream_t* stream, const char* text);

/**
 * mtv1_send_mark(stream, label)
 * Send mark as MTV1 MARK_TEXT frame.
 * label = null-terminated string (e.g., "bootup", "crash").
 * Returns 0 on success, -1 on error or label too long.
 */
int mtv1_send_mark(mtv1_stream_t* stream, const char* label);

/**
 * mtv1_send_end(stream)
 * Send END frame (no payload) to signal stream termination.
 * Returns 0 on success, -1 on error.
 */
int mtv1_send_end(mtv1_stream_t* stream);

/**
 * mtv1_stream_free(stream)
 * Clean up MTV1 streamer context (no malloc to free, but for consistency).
 */
void mtv1_stream_free(mtv1_stream_t* stream);

#endif /* MTV1_STREAM_H */
