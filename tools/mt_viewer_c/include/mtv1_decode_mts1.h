#ifndef MTV1_DECODE_MTS1_H
#define MTV1_DECODE_MTS1_H

#include <stdint.h>
#include <stddef.h>

/* MTS1 snapshot flags */
#define MTS1_FLAG_OVERFLOW  0x0001
#define MTS1_FLAG_FRAG_NA   0x0002
#define MTS1_FLAG_DROPS     0x0004
#define MTS1_FLAG_CRC_OK    0x0008

/* MTS1 Header: 40 bytes, little-endian */
typedef struct {
    char     magic[4];         /* "MTS1" */
    uint16_t version;          /* 1 */
    uint16_t flags;            /* MTS1_FLAG_* */
    uint32_t record_count;     /* number of allocation records */
    uint32_t current_used;     /* bytes currently allocated */
    uint32_t peak_used;        /* peak bytes ever allocated */
    uint32_t total_allocs;     /* cumulative malloc calls */
    uint32_t total_frees;      /* cumulative free calls */
    uint32_t seq;              /* global seq counter at snapshot time */
    uint32_t crc32;            /* CRC32 over header[0:32] + all records */
    /* padding (4 bytes to 40 total, not read) */
} mts1_header_t;

/* MTS1 Record: 24 bytes, little-endian (one per active allocation) */
typedef struct {
    uint64_t ptr;              /* allocation pointer */
    uint32_t size;             /* allocation size in bytes */
    uint32_t file_id;          /* FNV1a-32 hash of __FILE__ */
    uint16_t line;             /* source line number */
    uint8_t  state;            /* always = 1 (USED only) */
    uint8_t  _pad;             /* = 0 */
    uint32_t seq;              /* sequence number (age indicator) */
} mts1_record_t;

/* A fully parsed MTS1 snapshot (owns records array) */
typedef struct {
    mts1_header_t  hdr;
    mts1_record_t* records;    /* malloc'd array of hdr.record_count entries */
    int            crc_ok;     /* 1=verified, 0=failed, -1=not checked */
} mts1_snapshot_t;

/* Parse MTS1 payload bytes into a snapshot struct.
   Returns 0 on success. Allocates snapshot->records (caller must free).
   Returns -1 on error (invalid magic, version, size, etc). */
int mts1_parse(const uint8_t* payload, uint32_t payload_len,
               mts1_snapshot_t* out);

/* Verify CRC of a parsed MTS1 snapshot (recomputes from raw bytes).
   Mirrors the tracker's exact contract: header[0:32] + records, final XOR once.
   Returns 1 if CRC matches, 0 if mismatch. */
int mts1_verify_crc(const uint8_t* payload, uint32_t payload_len,
                    const mts1_snapshot_t* snap);

/* Free records allocated by mts1_parse() */
void mts1_snapshot_free(mts1_snapshot_t* snap);

/* Portable little-endian readers */
uint32_t mts1_read_u32le(const uint8_t* p);
uint64_t mts1_read_u64le(const uint8_t* p);
uint16_t mts1_read_u16le(const uint8_t* p);

#endif /* MTV1_DECODE_MTS1_H */
