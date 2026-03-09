/*
 * MTV1 Streamer Selftest
 *
 * Verify MTV1 frame generation and CRC32/IEEE calculation.
 * No malloc, deterministic.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "../../include/mtv1_config.h"
#include "../../include/mtv1_stream.h"
#include "../../include/mt_crc32_ieee.h"

/* Mock I/O buffer */
static uint8_t g_tx_buffer[2048];
static size_t  g_tx_offset = 0;

/* Mock transmit callback */
static int mock_tx(const uint8_t* data, size_t len, void* ctx)
{
    (void)ctx;
    if (g_tx_offset + len > sizeof(g_tx_buffer)) {
        return -1;
    }
    memcpy(g_tx_buffer + g_tx_offset, data, len);
    g_tx_offset += len;
    return (int)len;
}

/* Helper: Parse MTV1 frame from buffer and verify CRC */
static int verify_frame(const uint8_t* buf, size_t buf_len, uint8_t expect_type, size_t expect_payload_len)
{
    if (buf_len < 20) {
        printf("  ERROR: frame too short (%zu bytes)\n", buf_len);
        return 0;
    }

    mtv1_frame_hdr_t* hdr = (mtv1_frame_hdr_t*)buf;

    /* Check magic */
    if (hdr->magic[0] != 'M' || hdr->magic[1] != 'T' ||
        hdr->magic[2] != 'V' || hdr->magic[3] != '1') {
        printf("  ERROR: bad magic\n");
        return 0;
    }

    /* Check version */
    if (hdr->version != 1) {
        printf("  ERROR: bad version %u\n", hdr->version);
        return 0;
    }

    /* Check type */
    if (hdr->type != expect_type) {
        printf("  ERROR: expected type %u, got %u\n", expect_type, hdr->type);
        return 0;
    }

    /* Check payload length */
    if (hdr->payload_len != (uint32_t)expect_payload_len) {
        printf("  ERROR: expected payload_len %zu, got %u\n", expect_payload_len, hdr->payload_len);
        return 0;
    }

    /* Verify CRC */
    uint32_t stored_crc = hdr->crc32;
    uint32_t crc = 0xFFFFFFFFu;

    /* Zero out CRC field for calculation */
    uint8_t hdr_copy[20];
    memcpy(hdr_copy, buf, 20);
    *(uint32_t*)(hdr_copy + 16) = 0;

    crc = mt_crc32_ieee_update(hdr_copy, 20, crc);
    if (expect_payload_len > 0) {
        crc = mt_crc32_ieee_update(buf + 20, expect_payload_len, crc);
    }
    crc ^= 0xFFFFFFFFu;

    if (crc != stored_crc) {
        printf("  ERROR: CRC mismatch. Expected 0x%08X, got 0x%08X\n", crc, stored_crc);
        return 0;
    }

    return 1;
}

