#include "mtv1_commands.h"
#include "mtv1_protocol.h"
#include "mtv1_decode_mts1.h"
#include "mtv1_model.h"
#include "mtv1_filemap.h"
#include "mtv1_render_tui.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    FILE* f;
} file_byte_ctx_t;

static int replay_byte_source(void* ctx) {
    file_byte_ctx_t* fctx = (file_byte_ctx_t*)ctx;
    int c = fgetc(fctx->f);
    return (c == EOF) ? -1 : c;
}

int mtv1_replay_run(const mtv1_replay_opts_t* opts) {
    if (!opts || !opts->in_path) {
        fprintf(stderr, "Error: replay requires --in\n");
        return 1;
    }

    FILE* f = fopen(opts->in_path, "rb");
    if (!f) {
        fprintf(stderr, "Error: failed to open %s\n", opts->in_path);
        return 1;
    }

    file_byte_ctx_t byte_ctx = {.f = f};

    mtv1_model_t model;
    mtv1_model_init(&model);

    mtv1_filemap_t filemap;
    mtv1_filemap_init(&filemap);
    if (opts->filemap_path)
        mtv1_filemap_load(&filemap, opts->filemap_path);

    printf("Parsing MTV1 stream from %s...\n", opts->in_path);

    uint32_t frame_count = 0;
    while (1) {
        mtv1_frame_t frame;
        uint32_t resync_bytes = 0;

        mtv1_parse_result_t result = mtv1_frame_read(replay_byte_source, &byte_ctx,
                                                      &frame, &resync_bytes);

        if (result == MTV1_PARSE_EOF)
            break;

        if (result == MTV1_PARSE_RESYNC) {
            model.resync_count++;
            printf("  Resync at byte %u (skipped %u bytes)\n",
                   ftell(f) - resync_bytes, resync_bytes);
        }

        if (result != MTV1_PARSE_OK && result != MTV1_PARSE_RESYNC)
            continue;

        frame_count++;

        if (frame.hdr.type == MTV1_TYPE_SNAPSHOT_MTS1) {
            mts1_snapshot_t snap;
            if (mts1_parse(frame.payload, frame.hdr.payload_len, &snap) == 0) {
                snap.crc_ok = mts1_verify_crc(frame.payload, frame.hdr.payload_len, &snap);
                if (!snap.crc_ok)
                    model.crc_fail_count++;

                mtv1_model_push_snapshot(&model, frame.hdr.seq, &snap);
                mts1_snapshot_free(&snap);
            }
        } else if (frame.hdr.type == MTV1_TYPE_TELEMETRY_TEXT) {
            char text[256];
            uint32_t len = frame.hdr.payload_len;
            if (len >= sizeof(text)) len = sizeof(text) - 1;
            memcpy(text, frame.payload, len);
            text[len] = '\0';
            mtv1_model_push_telemetry(&model, frame.hdr.seq, text);
        } else if (frame.hdr.type == MTV1_TYPE_MARK_TEXT) {
            char text[256];
            uint32_t len = frame.hdr.payload_len;
            if (len >= sizeof(text)) len = sizeof(text) - 1;
            memcpy(text, frame.payload, len);
            text[len] = '\0';
            mtv1_model_push_mark(&model, frame.hdr.seq, text);
        } else if (frame.hdr.type == MTV1_TYPE_END) {
            printf("END frame received\n");
            mtv1_frame_free(&frame);
            break;
        }

        mtv1_frame_free(&frame);
    }

    fclose(f);

    printf("Parsed %u frames, %u snapshots, peak used: %u bytes\n",
           frame_count, model.timeline_len, model.peak_used_ever);

    if (opts->no_tui) {
        /* Text-only output */
        printf("\n=== Last Snapshot ===\n");
        if (model.last_snapshot) {
            printf("  Current: %u bytes\n", model.last_snapshot->hdr.current_used);
            printf("  Peak: %u bytes\n", model.last_snapshot->hdr.peak_used);
            printf("  Active allocs: %u\n", model.last_snapshot->hdr.record_count);
        }
    } else {
        /* TUI mode */
        if (tui_platform_init() == 0) {
            tui_state_t state = {
                .current_screen = TUI_SCREEN_OVERVIEW,
                .heatmap_mode = HEATMAP_MODE_SIZE,
                .diff_mode = 0,
                .run_a = &model,
                .run_b = NULL,
                .filemap = &filemap,
                .running = 1
            };

            tui_run(&state);
            tui_platform_cleanup();
        } else {
            fprintf(stderr, "Error: TUI init failed\n");
        }
    }

    mtv1_model_free(&model);
    mtv1_filemap_free(&filemap);

    return 0;
}
