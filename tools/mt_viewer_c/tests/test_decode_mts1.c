#include "../include/mtv1_protocol.h"
#include "../include/mtv1_decode_mts1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const uint8_t* data;
    size_t size;
    size_t pos;
} mem_byte_ctx_t;

static int mem_byte_source(void* ctx) {
    mem_byte_ctx_t* mctx = (mem_byte_ctx_t*)ctx;
    if (mctx->pos >= mctx->size)
        return -1;
    return mctx->data[mctx->pos++];
}

int main(void) {
    printf("MTS1 Decode Test\n");

    /* Load run_clean.bin */
    const char* vector_path = MTV1_VECTOR_DIR "run_clean.bin";
    FILE* f = fopen(vector_path, "rb");
    if (!f) {
        fprintf(stderr, "Error: failed to open %s\n", vector_path);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t* data = (uint8_t*)malloc(size);
    if (fread(data, 1, size, f) != (size_t)size) {
        fprintf(stderr, "Error: failed to read vector\n");
        free(data);
        fclose(f);
        return 1;
    }
    fclose(f);

    mem_byte_ctx_t byte_ctx = {.data = data, .size = (size_t)size, .pos = 0};

    uint32_t snap_count = 0;
    uint32_t total_records = 0;
    int all_crc_ok = 1;
    int ptr_monotonic_ok = 1;

    while (1) {
        mtv1_frame_t frame;
        mtv1_parse_result_t result = mtv1_frame_read(mem_byte_source, &byte_ctx, &frame, NULL);

        if (result == MTV1_PARSE_EOF)
            break;

        if (result != MTV1_PARSE_OK && result != MTV1_PARSE_RESYNC)
            continue;

        if (frame.hdr.type == MTV1_TYPE_SNAPSHOT_MTS1) {
            mts1_snapshot_t snap;
            int parse_result = mts1_parse(frame.payload, frame.hdr.payload_len, &snap);

            if (parse_result != 0) {
                fprintf(stderr, "Error: failed to parse MTS1 snapshot\n");
                continue;
            }

            snap_count++;
            total_records += snap.hdr.record_count;

            /* Validate magic */
            if (snap.hdr.magic[0] != 'M' || snap.hdr.magic[1] != 'T' ||
                snap.hdr.magic[2] != 'S' || snap.hdr.magic[3] != '1') {
                fprintf(stderr, "Error: bad MTS1 magic\n");
                mts1_snapshot_free(&snap);
                continue;
            }

            /* Validate version */
            if (snap.hdr.version != 1) {
                fprintf(stderr, "Error: bad MTS1 version: %u\n", snap.hdr.version);
                mts1_snapshot_free(&snap);
                continue;
            }

            /* Verify CRC */
            int crc_ok = mts1_verify_crc(frame.payload, frame.hdr.payload_len, &snap);
            printf("  Snapshot %u: %u records, CRC=%s\n",
                   snap_count, snap.hdr.record_count, crc_ok ? "OK" : "FAIL");

            if (!crc_ok)
                all_crc_ok = 0;

            /* Check ptr monotonic */
            for (uint32_t i = 1; i < snap.hdr.record_count; i++) {
                if (snap.records[i].ptr < snap.records[i-1].ptr) {
                    fprintf(stderr, "Error: ptr not monotonic at record %u\n", i);
                    ptr_monotonic_ok = 0;
                }
            }

            mts1_snapshot_free(&snap);
        } else if (frame.hdr.type == MTV1_TYPE_END) {
            printf("END frame received\n");
            mtv1_frame_free(&frame);
            break;
        }

        mtv1_frame_free(&frame);
    }

    free(data);

    printf("\nResults:\n");
    printf("  Snapshots: %u\n", snap_count);
    printf("  Total records: %u\n", total_records);
    printf("  CRC OK: %s\n", all_crc_ok ? "yes" : "no");
    printf("  Ptr monotonic: %s\n", ptr_monotonic_ok ? "yes" : "no");

    int pass = (snap_count > 0 && all_crc_ok && ptr_monotonic_ok);
    printf("\nTest: %s\n", pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}
