#include "mtv1_commands.h"
#include "mtv1_protocol.h"
#include "mtv1_decode_mts1.h"
#include "mtv1_model.h"
#include "mtv1_filemap.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    FILE* f;
} file_byte_ctx_t;

static int export_byte_source(void* ctx) {
    file_byte_ctx_t* fctx = (file_byte_ctx_t*)ctx;
    int c = fgetc(fctx->f);
    return (c == EOF) ? -1 : c;
}

int mtv1_export_run(const mtv1_export_opts_t* opts) {
    if (!opts || !opts->in_path || (!opts->csv_path && !opts->jsonl_path)) {
        fprintf(stderr, "Error: export requires --in and at least one of --csv/--jsonl\n");
        return 1;
    }

    FILE* f = fopen(opts->in_path, "rb");
    if (!f) {
        fprintf(stderr, "Error: failed to open %s\n", opts->in_path);
        return 1;
    }

    FILE* csv_out = NULL;
    if (opts->csv_path) {
        csv_out = fopen(opts->csv_path, "w");
        if (!csv_out) {
            fprintf(stderr, "Error: failed to create %s\n", opts->csv_path);
            fclose(f);
            return 1;
        }
        fprintf(csv_out, "snap_index,current_used,peak_used,total_allocs,total_frees,active_count\n");
    }

    FILE* jsonl_out = NULL;
    if (opts->jsonl_path) {
        jsonl_out = fopen(opts->jsonl_path, "w");
        if (!jsonl_out) {
            fprintf(stderr, "Error: failed to create %s\n", opts->jsonl_path);
            fclose(f);
            if (csv_out) fclose(csv_out);
            return 1;
        }
    }

    file_byte_ctx_t byte_ctx = {.f = f};
    uint32_t snap_index = 0;

    while (1) {
        mtv1_frame_t frame;
        mtv1_parse_result_t result = mtv1_frame_read(export_byte_source, &byte_ctx, &frame, NULL);

        if (result == MTV1_PARSE_EOF)
            break;

        if (result != MTV1_PARSE_OK && result != MTV1_PARSE_RESYNC)
            continue;

        if (frame.hdr.type == MTV1_TYPE_SNAPSHOT_MTS1) {
            mts1_snapshot_t snap;
            if (mts1_parse(frame.payload, frame.hdr.payload_len, &snap) == 0) {
                if (csv_out) {
                    fprintf(csv_out, "%u,%u,%u,%u,%u,%u\n",
                            snap_index,
                            snap.hdr.current_used,
                            snap.hdr.peak_used,
                            snap.hdr.total_allocs,
                            snap.hdr.total_frees,
                            snap.hdr.record_count);
                }

                if (jsonl_out) {
                    fprintf(jsonl_out, "{\"snap_index\":%u,\"current_used\":%u,\"peak_used\":%u,"
                            "\"total_allocs\":%u,\"total_frees\":%u,\"active_count\":%u,\"records\":[",
                            snap_index,
                            snap.hdr.current_used,
                            snap.hdr.peak_used,
                            snap.hdr.total_allocs,
                            snap.hdr.total_frees,
                            snap.hdr.record_count);

                    for (uint32_t i = 0; i < snap.hdr.record_count; i++) {
                        if (i > 0) fprintf(jsonl_out, ",");
                        fprintf(jsonl_out, "{\"ptr\":\"0x%llX\",\"size\":%u,\"file_id\":\"0x%X\",\"line\":%u}",
                                (unsigned long long)snap.records[i].ptr,
                                snap.records[i].size,
                                snap.records[i].file_id,
                                snap.records[i].line);
                    }

                    fprintf(jsonl_out, "]}\n");
                }

                snap_index++;
                mts1_snapshot_free(&snap);
            }
        } else if (frame.hdr.type == MTV1_TYPE_END) {
            mtv1_frame_free(&frame);
            break;
        }

        mtv1_frame_free(&frame);
    }

    fclose(f);
    if (csv_out) fclose(csv_out);
    if (jsonl_out) fclose(jsonl_out);

    printf("Export complete: %u snapshots\n", snap_index);
    if (csv_out)
        printf("  CSV: %s\n", opts->csv_path);
    if (jsonl_out)
        printf("  JSONL: %s\n", opts->jsonl_path);

    return 0;
}
