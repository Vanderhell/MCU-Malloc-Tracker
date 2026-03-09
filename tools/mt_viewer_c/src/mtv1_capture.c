#include "mtv1_commands.h"
#include "mtv1_protocol.h"
#include "mtv1_transport.h"
#include "mtv1_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    mtv1_transport_t* t;
    time_t start_time;
    int seconds;
} capture_byte_ctx_t;

static int capture_byte_source(void* ctx) {
    capture_byte_ctx_t* cctx = (capture_byte_ctx_t*)ctx;
    uint8_t buf[1];

    if (cctx->seconds > 0) {
        time_t now = time(NULL);
        if (now - cctx->start_time >= cctx->seconds)
            return -1;
    }

    int n = cctx->t->read(cctx->t, buf, 1, 100);
    return (n > 0) ? buf[0] : -1;
}

int mtv1_capture_run(const mtv1_capture_opts_t* opts) {
    if (!opts || !opts->port || !opts->out_path) {
        fprintf(stderr, "Error: capture requires --port and --out\n");
        return 1;
    }

    printf("Opening %s at %u baud...\n", opts->port, opts->baud);

    mtv1_transport_t* t = NULL;
#if MTV1_PLATFORM_WIN32
    t = mtv1_transport_serial_open_win32(opts->port, opts->baud);
#else
    t = mtv1_transport_serial_open_posix(opts->port, opts->baud);
#endif

    if (!t) {
        fprintf(stderr, "Error: failed to open %s\n", opts->port);
        return 1;
    }

    FILE* out = fopen(opts->out_path, "wb");
    if (!out) {
        fprintf(stderr, "Error: failed to create %s\n", opts->out_path);
        mtv1_transport_close(t);
        return 1;
    }

    printf("Capturing to %s\n", opts->out_path);

    capture_byte_ctx_t byte_ctx = {
        .t = t,
        .start_time = time(NULL),
        .seconds = opts->seconds
    };

    uint32_t total_bytes = 0;
    uint32_t frame_count = 0;

    while (1) {
        mtv1_frame_t frame;
        mtv1_parse_result_t result = mtv1_frame_read(capture_byte_source, &byte_ctx,
                                                      &frame, NULL);

        if (result == MTV1_PARSE_EOF) {
            printf("EOF\n");
            break;
        }

        if (result != MTV1_PARSE_OK)
            continue;

        /* Write frame to output file */
        fwrite(frame.hdr.magic, 1, 4, out);
        fputc(frame.hdr.version, out);
        fputc(frame.hdr.type, out);
        fwrite(&frame.hdr.flags, 1, 2, out);
        fwrite(&frame.hdr.seq, 1, 4, out);
        fwrite(&frame.hdr.payload_len, 1, 4, out);
        fwrite(&frame.hdr.crc32, 1, 4, out);
        if (frame.payload && frame.hdr.payload_len > 0)
            fwrite(frame.payload, 1, frame.hdr.payload_len, out);

        total_bytes += 20 + frame.hdr.payload_len;
        frame_count++;

        if (frame.hdr.type == MTV1_TYPE_END) {
            printf("Received END frame\n");
            break;
        }

        mtv1_frame_free(&frame);

        if (frame_count % 10 == 0)
            printf("  %u frames (%u bytes)\n", frame_count, total_bytes);
    }

    fclose(out);
    mtv1_transport_close(t);

    printf("Capture complete: %u frames, %u bytes\n", frame_count, total_bytes);
    return 0;
}
