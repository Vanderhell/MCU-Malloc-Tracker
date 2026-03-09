#include "mtv1_commands.h"
#include "mtv1_protocol.h"
#include "mtv1_decode_mts1.h"
#include "mtv1_model.h"
#include "mtv1_filemap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
    FILE* f;
} file_byte_ctx_t;

static int diff_byte_source(void* ctx) {
    file_byte_ctx_t* fctx = (file_byte_ctx_t*)ctx;
    int c = fgetc(fctx->f);
    return (c == EOF) ? -1 : c;
}

static mtv1_model_t* load_run_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    mtv1_model_t* m = (mtv1_model_t*)malloc(sizeof(*m));
    mtv1_model_init(m);

    file_byte_ctx_t byte_ctx = {.f = f};

    while (1) {
        mtv1_frame_t frame;
        mtv1_parse_result_t result = mtv1_frame_read(diff_byte_source, &byte_ctx, &frame, NULL);

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

    fclose(f);
    return m;
}

int mtv1_diff_compute(const mtv1_model_t* a, const mtv1_model_t* b,
                      mtv1_diff_result_t* out) {
    if (!a || !b || !out) return -1;

    memset(out, 0, sizeof(*out));

    /* Peak delta */
    out->peak_delta = (int32_t)b->peak_used_ever - (int32_t)a->peak_used_ever;

    /* Slope delta: simple linear fit over timeline */
    if (a->timeline_len > 1 && b->timeline_len > 1) {
        double sum_a = 0, sum_b = 0;
        for (uint32_t i = 0; i < a->timeline_len; i++)
            sum_a += a->timeline[i].current_used;
        for (uint32_t i = 0; i < b->timeline_len; i++)
            sum_b += b->timeline[i].current_used;

        double avg_a = sum_a / a->timeline_len;
        double avg_b = sum_b / b->timeline_len;

        double slope_a = (double)(a->timeline[a->timeline_len-1].current_used - a->timeline[0].current_used)
                         / a->timeline_len;
        double slope_b = (double)(b->timeline[b->timeline_len-1].current_used - b->timeline[0].current_used)
                         / b->timeline_len;

        out->slope_delta = slope_b - slope_a;
    }

    /* Spike count: frames with >10% increase in current_used */
    uint32_t peak_a = a->peak_used_ever;
    uint32_t peak_b = b->peak_used_ever;
    uint32_t threshold_a = peak_a / 10;
    uint32_t threshold_b = peak_b / 10;

    for (uint32_t i = 1; i < a->timeline_len; i++) {
        uint32_t delta = a->timeline[i].current_used > a->timeline[i-1].current_used
                         ? a->timeline[i].current_used - a->timeline[i-1].current_used
                         : 0;
        if (delta > threshold_a)
            out->spike_count++;
    }

    for (uint32_t i = 1; i < b->timeline_len; i++) {
        uint32_t delta = b->timeline[i].current_used > b->timeline[i-1].current_used
                         ? b->timeline[i].current_used - b->timeline[i-1].current_used
                         : 0;
        if (delta > threshold_b)
            out->spike_count++;
    }

    /* Hotspot delta */
    for (uint32_t i = 0; i < b->hotspot_count; i++) {
        int found = 0;
        for (uint32_t j = 0; j < a->hotspot_count; j++) {
            if (a->hotspots[j].file_id == b->hotspots[i].file_id &&
                a->hotspots[j].line == b->hotspots[i].line) {
                found = 1;
                break;
            }
        }
        if (!found)
            out->new_hotspot_count++;
    }

    for (uint32_t i = 0; i < a->hotspot_count; i++) {
        int found = 0;
        for (uint32_t j = 0; j < b->hotspot_count; j++) {
            if (b->hotspots[j].file_id == a->hotspots[i].file_id &&
                b->hotspots[j].line == a->hotspots[i].line) {
                found = 1;
                break;
            }
        }
        if (!found)
            out->resolved_hotspot_count++;
    }

    return 0;
}

int mtv1_diff_run(const mtv1_diff_opts_t* opts) {
    if (!opts || !opts->path_a || !opts->path_b || !opts->out_path) {
        fprintf(stderr, "Error: diff requires --a --b --out\n");
        return 1;
    }

    printf("Loading run A: %s\n", opts->path_a);
    mtv1_model_t* a = load_run_file(opts->path_a);
    if (!a) {
        fprintf(stderr, "Error: failed to load %s\n", opts->path_a);
        return 1;
    }

    printf("Loading run B: %s\n", opts->path_b);
    mtv1_model_t* b = load_run_file(opts->path_b);
    if (!b) {
        fprintf(stderr, "Error: failed to load %s\n", opts->path_b);
        mtv1_model_free(a);
        free(a);
        return 1;
    }

    mtv1_filemap_t filemap;
    mtv1_filemap_init(&filemap);
    if (opts->filemap_path)
        mtv1_filemap_load(&filemap, opts->filemap_path);

    mtv1_diff_result_t diff;
    mtv1_diff_compute(a, b, &diff);

    FILE* out = fopen(opts->out_path, "w");
    if (!out) {
        fprintf(stderr, "Error: failed to create %s\n", opts->out_path);
        mtv1_model_free(a);
        mtv1_model_free(b);
        free(a);
        free(b);
        return 1;
    }

    fprintf(out, "# MTV1 Diff Report\n\n");
    fprintf(out, "## Peak Heap Usage\n");
    fprintf(out, "- Run A: %u bytes\n", a->peak_used_ever);
    fprintf(out, "- Run B: %u bytes\n", b->peak_used_ever);
    fprintf(out, "- Delta: %+d bytes\n\n", diff.peak_delta);

    fprintf(out, "## Timeline Statistics\n");
    fprintf(out, "- Run A: %u snapshots\n", a->timeline_len);
    fprintf(out, "- Run B: %u snapshots\n", b->timeline_len);
    fprintf(out, "- Slope Delta: %.2f bytes/frame\n\n", diff.slope_delta);

    fprintf(out, "## Spikes Detected\n");
    fprintf(out, "- Total spikes (>10%% peak): %u\n\n", diff.spike_count);

    fprintf(out, "## Hotspot Changes\n");
    fprintf(out, "- New hotspots (in B, not A): %u\n", diff.new_hotspot_count);
    fprintf(out, "- Resolved hotspots (in A, not B): %u\n\n", diff.resolved_hotspot_count);

    fprintf(out, "## Top Hotspots (Run A)\n");
    mtv1_hotspot_t top_a[10];
    uint32_t top_a_count = mtv1_model_top_hotspots(a, top_a, 10);
    for (uint32_t i = 0; i < top_a_count; i++) {
        fprintf(out, "%2u. 0x%X:%u  %u allocs, %u bytes\n",
                i+1, top_a[i].file_id, top_a[i].line, top_a[i].total_allocs, top_a[i].total_bytes);
    }

    fprintf(out, "\n## Top Hotspots (Run B)\n");
    mtv1_hotspot_t top_b[10];
    uint32_t top_b_count = mtv1_model_top_hotspots(b, top_b, 10);
    for (uint32_t i = 0; i < top_b_count; i++) {
        fprintf(out, "%2u. 0x%X:%u  %u allocs, %u bytes\n",
                i+1, top_b[i].file_id, top_b[i].line, top_b[i].total_allocs, top_b[i].total_bytes);
    }

    fclose(out);

    printf("Diff report written to %s\n", opts->out_path);

    mtv1_model_free(a);
    mtv1_model_free(b);
    free(a);
    free(b);
    mtv1_filemap_free(&filemap);

    return 0;
}
