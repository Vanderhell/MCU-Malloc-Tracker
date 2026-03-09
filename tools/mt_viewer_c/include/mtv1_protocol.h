#ifndef MTV1_PROTOCOL_H
#define MTV1_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* MTV1 frame types */
#define MTV1_TYPE_SNAPSHOT_MTS1  1
#define MTV1_TYPE_TELEMETRY_TEXT 2
#define MTV1_TYPE_MARK_TEXT      3
#define MTV1_TYPE_END            4

/* Frame header: 20 bytes, little-endian */
typedef struct {
    uint8_t  magic[4];      /* "MTV1" */
    uint8_t  version;       /* 1 */
    uint8_t  type;          /* MTV1_TYPE_* */
    uint16_t flags;         /* reserved */
    uint32_t seq;           /* monotonic frame counter */
    uint32_t payload_len;   /* payload bytes following header */
    uint32_t crc32;         /* CRC32/IEEE over header (crc32=0) + payload */
} mtv1_frame_hdr_t;         /* Total: 20 bytes */

/* Parsed frame (owns payload memory) */
typedef struct {
    mtv1_frame_hdr_t hdr;
    uint8_t*         payload;    /* malloc'd, NULL for type=END */
    int              crc_ok;     /* 1=verified, 0=failed/missing */
} mtv1_frame_t;

/* Parse result codes */
typedef enum {
    MTV1_PARSE_OK      = 0,
    MTV1_PARSE_EOF     = 1,
    MTV1_PARSE_RESYNC  = 2,   /* resynced past garbage bytes */
    MTV1_PARSE_ERROR   = 3
} mtv1_parse_result_t;

/* Read exactly one MTV1 frame from a byte source.
   On parse error, scans forward byte-by-byte for "MTV1" (resync rule).
   Returns MTV1_PARSE_OK / MTV1_PARSE_EOF / MTV1_PARSE_RESYNC / MTV1_PARSE_ERROR.
   Caller must call mtv1_frame_free() on success. */
mtv1_parse_result_t mtv1_frame_read(
    int (*read_byte)(void* ctx),   /* returns byte or -1 on EOF */
    void*          ctx,
    mtv1_frame_t*  out_frame,
    uint32_t*      out_resync_bytes  /* bytes skipped during resync, may be NULL */
);

/* Verify CRC of a parsed frame.
   CRC covers: all 20 header bytes (crc32 field zeroed) + payload.
   Returns 1 if ok, 0 if mismatch. */
int mtv1_frame_verify_crc(const mtv1_frame_t* f);

/* Free payload allocated by mtv1_frame_read() */
void mtv1_frame_free(mtv1_frame_t* f);

#endif /* MTV1_PROTOCOL_H */
