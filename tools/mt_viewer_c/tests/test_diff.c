#include "../include/mtv1_protocol.h"
#include "../include/mtv1_decode_mts1.h"
#include "../include/mtv1_model.h"
#include "../include/mtv1_commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

static mtv1_model_t* load_run_mem(const uint8_t* data, size_t size) {
    mtv1_model_t* m = (mtv1_model_t*)malloc(sizeof(*m));
    mtv1_model_init(m);

    mem_byte_ctx_t byte_ctx = {.data = data, .size = size, .pos = 0};

    while (1) {
        mtv1_frame_t frame;
        mtv1_parse_result_t result = mtv1_frame_read(mem_byte_source, &byte_ctx, &frame, NULL);

        if (result == MTV1_PARSE_EOF)
            break;

        if (result != MTV1_PARSE_OK && result != MTV1_PARSE_RESYNC)
            continue;

        if (frame.hdr.type == MTV1_TYPE_SNAPSHOT_MTS1) {
            mts1_snapshot_t snap;
            if (mts1_parse(frame.payload, frame.hdr.payload_len, &snap) == 0) {
                snap.crc_ok = mts1_verify_crc(frame.payload, frame.hdr.payload_len, &snap);
                mtv1_model_push_snapshot(m, frame.hdr.seq, &snap);
                mts1_snapshot_free(&snap);
            }
        } else if (frame.hdr.type == MTV1_TYPE_END) {
            mtv1_frame_free(&frame);
            break;
        }

        mtv1_frame_free(&frame);
    }

    return m;
}

int main(void) {
    printf("MTV1 Diff Test (identity: same file compared to itself)\n");

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

    /* Load same file as both A and B */
    printf("Loading run_clean.bin as both A and B...\n");
    mtv1_model_t* a = load_run_mem(data, (size_t)size);
    mtv1_model_t* b = load_run_mem(data, (size_t)size);

    free(data);

    printf("Run A: %u snapshots, peak=%u\n", a->timeline_len, a->peak_used_ever);
    printf("Run B: %u snapshots, peak=%u\n", b->timeline_len, b->peak_used_ever);

    /* Compute diff */
    mtv1_diff_result_t diff;
    mtv1_diff_compute(a, b, &diff);

    printf("\nDiff Results:\n");
    printf("  peak_delta: %d (expected: 0)\n", diff.peak_delta);
    printf("  slope_delta: %.2f (expected: ~0)\n", diff.slope_delta);
    printf("  spike_count: %u (expected: 0)\n", diff.spike_count);
    printf("  new_hotspot_count: %u (expected: 0)\n", diff.new_hotspot_count);
    printf("  resolved_hotspot_count: %u (expected: 0)\n", diff.resolved_hotspot_count);

    /* Validate */
    int peak_ok = (diff.peak_delta == 0);
    int slope_ok = (fabs(diff.slope_delta) < 0.01);
    int spike_ok = (diff.spike_count == 0);
    int hotspot_new_ok = (diff.new_hotspot_count == 0);
    int hotspot_resolved_ok = (diff.resolved_hotspot_count == 0);

    int pass = peak_ok && slope_ok && spike_ok && hotspot_new_ok && hotspot_resolved_ok;

    printf("\nValidation:\n");
    printf("  peak_delta == 0: %s\n", peak_ok ? "PASS" : "FAIL");
    printf("  |slope_delta| < 0.01: %s\n", slope_ok ? "PASS" : "FAIL");
    printf("  spike_count == 0: %s\n", spike_ok ? "PASS" : "FAIL");
    printf("  new_hotspot_count == 0: %s\n", hotspot_new_ok ? "PASS" : "FAIL");
    printf("  resolved_hotspot_count == 0: %s\n", hotspot_resolved_ok ? "PASS" : "FAIL");

    printf("\nTest: %s\n", pass ? "PASS" : "FAIL");

    mtv1_model_free(a);
    mtv1_model_free(b);
    free(a);
    free(b);

    return pass ? 0 : 1;
}