int main(void)
{
    int test_count = 0, pass_count = 0;

    printf("====== MTV1 Streamer Selftest ======\n\n");

    /* Test 1: Initialize streamer */
    printf("Test 1: Initialize streamer\n");
    test_count++;

    mtv1_stream_t* stream = mtv1_stream_init(mock_tx, NULL);
    if (stream == NULL) {
        printf("  FAIL: mtv1_stream_init() returned NULL\n");
    } else {
        printf("  PASS\n");
        pass_count++;
    }

    /* Test 2: Send snapshot frame */
    printf("\nTest 2: Send snapshot frame\n");
    test_count++;

    g_tx_offset = 0;

    /* Build a minimal MTS1 snapshot (40-byte header, no records) */
    uint8_t snapshot_buf[40];
    memset(snapshot_buf, 0, sizeof(snapshot_buf));
    snapshot_buf[0] = 'M';
    snapshot_buf[1] = 'T';
    snapshot_buf[2] = 'S';
    snapshot_buf[3] = '1';
    snapshot_buf[4] = 1;  /* version */
    snapshot_buf[8] = 0;  /* record_count = 0 */

    int ret = mtv1_send_snapshot(stream, snapshot_buf, sizeof(snapshot_buf));
    if (ret != 0) {
        printf("  FAIL: mtv1_send_snapshot() returned %d\n", ret);
    } else if (!verify_frame(g_tx_buffer, g_tx_offset, MTV1_TYPE_SNAPSHOT_MTS1, sizeof(snapshot_buf))) {
        printf("  FAIL: frame verification failed\n");
    } else {
        printf("  Snapshot frame: size=%zu, seq=%u\n", g_tx_offset, *(uint32_t*)(g_tx_buffer + 8));
        printf("  PASS\n");
        pass_count++;
    }

    /* Test 3: Send telemetry frame */
    printf("\nTest 3: Send telemetry frame\n");
    test_count++;

    g_tx_offset = 0;
    const char* telem_text = "heap ok";
    ret = mtv1_send_telemetry_line(stream, telem_text);
    if (ret != 0) {
        printf("  FAIL: mtv1_send_telemetry_line() returned %d\n", ret);
    } else if (!verify_frame(g_tx_buffer, g_tx_offset, MTV1_TYPE_TELEMETRY_TEXT, strlen(telem_text))) {
        printf("  FAIL: frame verification failed\n");
    } else {
        printf("  Telemetry frame: size=%zu, seq=%u\n", g_tx_offset, *(uint32_t*)(g_tx_buffer + 8));
        printf("  PASS\n");
        pass_count++;
    }

    /* Test 4: Send mark frame */
    printf("\nTest 4: Send mark frame\n");
    test_count++;

    g_tx_offset = 0;
    const char* mark_text = "bootup";
    ret = mtv1_send_mark(stream, mark_text);
    if (ret != 0) {
        printf("  FAIL: mtv1_send_mark() returned %d\n", ret);
    } else if (!verify_frame(g_tx_buffer, g_tx_offset, MTV1_TYPE_MARK_TEXT, strlen(mark_text))) {
        printf("  FAIL: frame verification failed\n");
    } else {
        printf("  Mark frame: size=%zu, seq=%u\n", g_tx_offset, *(uint32_t*)(g_tx_buffer + 8));
        printf("  PASS\n");
        pass_count++;
    }

    /* Test 5: Send END frame */
    printf("\nTest 5: Send END frame\n");
    test_count++;

    g_tx_offset = 0;
    ret = mtv1_send_end(stream);
    if (ret != 0) {
        printf("  FAIL: mtv1_send_end() returned %d\n", ret);
    } else if (!verify_frame(g_tx_buffer, g_tx_offset, MTV1_TYPE_END, 0)) {
        printf("  FAIL: frame verification failed\n");
    } else {
        printf("  END frame: size=%zu, seq=%u\n", g_tx_offset, *(uint32_t*)(g_tx_buffer + 8));
        printf("  PASS\n");
        pass_count++;
    }

    /* Test 6: Verify sequence numbers are monotonic */
    printf("\nTest 6: Verify sequence numbers\n");
    test_count++;

    mtv1_stream_t* stream2 = mtv1_stream_init(mock_tx, NULL);
    if (stream2 == NULL) {
        printf("  FAIL: could not reinitialize\n");
    } else {
        uint32_t seq1 = mtv1_stream_next_seq(stream2);
        uint32_t seq2 = mtv1_stream_next_seq(stream2);
        uint32_t seq3 = mtv1_stream_next_seq(stream2);

        if (seq1 != 1 || seq2 != 2 || seq3 != 3) {
            printf("  FAIL: expected seq 1,2,3 got %u,%u,%u\n", seq1, seq2, seq3);
        } else {
            printf("  Sequences: %u, %u, %u\n", seq1, seq2, seq3);
            printf("  PASS\n");
            pass_count++;
        }
    }

    /* Summary */
    printf("\n====== SUMMARY ======\n");
    printf("Tests: %d/%d PASS\n", pass_count, test_count);

    if (pass_count == test_count) {
        printf("Status: ALL PASS ✓\n");
        return 0;
    } else {
        printf("Status: FAILURES\n");
        return 1;
    }
}
