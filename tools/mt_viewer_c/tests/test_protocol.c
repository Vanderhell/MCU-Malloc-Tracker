#include "../include/mtv1_protocol.h"
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
    printf("MTV1 Protocol Test (resync validation)\n");

    /* Load run_corrupt_resync.bin */
    const char* vector_path = MTV1_VECTOR_DIR "run_corrupt_resync.bin";
    FILE* f = fopen(vector_path, "rb");
    if (!f) {
        fprintf(stderr, "Error: failed to open %s\n", vector_path);
        return 1;
    }

    /* Read entire file into memory */
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

    uint32_t valid_frames = 0;
    uint32_t resync_events = 0;
    uint32_t end_frames = 0;

    while (1) {
        mtv1_frame_t frame;
        uint32_t resync_bytes = 0;

        mtv1_parse_result_t result = mtv1_frame_read(mem_byte_source, &byte_ctx,
                                                      &frame, &resync_bytes);

        if (result == MTV1_PARSE_EOF)
            break;

        if (result == MTV1_PARSE_RESYNC) {
            printf("  Resync: skipped %u bytes\n", resync_bytes);
            resync_events++;
        }

        if (result != MTV1_PARSE_OK && result != MTV1_PARSE_RESYNC)
            continue;

        if (result == MTV1_PARSE_OK) {
            valid_frames++;
            printf("  Frame %u: type=%u, payload_len=%u, crc_ok=%d\n",
                   valid_frames, frame.hdr.type, frame.hdr.payload_len, frame.crc_ok);
        }

        if (frame.hdr.type == MTV1_TYPE_END)
            end_frames++;

        mtv1_frame_free(&frame);
    }

    free(data);

    printf("\nResults:\n");
    printf("  Valid frames: %u\n", valid_frames);
    printf("  Resync events: %u\n", resync_events);
    printf("  END frames: %u\n", end_frames);

    int pass = (valid_frames == 2 && resync_events == 1 && end_frames == 1);
    printf("\nTest: %s\n", pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}
